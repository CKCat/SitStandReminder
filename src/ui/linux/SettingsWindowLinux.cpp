#include "../SettingsWindow.hpp"
#include "X11App.hpp"
#include "../FloatingWindow.hpp"
#include "../TrayWindow.hpp"
#include "../../graphics/D2DContext.hpp"
#include "../../platform/ThemeManager.hpp"
#include "X11Utils.hpp"
#include <algorithm>
#include <iostream>

#ifndef _WIN32

extern StateMachine* g_pStateMachine;

bool SettingsWindow::Create(void* /*hInstance*/) {
    if (m_window) return true;
    auto& app = X11App::Instance();
    if (!app.Initialize()) return false;

    Display* dpy = app.GetDisplay();
    int screen = app.GetScreen();

    float dpiScale = app.GetDisplayDpiScale();
    m_width = static_cast<int>(std::round(560.0f * dpiScale));
    m_height = static_cast<int>(std::round(660.0f * dpiScale));

    ScreenGeometry primary = app.GetPrimaryScreen();
    int x = primary.x + (primary.width - m_width) / 2;
    int y = primary.y + (primary.height - m_height) / 2;

    Visual* visual = DefaultVisual(dpy, screen);
    int depth = DefaultDepth(dpy, screen);
    Colormap cmap = DefaultColormap(dpy, screen);

    XSetWindowAttributes attrs = {};
    attrs.colormap = cmap;
    attrs.background_pixmap = None;
    attrs.border_pixel = 0;
    attrs.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask | KeyPressMask;

    unsigned long mask = CWColormap | CWBackPixmap | CWBorderPixel | CWEventMask;

    m_window = XCreateWindow(
        dpy, RootWindow(dpy, screen),
        x, y, m_width, m_height, 0,
        depth, InputOutput, visual, mask, &attrs
    );

    if (!m_window) return false;

    // 设置标准 UTF-8 窗口标题、应用程序类与协议
    X11Utils::SetWindowUtf8Title(dpy, m_window, AppConstants::Identity::TITLE_SETTINGS);
    X11Utils::SetWindowClass(dpy, m_window, "sitstandreminder", "SitStandReminder");
    Atom wmDelete = app.GetAtom("WM_DELETE_WINDOW");
    XSetWMProtocols(dpy, m_window, &wmDelete, 1);

    // 注入 EWMH 标准 _NET_WM_ICON 属性，彻底杜绝 GNOME Shell 任务栏与 Dock 栏的未知图标占位
    const int iconDim = 16;
    std::vector<uint32_t> appIcon(iconDim * iconDim, 0xFF4CAF50);
    X11Utils::SetWindowIcon(dpy, m_window, iconDim, iconDim, appIcon.data());

    m_gc = XCreateGC(dpy, m_window, 0, nullptr);
    m_pRenderTarget = std::make_unique<LinuxRenderTarget>(m_width, m_height);

    app.RegisterWindow(m_window, [this](const XEvent& ev) { HandleEvent(ev); });

    LoadConfigToUI();
    return true;
}

void SettingsWindow::Show(void* /*hInstance*/) {
    if (!m_window) {
        Create();
    }
    LoadConfigToUI();
    auto* dpy = X11App::Instance().GetDisplay();
    XMapRaised(dpy, m_window);
    m_visible = true;
    Render();
}

void SettingsWindow::Close() {
    if (m_window) {
        auto* dpy = X11App::Instance().GetDisplay();
        if (dpy) {
            XUnmapWindow(dpy, m_window);
            XFlush(dpy);
        }
        m_visible = false;
    }
}

void SettingsWindow::Destroy() {
    Close();
    auto* dpy = X11App::Instance().GetDisplay();
    if (m_window && dpy) {
        X11App::Instance().UnregisterWindow(m_window);
        if (m_gc) {
            XFreeGC(dpy, m_gc);
            m_gc = nullptr;
        }
        XDestroyWindow(dpy, m_window);
        m_window = 0;
    }
}

void SettingsWindow::OnThemeChanged() {
    if (m_visible) {
        Render();
    }
}

void SettingsWindow::LoadConfigToUI() {
    m_tempConfig = ConfigManager::Instance().GetConfig();
    m_selectedPreset = AppConstants::FindMatchingPresetId(
        m_tempConfig.workMinutes, m_tempConfig.standMinutes, m_tempConfig.restSeconds
    );
}

void SettingsWindow::SaveConfigFromUI() {
    ConfigManager::Instance().SetConfig(m_tempConfig);
    ConfigManager::Instance().Save();

    if (g_pStateMachine) {
        g_pStateMachine->SetConfig(m_tempConfig);
    }
    ThemeManager::Instance().Refresh();
    FloatingWindow::Instance().OnConfigChanged();
    TrayWindow::Instance().RefreshTrayDisplayMode();
}

void SettingsWindow::HandleEvent(const XEvent& ev) {
    switch (ev.type) {
    case Expose:
        Render();
        break;

    case ClientMessage: {
        Atom wmDelete = X11App::Instance().GetAtom("WM_DELETE_WINDOW");
        if (ev.xclient.data.l[0] == static_cast<long>(wmDelete)) {
            Close();
        }
        break;
    }

    case KeyPress: {
        KeySym sym = XLookupKeysym(const_cast<XKeyEvent*>(&ev.xkey), 0);
        if (sym == XK_Escape) {
            Close();
        } else if (sym == XK_Return || sym == XK_KP_Enter) {
            SaveConfigFromUI();
            Close();
        }
        break;
    }

    case ButtonPress: {
        if (ev.xbutton.button == Button1) {
            float dpiScale = X11App::Instance().GetDisplayDpiScale();
            int mx = static_cast<int>(std::round(ev.xbutton.x / dpiScale));
            int my = static_cast<int>(std::round(ev.xbutton.y / dpiScale));

            // 1. 预设按钮区域 (y: 42 ~ 72)
            if (my >= 42 && my <= 72) {
                int btnW = 114;
                for (int i = 0; i < 4; ++i) {
                    int bx = 36 + i * (btnW + 10);
                    if (mx >= bx && mx <= bx + btnW) {
                        auto p = AppConstants::PRESETS[i];
                        m_selectedPreset = p.id;
                        m_tempConfig.workMinutes = p.workMinutes;
                        m_tempConfig.standMinutes = p.standMinutes;
                        m_tempConfig.enableStand = (p.standMinutes > 0);
                        m_tempConfig.restSeconds = p.restSeconds;
                        Render();
                        return;
                    }
                }
            }

            // 2. 自定义时长步进按钮 (y: 92 ~ 172)
            // 坐姿工作 (- / +)
            if (my >= 92 && my <= 116) {
                if (mx >= 210 && mx <= 238) {
                    m_tempConfig.workMinutes = (std::max)(1, m_tempConfig.workMinutes - 5);
                    m_selectedPreset = -1;
                    Render();
                    return;
                } else if (mx >= 294 && mx <= 322) {
                    m_tempConfig.workMinutes = (std::min)(180, m_tempConfig.workMinutes + 5);
                    m_selectedPreset = -1;
                    Render();
                    return;
                }
            }
            // 站立办公 (- / +)
            if (my >= 120 && my <= 144) {
                if (mx >= 210 && mx <= 238) {
                    m_tempConfig.standMinutes = (std::max)(0, m_tempConfig.standMinutes - 5);
                    m_tempConfig.enableStand = (m_tempConfig.standMinutes > 0);
                    m_selectedPreset = -1;
                    Render();
                    return;
                } else if (mx >= 294 && mx <= 322) {
                    m_tempConfig.standMinutes = (std::min)(90, m_tempConfig.standMinutes + 5);
                    m_tempConfig.enableStand = (m_tempConfig.standMinutes > 0);
                    m_selectedPreset = -1;
                    Render();
                    return;
                }
            }
            // 工间休息 (- / +)
            if (my >= 148 && my <= 172) {
                if (mx >= 210 && mx <= 238) {
                    m_tempConfig.restSeconds = (std::max)(5, m_tempConfig.restSeconds - 10);
                    m_selectedPreset = -1;
                    Render();
                    return;
                } else if (mx >= 294 && mx <= 322) {
                    m_tempConfig.restSeconds = (std::min)(300, m_tempConfig.restSeconds + 10);
                    m_selectedPreset = -1;
                    Render();
                    return;
                }
            }

            // 3. Card 2 药丸组 (y: 224 ~ 400)
            // 行 1: 工间操模式 (y: 224 ~ 254)
            if (my >= 224 && my <= 254) {
                int pillW = 114;
                for (int i = 0; i < 4; ++i) {
                    int px = 36 + i * (pillW + 10);
                    if (mx >= px && mx <= px + pillW) {
                        m_tempConfig.exerciseMode = static_cast<ExerciseMode>(i);
                        Render();
                        return;
                    }
                }
            }

            // 行 2: 伴侣形象 (y: 260 ~ 290)
            if (my >= 260 && my <= 290) {
                int pillW = 114;
                for (int i = 0; i < 4; ++i) {
                    int px = 36 + i * (pillW + 10);
                    if (mx >= px && mx <= px + pillW) {
                        m_tempConfig.mascotTheme = static_cast<MascotTheme>(i);
                        Render();
                        return;
                    }
                }
            }

            // 行 3: 托盘图标风格 (y: 296 ~ 326)
            if (my >= 296 && my <= 326) {
                int pillW = 114;
                for (int i = 0; i < 4; ++i) {
                    int px = 36 + i * (pillW + 10);
                    if (mx >= px && mx <= px + pillW) {
                        m_tempConfig.trayDisplayMode = static_cast<TrayDisplayMode>(i);
                        ConfigManager::Instance().SetConfig(m_tempConfig);
                        TrayWindow::Instance().RefreshTrayDisplayMode();
                        Render();
                        return;
                    }
                }
            }

            // 行 4: 外边框厚度 (y: 332 ~ 362)
            if (my >= 332 && my <= 362) {
                int pillW = 114;
                for (int i = 0; i < 4; ++i) {
                    int px = 36 + i * (pillW + 10);
                    if (mx >= px && mx <= px + pillW) {
                        m_tempConfig.borderWidth = static_cast<BorderWidth>(i);
                        Render();
                        return;
                    }
                }
            }

            // 行 5: 主题外观 (y: 368 ~ 398)
            if (my >= 368 && my <= 398) {
                int pillW = 154;
                for (int i = 0; i < 3; ++i) {
                    int px = 36 + i * (pillW + 13);
                    if (mx >= px && mx <= px + pillW) {
                        m_tempConfig.themeMode = static_cast<ThemeMode>(i);
                        Render();
                        return;
                    }
                }
            }

            // 4. Card 3 选项复选框与快捷方式按钮点击 (y: 450 ~ 570)
            // 左列 (x: 36 ~ 265)
            if (mx >= 36 && mx <= 265) {
                if (my >= 454 && my <= 476) {
                    m_tempConfig.enableStand = !m_tempConfig.enableStand;
                    Render();
                    return;
                }
                if (my >= 482 && my <= 504) {
                    m_tempConfig.alwaysTopMost = !m_tempConfig.alwaysTopMost;
                    Render();
                    return;
                }
                if (my >= 510 && my <= 532) {
                    m_tempConfig.enableSound = !m_tempConfig.enableSound;
                    Render();
                    return;
                }
                if (my >= 538 && my <= 560) {
                    m_tempConfig.autoStart = !m_tempConfig.autoStart;
                    Render();
                    return;
                }
            }
            // 右列 (x: 275 ~ 524)
            if (mx >= 275 && mx <= 524) {
                if (my >= 454 && my <= 476) {
                    m_tempConfig.blockInput = !m_tempConfig.blockInput;
                    Render();
                    return;
                }
                if (my >= 482 && my <= 504) {
                    m_tempConfig.strongReminder = !m_tempConfig.strongReminder;
                    Render();
                    return;
                }
                if (my >= 510 && my <= 532) {
                    m_tempConfig.enableEdgeDock = !m_tempConfig.enableEdgeDock;
                    Render();
                    return;
                }
                if (my >= 536 && my <= 564) {
                    ConfigManager::Instance().InstallDesktopShortcuts(true, true);
                    m_shortcutFeedback = true;
                    Render();
                    return;
                }
            }

            // 5. 底部操作按钮 (y: 604 ~ 646)
            // [ 保存并应用 ] (x: 24 ~ 230)
            if (my >= 604 && my <= 646 && mx >= 24 && mx <= 230) {
                SaveConfigFromUI();
                Close();
                return;
            }
            // [ 恢复出厂设置 ] (x: 244 ~ 380)
            if (my >= 604 && my <= 646 && mx >= 244 && mx <= 380) {
                ConfigManager::Instance().ClearConfig();
                LoadConfigToUI();
                Render();
                return;
            }
            // [ 关闭 ] (x: 394 ~ 536)
            if (my >= 604 && my <= 646 && mx >= 394 && mx <= 536) {
                Close();
                return;
            }
        }
        break;
    }
    }
}

void SettingsWindow::Render() {
    if (!m_window || !m_pRenderTarget) return;
    auto* dpy = X11App::Instance().GetDisplay();
    if (!dpy) return;

    const auto& colors = ThemeManager::Instance().GetColors();

    float dpiScale = X11App::Instance().GetDisplayDpiScale();
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Scale(dpiScale, dpiScale));

    // 填充底色 (极高质感的现代背景色)
    m_pRenderTarget->Clear(colors.background);

    LinuxBrush textBrush(colors.textPrimary);
    LinuxBrush textSecBrush(colors.textSecondary);
    LinuxBrush accentBrush(colors.forestGreen);
    LinuxBrush cardBgBrush(colors.cardBackground);
    LinuxBrush borderBrush(colors.cardBorder);

    auto fmtHeader = D2DContext::Instance().GetCachedTextFormat(L"sans-serif", 12.0f, DWRITE_FONT_WEIGHT_BOLD);

    // ---------------- Card 1: 周期预设与自定义时长卡片 ----------------
    D2D1_ROUNDED_RECT card1 = D2D1::RoundedRect(D2D1::RectF(24, 14, 536, 182), 10.0f, 10.0f);
    m_pRenderTarget->FillRoundedRectangle(card1, &cardBgBrush);
    m_pRenderTarget->DrawRoundedRectangle(card1, &borderBrush, 1.0f);

    const wchar_t* c1Title = L"办公周期与自定义时长";
    m_pRenderTarget->DrawText(c1Title, wcslen(c1Title), fmtHeader.Get(), D2D1::RectF(36, 22, 400, 38), &textSecBrush);

    // 预设按钮组
    int btnW = 114;
    for (int i = 0; i < 4; ++i) {
        int bx = 36 + i * (btnW + 10);
        bool isSel = (m_selectedPreset == AppConstants::PRESETS[i].id);
        D2D1_ROUNDED_RECT rRect = D2D1::RoundedRect(D2D1::RectF(bx, 42, bx + btnW, 72), 6.0f, 6.0f);

        if (isSel) {
            m_pRenderTarget->FillRoundedRectangle(rRect, &accentBrush);
        } else {
            LinuxBrush itemBg(D2D1::ColorF(colors.background.r, colors.background.g, colors.background.b, 0.7f));
            m_pRenderTarget->FillRoundedRectangle(rRect, &itemBg);
            m_pRenderTarget->DrawRoundedRectangle(rRect, &borderBrush, 1.0f);
        }

        auto fmtBtn = D2DContext::Instance().GetCachedTextFormat(
            L"sans-serif", 11.0f, isSel ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER
        );
        const auto* lbl = AppConstants::PRESETS[i].buttonLabel;
        LinuxBrush whiteBtnBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f));
        m_pRenderTarget->DrawText(lbl, wcslen(lbl), fmtBtn.Get(), D2D1::RectF(bx, 42, bx + btnW, 72), isSel ? &whiteBtnBrush : &textBrush);
    }

    // 分割线
    m_pRenderTarget->DrawLine(D2D1::Point2F(36, 82), D2D1::Point2F(524, 82), &borderBrush, 1.0f);

    // 3 行精准矢量居中 Stepper
    auto drawStepper = [&](int y, const wchar_t* title, int value, const wchar_t* unit) {
        auto fmtLabel = D2DContext::Instance().GetCachedTextFormat(
            L"sans-serif", 12.0f, DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER
        );
        m_pRenderTarget->DrawText(title, wcslen(title), fmtLabel.Get(), D2D1::RectF(36, y, 200, y + 22), &textBrush);

        // [ - ] 按钮
        D2D1_ROUNDED_RECT rMinus = D2D1::RoundedRect(D2D1::RectF(210, y, 238, y + 22), 4.0f, 4.0f);
        LinuxBrush itemBg(D2D1::ColorF(colors.background.r, colors.background.g, colors.background.b, 0.85f));
        m_pRenderTarget->FillRoundedRectangle(rMinus, &itemBg);
        m_pRenderTarget->DrawRoundedRectangle(rMinus, &borderBrush, 1.0f);
        // 矢量居中水平横线
        float cxMinus = 224.0f;
        float cyMinus = y + 11.0f;
        m_pRenderTarget->DrawLine(D2D1::Point2F(cxMinus - 4.5f, cyMinus), D2D1::Point2F(cxMinus + 4.5f, cyMinus), &textBrush, 1.8f);

        // 数字文本
        wchar_t vbuf[32];
        swprintf(vbuf, 32, L"%d", value);
        auto fmtNum = D2DContext::Instance().GetCachedTextFormat(
            L"sans-serif", 13.0f, DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER
        );
        m_pRenderTarget->DrawText(vbuf, wcslen(vbuf), fmtNum.Get(), D2D1::RectF(242, y, 290, y + 22), &accentBrush);

        // [ + ] 按钮
        D2D1_ROUNDED_RECT rPlus = D2D1::RoundedRect(D2D1::RectF(294, y, 322, y + 22), 4.0f, 4.0f);
        m_pRenderTarget->FillRoundedRectangle(rPlus, &itemBg);
        m_pRenderTarget->DrawRoundedRectangle(rPlus, &borderBrush, 1.0f);
        // 矢量居中十字加号
        float cxPlus = 308.0f;
        float cyPlus = y + 11.0f;
        m_pRenderTarget->DrawLine(D2D1::Point2F(cxPlus - 4.5f, cyPlus), D2D1::Point2F(cxPlus + 4.5f, cyPlus), &textBrush, 1.8f);
        m_pRenderTarget->DrawLine(D2D1::Point2F(cxPlus, cyPlus - 4.5f), D2D1::Point2F(cxPlus, cyPlus + 4.5f), &textBrush, 1.8f);

        // 单位文本
        m_pRenderTarget->DrawText(unit, wcslen(unit), fmtLabel.Get(), D2D1::RectF(332, y, 400, y + 22), &textSecBrush);
    };

    drawStepper(92,  L"坐姿工作时长：", m_tempConfig.workMinutes, L"分钟");
    drawStepper(120, L"站立办公时长：", m_tempConfig.standMinutes, L"分钟");
    drawStepper(148, L"工间操休息：",   m_tempConfig.restSeconds, L"秒");

    // ---------------- Card 2: 个性化偏好与外观风格卡片 ----------------
    D2D1_ROUNDED_RECT card2 = D2D1::RoundedRect(D2D1::RectF(24, 194, 536, 410), 10.0f, 10.0f);
    m_pRenderTarget->FillRoundedRectangle(card2, &cardBgBrush);
    m_pRenderTarget->DrawRoundedRectangle(card2, &borderBrush, 1.0f);

    const wchar_t* c2Title = L"个性化偏好与外观风格";
    m_pRenderTarget->DrawText(c2Title, wcslen(c2Title), fmtHeader.Get(), D2D1::RectF(36, 202, 400, 218), &textSecBrush);

    // 通用 4 药丸渲染 helper
    auto drawPills4 = [&](int y, const wchar_t* names[4], int selectedIdx) {
        int pillW = 114;
        for (int i = 0; i < 4; ++i) {
            int px = 36 + i * (pillW + 10);
            bool isSel = (selectedIdx == i);
            D2D1_ROUNDED_RECT rPill = D2D1::RoundedRect(D2D1::RectF(px, y, px + pillW, y + 28), 6.0f, 6.0f);
            if (isSel) {
                m_pRenderTarget->FillRoundedRectangle(rPill, &accentBrush);
                LinuxBrush whiteBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
                auto fmtPill = D2DContext::Instance().GetCachedTextFormat(
                    L"sans-serif", 11.0f, DWRITE_FONT_WEIGHT_BOLD,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER
                );
                m_pRenderTarget->DrawText(names[i], wcslen(names[i]), fmtPill.Get(), D2D1::RectF(px, y, px + pillW, y + 28), &whiteBrush);
            } else {
                LinuxBrush itemBg(D2D1::ColorF(colors.background.r, colors.background.g, colors.background.b, 0.7f));
                m_pRenderTarget->FillRoundedRectangle(rPill, &itemBg);
                m_pRenderTarget->DrawRoundedRectangle(rPill, &borderBrush, 1.0f);
                auto fmtPill = D2DContext::Instance().GetCachedTextFormat(
                    L"sans-serif", 11.0f, DWRITE_FONT_WEIGHT_REGULAR,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER
                );
                m_pRenderTarget->DrawText(names[i], wcslen(names[i]), fmtPill.Get(), D2D1::RectF(px, y, px + pillW, y + 28), &textBrush);
            }
        }
    };

    // 行 1: 工间操模式 (4 药丸)
    const wchar_t* modeNames[] = { L"综合保养操", L"科学颈椎操", L"护眼远眺操", L"极简放空" };
    drawPills4(224, modeNames, static_cast<int>(m_tempConfig.exerciseMode));

    // 行 2: 伴侣形象 (4 药丸)
    const wchar_t* mascotNames[] = { L"极简商务", L"佛系水豚", L"灵动像素猫", L"赛博小助手" };
    drawPills4(260, mascotNames, static_cast<int>(m_tempConfig.mascotTheme));

    // 行 3: 托盘风格 (4 药丸)
    const wchar_t* trayNames[] = { L"经典图标", L"动态倒计时", L"感应猫RunCat", L"CPU动力猫" };
    drawPills4(296, trayNames, static_cast<int>(m_tempConfig.trayDisplayMode));

    // 行 4: 外边框厚度 (4 药丸)
    const wchar_t* borderNames[] = { L"细线 1.5px", L"标准 2.5px", L"加粗 3.5px", L"极粗 4.5px" };
    drawPills4(332, borderNames, static_cast<int>(m_tempConfig.borderWidth));

    // 行 5: 主题外观 (3 药丸)
    const wchar_t* themeNames[] = { L"跟随系统 (Auto)", L"明亮浅色 (Light)", L"工效深色 (Dark)" };
    int themeW = 154;
    for (int i = 0; i < 3; ++i) {
        int px = 36 + i * (themeW + 13);
        bool isSel = (static_cast<int>(m_tempConfig.themeMode) == i);
        D2D1_ROUNDED_RECT rPill = D2D1::RoundedRect(D2D1::RectF(px, 368, px + themeW, 396), 6.0f, 6.0f);
        if (isSel) {
            m_pRenderTarget->FillRoundedRectangle(rPill, &accentBrush);
            LinuxBrush whiteBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
            auto fmtPill = D2DContext::Instance().GetCachedTextFormat(
                L"sans-serif", 11.0f, DWRITE_FONT_WEIGHT_BOLD,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER
            );
            m_pRenderTarget->DrawText(themeNames[i], wcslen(themeNames[i]), fmtPill.Get(), D2D1::RectF(px, 368, px + themeW, 396), &whiteBrush);
        } else {
            LinuxBrush itemBg(D2D1::ColorF(colors.background.r, colors.background.g, colors.background.b, 0.7f));
            m_pRenderTarget->FillRoundedRectangle(rPill, &itemBg);
            m_pRenderTarget->DrawRoundedRectangle(rPill, &borderBrush, 1.0f);
            auto fmtPill = D2DContext::Instance().GetCachedTextFormat(
                L"sans-serif", 11.0f, DWRITE_FONT_WEIGHT_REGULAR,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER
            );
            m_pRenderTarget->DrawText(themeNames[i], wcslen(themeNames[i]), fmtPill.Get(), D2D1::RectF(px, 368, px + themeW, 396), &textBrush);
        }
    }

    // ---------------- Card 3: 通用系统偏好卡片 ----------------
    D2D1_ROUNDED_RECT card3 = D2D1::RoundedRect(D2D1::RectF(24, 422, 536, 592), 10.0f, 10.0f);
    m_pRenderTarget->FillRoundedRectangle(card3, &cardBgBrush);
    m_pRenderTarget->DrawRoundedRectangle(card3, &borderBrush, 1.0f);

    const wchar_t* c3Title = L"系统与通用选项";
    m_pRenderTarget->DrawText(c3Title, wcslen(c3Title), fmtHeader.Get(), D2D1::RectF(36, 430, 400, 446), &textSecBrush);

    auto drawCheckAt = [&](int x, int y, const wchar_t* text, bool val, int maxTextW = 200) {
        D2D1_ROUNDED_RECT box = D2D1::RoundedRect(D2D1::RectF(x, y, x + 18, y + 18), 4.0f, 4.0f);
        if (val) {
            m_pRenderTarget->FillRoundedRectangle(box, &accentBrush);
            LinuxBrush whiteCheckBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f));
            // 矢量精致抗锯齿两段折线勾选
            m_pRenderTarget->DrawLine(D2D1::Point2F(x + 4.5f, y + 9.5f), D2D1::Point2F(x + 7.5f, y + 13.0f), &whiteCheckBrush, 2.0f);
            m_pRenderTarget->DrawLine(D2D1::Point2F(x + 7.5f, y + 13.0f), D2D1::Point2F(x + 13.5f, y + 5.5f), &whiteCheckBrush, 2.0f);
        } else {
            LinuxBrush itemBg(D2D1::ColorF(colors.background.r, colors.background.g, colors.background.b, 0.85f));
            m_pRenderTarget->FillRoundedRectangle(box, &itemBg);
            m_pRenderTarget->DrawRoundedRectangle(box, &borderBrush, 1.2f);
        }
        auto fmtChk = D2DContext::Instance().GetCachedTextFormat(
            L"sans-serif", 11.5f, DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER
        );
        m_pRenderTarget->DrawText(text, wcslen(text), fmtChk.Get(), D2D1::RectF(x + 26, y - 1, x + 26 + maxTextW, y + 19), &textBrush);
    };

    // 左列 4 项 (x=36, y=454, 482, 510, 538)
    drawCheckAt(36, 454, L"启用站立工作循环", m_tempConfig.enableStand, 190);
    drawCheckAt(36, 482, L"悬浮窗始终置顶", m_tempConfig.alwaysTopMost, 190);
    drawCheckAt(36, 510, L"阶段切换提示音", m_tempConfig.enableSound, 190);
    drawCheckAt(36, 538, L"登录桌面开机自启", m_tempConfig.autoStart, 190);

    // 右列 3 项 + 快捷方式按钮 (x=280, y=454, 482, 510, 536)
    drawCheckAt(280, 454, L"全屏拦截非必要输入", m_tempConfig.blockInput, 210);
    drawCheckAt(280, 482, L"临界 30 秒红光强提醒", m_tempConfig.strongReminder, 210);
    drawCheckAt(280, 510, L"边缘吸附与推入折叠", m_tempConfig.enableEdgeDock, 210);

    // [ 生成桌面快捷方式 ] 胶囊交互按钮 (x: 280 ~ 524, y: 536 ~ 562)
    D2D1_ROUNDED_RECT rBtnShortcut = D2D1::RoundedRect(D2D1::RectF(280, 536, 524, 562), 6.0f, 6.0f);
    auto fmtBtn = D2DContext::Instance().GetCachedTextFormat(
        L"sans-serif", 11.0f, DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER
    );

    if (m_shortcutFeedback) {
        LinuxBrush successBg(D2D1::ColorF(0.20f, 0.75f, 0.45f, 0.25f));
        LinuxBrush successBorder(D2D1::ColorF(0.20f, 0.85f, 0.45f, 0.75f));
        LinuxBrush successText(D2D1::ColorF(0.35f, 0.95f, 0.65f));
        m_pRenderTarget->FillRoundedRectangle(rBtnShortcut, &successBg);
        m_pRenderTarget->DrawRoundedRectangle(rBtnShortcut, &successBorder, 1.0f);
        const wchar_t* btnText = L"✓ 桌面与菜单已创建";
        m_pRenderTarget->DrawText(btnText, wcslen(btnText), fmtBtn.Get(), D2D1::RectF(280, 536, 524, 562), &successText);
    } else {
        m_pRenderTarget->FillRoundedRectangle(rBtnShortcut, &cardBgBrush);
        m_pRenderTarget->DrawRoundedRectangle(rBtnShortcut, &borderBrush, 1.0f);
        const wchar_t* btnText = L"生成桌面快捷方式";
        m_pRenderTarget->DrawText(btnText, wcslen(btnText), fmtBtn.Get(), D2D1::RectF(280, 536, 524, 562), &accentBrush);
    }

    // ---------------- 底部操作按钮栏 ----------------
    // [ 保存并应用配置 ]
    D2D1_ROUNDED_RECT rSave = D2D1::RoundedRect(D2D1::RectF(24, 604, 230, 646), 8.0f, 8.0f);
    m_pRenderTarget->FillRoundedRectangle(rSave, &accentBrush);
    auto fmtAction = D2DContext::Instance().GetCachedTextFormat(
        L"sans-serif", 13.0f, DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER
    );
    LinuxBrush whiteBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f));
    const wchar_t* saveText = L"保存并应用配置";
    m_pRenderTarget->DrawText(saveText, wcslen(saveText), fmtAction.Get(), D2D1::RectF(24, 604, 230, 646), &whiteBrush);

    // [ 恢复出厂设置 ]
    D2D1_ROUNDED_RECT rClean = D2D1::RoundedRect(D2D1::RectF(244, 604, 380, 646), 8.0f, 8.0f);
    m_pRenderTarget->FillRoundedRectangle(rClean, &cardBgBrush);
    m_pRenderTarget->DrawRoundedRectangle(rClean, &borderBrush, 1.0f);
    LinuxBrush redBrush(colors.alertRed);
    const wchar_t* cleanText = L"恢复出厂设置";
    m_pRenderTarget->DrawText(cleanText, wcslen(cleanText), fmtAction.Get(), D2D1::RectF(244, 604, 380, 646), &redBrush);

    // [ 关闭 ]
    D2D1_ROUNDED_RECT rClose = D2D1::RoundedRect(D2D1::RectF(394, 604, 536, 646), 8.0f, 8.0f);
    m_pRenderTarget->FillRoundedRectangle(rClose, &cardBgBrush);
    m_pRenderTarget->DrawRoundedRectangle(rClose, &borderBrush, 1.0f);
    const wchar_t* closeText = L"关闭";
    m_pRenderTarget->DrawText(closeText, wcslen(closeText), fmtAction.Get(), D2D1::RectF(394, 604, 536, 646), &textBrush);

    // 恢复变换矩阵
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    // 提交到 X11
    Visual* visual = DefaultVisual(dpy, DefaultScreen(dpy));
    int depth = DefaultDepth(dpy, DefaultScreen(dpy));

    XImage* ximage = XCreateImage(
        dpy, visual, depth, ZPixmap, 0,
        reinterpret_cast<char*>(m_pRenderTarget->GetPixels()),
        m_width, m_height, 32, 0
    );

    if (ximage) {
        XPutImage(dpy, m_window, m_gc, ximage, 0, 0, 0, 0, m_width, m_height);
        XFlush(dpy);
        ximage->data = nullptr;
        XDestroyImage(ximage);
    }

    const char* dumpPath = getenv("DUMP_SETTINGS_BMP");
    if (dumpPath) {
        static bool s_dumped = false;
        if (!s_dumped) {
            s_dumped = true;
            uint32_t imageSize = m_width * 4 * m_height;
            uint32_t fileSize = 54 + imageSize;
            uint8_t header[54] = {
                'B', 'M',
                (uint8_t)(fileSize), (uint8_t)(fileSize >> 8), (uint8_t)(fileSize >> 16), (uint8_t)(fileSize >> 24),
                0, 0, 0, 0,
                54, 0, 0, 0,
                40, 0, 0, 0,
                (uint8_t)(m_width), (uint8_t)(m_width >> 8), (uint8_t)(m_width >> 16), (uint8_t)(m_width >> 24),
                (uint8_t)(-m_height), (uint8_t)((-m_height) >> 8), (uint8_t)((-m_height) >> 16), (uint8_t)((-m_height) >> 24),
                1, 0,
                32, 0,
                0, 0, 0, 0,
                (uint8_t)(imageSize), (uint8_t)(imageSize >> 8), (uint8_t)(imageSize >> 16), (uint8_t)(imageSize >> 24),
                0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0
            };
            if (FILE* f = fopen(dumpPath, "wb")) {
                fwrite(header, 1, 54, f);
                fwrite(m_pRenderTarget->GetPixels(), 4, m_width * m_height, f);
                fclose(f);
            }
        }
    }
}

void SettingsWindow::SimulateClick(int x, int y) {
    float dpiScale = X11App::Instance().GetDisplayDpiScale();
    XEvent ev{};
    ev.type = ButtonPress;
    ev.xbutton.button = Button1;
    ev.xbutton.x = static_cast<int>(std::round(x * dpiScale));
    ev.xbutton.y = static_cast<int>(std::round(y * dpiScale));
    HandleEvent(ev);
}

#endif
