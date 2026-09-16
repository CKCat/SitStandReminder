#include "../FloatingWindow.hpp"
#include "X11App.hpp"
#include "../SettingsWindow.hpp"
#include "../TrayWindow.hpp"
#include "../../graphics/MascotRenderer.hpp"
#include "../../graphics/D2DContext.hpp"
#include "../../platform/ThemeManager.hpp"
#include "X11Utils.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#ifndef _WIN32

struct MotifWmHints {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
};

bool FloatingWindow::Create(void* /*hInstance*/) {
    auto& app = X11App::Instance();
    if (!app.Initialize()) return false;

    Display* dpy = app.GetDisplay();
    int screen = app.GetScreen();
    auto primary = app.GetPrimaryScreen();

    float dpiScale = app.GetDisplayDpiScale();
    m_width = static_cast<int>(std::round(AppConstants::FloatingWindowDimensions::BASE_WIDTH * dpiScale));
    m_height = static_cast<int>(std::round(AppConstants::FloatingWindowDimensions::BASE_HEIGHT * dpiScale));

    // 默认定位在主显示器右上角舒适工作区
    m_x = primary.x + primary.width - m_width - static_cast<int>(std::round(32.0f * dpiScale));
    m_y = primary.y + static_cast<int>(std::round(72.0f * dpiScale));

    Visual* visual = app.GetVisual32();
    int depth = app.GetDepth32();
    Colormap cmap = app.GetColormap32();

    XSetWindowAttributes attrs = {};
    attrs.colormap = cmap;
    attrs.background_pixmap = None;
    attrs.border_pixel = 0;
    attrs.override_redirect = False; // 由窗口管理器托管，确保 EWMH 规范置顶、跨工作区粘附生效
    attrs.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask |
                       ButtonReleaseMask | PointerMotionMask | EnterWindowMask | LeaveWindowMask;

    unsigned long mask = CWColormap | CWBackPixmap | CWBorderPixel | CWEventMask | CWOverrideRedirect;

    m_window = XCreateWindow(
        dpy, RootWindow(dpy, screen),
        m_x, m_y, m_width, m_height, 0,
        depth, InputOutput, visual, mask, &attrs
    );

    if (!m_window) return false;

    // 注入 Motif WM Hints 消除窗口边框与标题栏
    MotifWmHints hints = {};
    hints.flags = 2; // MWM_HINTS_DECORATIONS
    hints.decorations = 0; // 无边框
    Atom motifHints = app.GetAtom("_MOTIF_WM_HINTS");
    XChangeProperty(dpy, m_window, motifHints, motifHints, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&hints), 5);

    // 注入现代窗口管理器属性 (Utility, Above, Sticky, SkipTaskbar, SkipPager)
    Atom wmType = app.GetAtom("_NET_WM_WINDOW_TYPE");
    Atom wmTypeUtility = app.GetAtom("_NET_WM_WINDOW_TYPE_UTILITY");
    XChangeProperty(dpy, m_window, wmType, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&wmTypeUtility), 1);

    Atom wmState = app.GetAtom("_NET_WM_STATE");
    Atom stateAbove = app.GetAtom("_NET_WM_STATE_ABOVE");
    Atom stateSticky = app.GetAtom("_NET_WM_STATE_STICKY");
    Atom stateSkipTaskbar = app.GetAtom("_NET_WM_STATE_SKIP_TASKBAR");
    Atom stateSkipPager = app.GetAtom("_NET_WM_STATE_SKIP_PAGER");

    m_isTopMost = ConfigManager::Instance().GetConfig().alwaysTopMost;
    std::vector<Atom> states;
    if (m_isTopMost) {
        states.push_back(stateAbove);
    }
    states.push_back(stateSticky);
    states.push_back(stateSkipTaskbar);
    states.push_back(stateSkipPager);

    XChangeProperty(dpy, m_window, wmState, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(states.data()), static_cast<int>(states.size()));

    X11Utils::SetWindowUtf8Title(dpy, m_window, "坐立提醒 · 悬浮窗");
    X11Utils::SetWindowClass(dpy, m_window, "sitstandreminder", "SitStandReminder");

    const int iconDim = 16;
    std::vector<uint32_t> appIcon(iconDim * iconDim, 0xFF4CAF50);
    X11Utils::SetWindowIcon(dpy, m_window, iconDim, iconDim, appIcon.data());

    m_gc = XCreateGC(dpy, m_window, 0, nullptr);
    m_pRenderTarget = std::make_unique<LinuxRenderTarget>(m_width, m_height);

    // 注册到应用事件与 60FPS 动画循环
    app.RegisterWindow(m_window, [this](const XEvent& ev) { HandleEvent(ev); });
    app.AddAnimationCallback(this, [this]() { OnAnimationTick(); });

    return true;
}

void FloatingWindow::Destroy() {
    if (m_window) {
        auto& app = X11App::Instance();
        if (m_isDragging) {
            m_isDragging = false;
            XUngrabPointer(app.GetDisplay(), CurrentTime);
        }
        app.RemoveAnimationCallback(this);
        app.UnregisterWindow(m_window);
        if (m_gc) {
            XFreeGC(app.GetDisplay(), m_gc);
            m_gc = nullptr;
        }
        XDestroyWindow(app.GetDisplay(), m_window);
        m_window = 0;
        m_hasLastClick = false;
    }
}

void FloatingWindow::Show(bool show) {
    if (!m_window) return;
    auto* dpy = X11App::Instance().GetDisplay();
    if (show) {
        XMapRaised(dpy, m_window);
        m_visible = true;
        Render();
    } else {
        if (m_isDragging) {
            m_isDragging = false;
            XUngrabPointer(dpy, CurrentTime);
        }
        XUnmapWindow(dpy, m_window);
        m_visible = false;
        m_hasLastClick = false;
    }
}

void FloatingWindow::UpdateState(AppState state, int remainingSec, int totalSec) {
    m_state = state;
    m_remainingSeconds = remainingSec;
    m_totalSeconds = totalSec;
    m_animTick++;
    if (m_visible) {
        Render();
    }
}

void FloatingWindow::RepositionDefault() {
    auto* dpy = X11App::Instance().GetDisplay();
    if (!dpy || !m_window) return;
    auto primary = X11App::Instance().GetPrimaryScreen();
    m_x = primary.x + primary.width - m_width - 32;
    m_y = primary.y + 72;
    m_dockState = DockState::Floating;
    XMoveWindow(dpy, m_window, m_x, m_y);
    Render();
}

void FloatingWindow::ClampToWorkArea() {
    auto* dpy = X11App::Instance().GetDisplay();
    if (!dpy || !m_window) return;
    auto screen = X11App::Instance().GetScreenForPoint(m_x + m_width / 2, m_y + m_height / 2);

    m_x = (std::clamp)(m_x, screen.x, screen.x + screen.width - m_width);
    m_y = (std::clamp)(m_y, screen.y, screen.y + screen.height - m_height);
    XMoveWindow(dpy, m_window, m_x, m_y);
}

void FloatingWindow::SetTopMost(bool topMost) {
    m_isTopMost = topMost;
    if (!m_window) return;
    auto* dpy = X11App::Instance().GetDisplay();
    if (!dpy) return;

    if (topMost) {
        XRaiseWindow(dpy, m_window);
    }

    Atom wmState = X11App::Instance().GetAtom("_NET_WM_STATE");
    Atom stateAbove = X11App::Instance().GetAtom("_NET_WM_STATE_ABOVE");

    XEvent xev = {};
    xev.type = ClientMessage;
    xev.xclient.window = m_window;
    xev.xclient.message_type = wmState;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = topMost ? 1 : 0; // 1 = _NET_WM_STATE_ADD, 0 = _NET_WM_STATE_REMOVE
    xev.xclient.data.l[1] = static_cast<long>(stateAbove);
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 1; // 1: normal application
    xev.xclient.data.l[4] = 0;

    XSendEvent(dpy, DefaultRootWindow(dpy), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XFlush(dpy);
}

void FloatingWindow::OnConfigChanged() {
    bool wantTopMost = ConfigManager::Instance().GetConfig().alwaysTopMost;
    if (m_isTopMost != wantTopMost) {
        SetTopMost(wantTopMost);
    }
    Render();
}

void FloatingWindow::OnThemeChanged() {
    Render();
}

void FloatingWindow::StopAnimation() {
    m_isAnimating = false;
}

void FloatingWindow::Render() {
    if (!m_window || !m_pRenderTarget) return;
    auto* dpy = X11App::Instance().GetDisplay();
    if (!dpy) return;

    float dpiScale = X11App::Instance().GetDisplayDpiScale();
    int baseW = AppConstants::FloatingWindowDimensions::BASE_WIDTH;
    int baseH = AppConstants::FloatingWindowDimensions::BASE_HEIGHT;
    int tabW = AppConstants::FloatingWindowDimensions::DOCK_TAB_WIDTH;

    bool isCollapsed = (m_dockState == DockState::DockedLeft_Collapsed ||
                        m_dockState == DockState::DockedRight_Collapsed ||
                        m_dockState == DockState::DockedTop_Collapsed);

    float logW = static_cast<float>(baseW);
    float logH = static_cast<float>(baseH);
    if (isCollapsed) {
        if (m_dockState == DockState::DockedTop_Collapsed) {
            logW = static_cast<float>(baseW);
            logH = static_cast<float>(tabW);
        } else {
            logW = static_cast<float>(tabW);
            logH = static_cast<float>(baseH);
        }
    }

    int curW = static_cast<int>(std::round(logW * dpiScale));
    int curH = static_cast<int>(std::round(logH * dpiScale));

    // 折叠状态下的边缘锚定校准，杜绝任何窗口管理器挤压位移
    auto screen = X11App::Instance().GetScreenForPoint(m_x + curW / 2, m_y + curH / 2);
    if (m_dockState == DockState::DockedRight_Collapsed) {
        m_x = screen.x + screen.width - curW;
    } else if (m_dockState == DockState::DockedLeft_Collapsed) {
        m_x = screen.x;
    } else if (m_dockState == DockState::DockedTop_Collapsed) {
        m_y = screen.y;
    }

    if (m_pRenderTarget->GetWidth() != curW || m_pRenderTarget->GetHeight() != curH ||
        m_width != curW || m_height != curH) {
        m_pRenderTarget->Resize(curW, curH);
        m_width = curW;
        m_height = curH;
        XMoveResizeWindow(dpy, m_window, m_x, m_y, curW, curH);
    }

    m_pRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0.0f));
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Scale(dpiScale, dpiScale));

    const auto& config = ConfigManager::Instance().GetConfig();
    const auto& colors = ThemeManager::Instance().GetColors();
    bool isDark = ThemeManager::Instance().IsEffectiveDark();

    D2D1_COLOR_F accent = colors.forestGreen;
    if (m_state == AppState::Standing) accent = colors.skyBlue;
    else if (m_state == AppState::Resting) accent = colors.alertRed;
    else if (m_state == AppState::Paused) accent = colors.textMuted;

    // 坐姿临界 30 秒泛红预警
    if (m_state == AppState::Working && m_remainingSeconds <= 30 && m_remainingSeconds > 0) {
        accent = colors.alertRed;
    }

    float strokeW = GetStrokeWidthForBorder(config.borderWidth);

    if (isCollapsed) {
        bool isLeft = (m_dockState == DockState::DockedLeft_Collapsed);
        D2D1_RECT_F tabRect = D2D1::RectF(0, 0, logW, logH);
        MascotRenderer::Instance().DrawMascotDockTab(
            m_pRenderTarget.get(), tabRect, config.mascotTheme, m_state,
            m_remainingSeconds, m_totalSeconds, m_animTick, accent,
            isLeft, 1.0f, isDark
        );
    } else {
        // 1. 卡片圆角半透明磨砂背景 (与 Windows 端保持 100% 一致)
        float cornerRadius = 12.0f;
        D2D1_RECT_F borderRect = D2D1::RectF(strokeW * 0.5f, strokeW * 0.5f, logW - strokeW * 0.5f, logH - strokeW * 0.5f);
        D2D1_ROUNDED_RECT cardRounded = D2D1::RoundedRect(borderRect, cornerRadius, cornerRadius);
        D2D1_COLOR_F cardBgColor = isDark
            ? D2D1::ColorF(0.09f, 0.11f, 0.15f, 0.95f)
            : D2D1::ColorF(0.96f, 0.97f, 0.99f, 0.96f);
        LinuxBrush cardBgBrush(cardBgColor);
        m_pRenderTarget->FillRoundedRectangle(cardRounded, &cardBgBrush);

        // 2. 绘制外边框圆角流光进度条
        float progress = (m_totalSeconds > 0) ? std::clamp(static_cast<float>(m_remainingSeconds) / static_cast<float>(m_totalSeconds), 0.0f, 1.0f) : 1.0f;
        MascotRenderer::Instance().DrawRoundedRectProgress(
            m_pRenderTarget.get(), borderRect, cornerRadius, strokeW, progress, accent
        );

        // 3. 左侧大画幅人体工学吉祥物/形象 (居左对齐，释放右侧信息空间)
        D2D1_POINT_2F ringCenter{ 26.0f, logH * 0.5f };
        float ringRadius = 18.0f;
        MascotRenderer::Instance().DrawMascotFloating(
            m_pRenderTarget.get(), ringCenter, ringRadius, config.mascotTheme, m_state,
            m_remainingSeconds, m_totalSeconds, m_animTick, accent,
            1.0f, isDark
        );

        // 4. 右侧状态标签 (Top: y=8~24)
        float textLeft = 48.0f;
        float textRight = logW - 6.0f;

        std::wstring stateLabel;
        D2D1_COLOR_F stateColor;

        if (m_remainingSeconds <= 30 && m_state == AppState::Working && config.strongReminder) {
            stateLabel = L"即将休息";
            stateColor = isDark ? D2D1::ColorF(0.98f, 0.42f, 0.42f) : D2D1::ColorF(0.88f, 0.18f, 0.18f);
        } else if (m_state == AppState::Standing) {
            stateLabel = L"站立办公";
            stateColor = isDark ? D2D1::ColorF(0.98f, 0.75f, 0.15f) : D2D1::ColorF(0.85f, 0.52f, 0.05f);
        } else if (m_state == AppState::Resting) {
            stateLabel = L"工间放松";
            stateColor = isDark ? D2D1::ColorF(0.22f, 0.80f, 0.98f) : D2D1::ColorF(0.05f, 0.55f, 0.82f);
        } else if (m_state == AppState::Paused) {
            stateLabel = L"计时暂停";
            stateColor = isDark ? D2D1::ColorF(0.65f, 0.70f, 0.78f) : D2D1::ColorF(0.45f, 0.50f, 0.58f);
        } else {
            stateLabel = L"坐姿专注";
            stateColor = isDark ? D2D1::ColorF(0.20f, 0.88f, 0.58f) : D2D1::ColorF(0.06f, 0.65f, 0.35f);
        }

        auto fmtLabel = D2DContext::Instance().GetCachedTextFormat(L"sans-serif", 11.5f, DWRITE_FONT_WEIGHT_BOLD);
        if (fmtLabel) {
            LinuxBrush labelBrush(stateColor);
            D2D1_RECT_F labelRect = D2D1::RectF(textLeft, 8.0f, textRight, 24.0f);
            m_pRenderTarget->DrawText(stateLabel.c_str(), static_cast<UINT32>(stateLabel.length()), fmtLabel.Get(), labelRect, &labelBrush);
        }

        // 5. 右侧核心倒计时 mm:ss (Middle: y=24~62)
        int minutes = m_remainingSeconds / 60;
        int seconds = m_remainingSeconds % 60;
        wchar_t timeBuf[16];
        swprintf(timeBuf, 16, L"%02d:%02d", minutes, seconds);

        auto fmtTime = D2DContext::Instance().GetCachedTextFormat(
            L"sans-serif", 24.0f, DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING,
            DWRITE_PARAGRAPH_ALIGNMENT_CENTER
        );
        if (fmtTime) {
            D2D1_COLOR_F timeColor = isDark ? D2D1::ColorF(0.96f, 0.98f, 1.0f) : D2D1::ColorF(0.09f, 0.13f, 0.20f);
            LinuxBrush timeBrush(timeColor);
            D2D1_RECT_F timeRect = D2D1::RectF(textLeft, 22.0f, textRight, 60.0f);
            m_pRenderTarget->DrawText(timeBuf, static_cast<UINT32>(wcslen(timeBuf)), fmtTime.Get(), timeRect, &timeBrush);
        }
    }

    // 恢复变换矩阵
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    // 提交到 X11 窗口
    Visual* visual = X11App::Instance().GetVisual32();
    int depth = X11App::Instance().GetDepth32();

    XImage* ximage = XCreateImage(
        dpy, visual, depth, ZPixmap, 0,
        reinterpret_cast<char*>(m_pRenderTarget->GetPixels()),
        curW, curH, 32, 0
    );

    if (ximage) {
        XPutImage(dpy, m_window, m_gc, ximage, 0, 0, 0, 0, curW, curH);
        ximage->data = nullptr; // 防止 XDestroyImage 释放 m_pixels 的内存
        XDestroyImage(ximage);
    }

    const char* dumpPath = getenv("DUMP_FLOATING_BMP");
    if (dumpPath) {
        static bool s_dumped = false;
        if (!s_dumped) {
            s_dumped = true;
            uint32_t imageSize = curW * 4 * curH;
            uint32_t fileSize = 54 + imageSize;
            uint8_t header[54] = {
                'B', 'M',
                (uint8_t)(fileSize), (uint8_t)(fileSize >> 8), (uint8_t)(fileSize >> 16), (uint8_t)(fileSize >> 24),
                0, 0, 0, 0,
                54, 0, 0, 0,
                40, 0, 0, 0,
                (uint8_t)(curW), (uint8_t)(curW >> 8), (uint8_t)(curW >> 16), (uint8_t)(curW >> 24),
                (uint8_t)(-curH), (uint8_t)((-curH) >> 8), (uint8_t)((-curH) >> 16), (uint8_t)((-curH) >> 24),
                1, 0,
                32, 0,
                0, 0, 0, 0,
                (uint8_t)(imageSize), (uint8_t)(imageSize >> 8), (uint8_t)(imageSize >> 16), (uint8_t)(imageSize >> 24),
                0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0
            };
            if (FILE* f = fopen(dumpPath, "wb")) {
                fwrite(header, 1, 54, f);
                fwrite(m_pRenderTarget->GetPixels(), 4, curW * curH, f);
                fclose(f);
            }
        }
    }
}

void FloatingWindow::HandleEvent(const XEvent& ev) {
    auto* dpy = X11App::Instance().GetDisplay();

    switch (ev.type) {
    case Expose:
        Render();
        break;

    case ButtonPress: {
        // 若当前有右键菜单打开，非右键点击悬浮窗时立即关闭菜单
        if (TrayWindow::Instance().IsMenuVisible() && ev.xbutton.button != Button3) {
            TrayWindow::Instance().HideMenu();
        }

        if (ev.xbutton.button == Button1) {
            // 左键双击唤醒设置中心
            auto now = std::chrono::steady_clock::now();
            if (m_hasLastClick) {
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastClickTime).count();
                if (diff < 300) {
                    m_hasLastClick = false;
                    SettingsWindow::Instance().Show();
                    return;
                }
            }
            m_hasLastClick = true;
            m_lastClickTime = now;

            // 获取当前所在屏幕
            auto screen = X11App::Instance().GetScreenForPoint(m_x + m_width / 2, m_y + m_height / 2);

            // 如果已折叠，点击立即展开
            if (m_dockState == DockState::DockedLeft_Collapsed) {
                StartSlideAnimation(screen.x, true, DockState::Snapped_Left);
                return;
            } else if (m_dockState == DockState::DockedRight_Collapsed) {
                StartSlideAnimation(screen.x + screen.width - m_width, true, DockState::Snapped_Right);
                return;
            } else if (m_dockState == DockState::DockedTop_Collapsed) {
                StartSlideAnimation(screen.y, false, DockState::Snapped_Top);
                return;
            }

            // 开始平滑拖拽并独占捕获指针
            m_isDragging = true;
            m_dragStartMouseX = ev.xbutton.x_root;
            m_dragStartMouseY = ev.xbutton.y_root;
            m_dragStartWinX = m_x;
            m_dragStartWinY = m_y;
            XGrabPointer(dpy, m_window, False,
                ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync,
                None, None, CurrentTime);
        } else if (ev.xbutton.button == Button3) {
            // 右键弹出菜单
            TrayWindow::Instance().ShowContextMenu();
        }
        break;
    }

    case ButtonRelease: {
        if (ev.xbutton.button == Button1 && m_isDragging) {
            m_isDragging = false;
            XUngrabPointer(dpy, CurrentTime);
            CheckEdgeDock(true);
        }
        break;
    }

    case MotionNotify: {
        if (m_isDragging) {
            m_x = m_dragStartWinX + (ev.xmotion.x_root - m_dragStartMouseX);
            m_y = m_dragStartWinY + (ev.xmotion.y_root - m_dragStartMouseY);
            XMoveWindow(dpy, m_window, m_x, m_y);
            CheckEdgeDock(false);
        }
        break;
    }

    case EnterNotify: {
        m_isMouseHovered = true;
        m_outsideTicks = 0;
        auto screen = X11App::Instance().GetScreenForPoint(m_x + m_width / 2, m_y + m_height / 2);
        // 鼠标移入折叠拉手，0 延迟平滑弹出
        if (m_dockState == DockState::DockedLeft_Collapsed) {
            StartSlideAnimation(screen.x, true, DockState::DockedLeft_Expanded);
        } else if (m_dockState == DockState::DockedRight_Collapsed) {
            StartSlideAnimation(screen.x + screen.width - m_width, true, DockState::DockedRight_Expanded);
        } else if (m_dockState == DockState::DockedTop_Collapsed) {
            StartSlideAnimation(screen.y, false, DockState::DockedTop_Expanded);
        }
        break;
    }

    case LeaveNotify: {
        m_isMouseHovered = false;
        m_outsideTicks = 0;
        break;
    }
    }
}

// 智能双模多屏边缘吸附 (Multi-Monitor Smart Snapping & Push-to-Collapse)
void FloatingWindow::CheckEdgeDock(bool isFinal) {
    if (!ConfigManager::Instance().GetConfig().enableEdgeDock) return;
    auto* dpy = X11App::Instance().GetDisplay();
    auto screen = X11App::Instance().GetScreenForPoint(m_x + m_width / 2, m_y + m_height / 2);

    constexpr int SNAP_THRESHOLD = 20; // 20px 磁吸距离

    if (isFinal) {
        // 拖拽释放时：若是推向当前屏幕外（越界 > 2px），激活折叠模式
        float dpiScale = X11App::Instance().GetDisplayDpiScale();
        int tabWidth = static_cast<int>(std::round(AppConstants::FloatingWindowDimensions::DOCK_TAB_WIDTH * dpiScale));
        if (m_x < screen.x - 2) {
            StartSlideAnimation(screen.x, true, DockState::DockedLeft_Collapsed);
            return;
        } else if (m_x + m_width > screen.x + screen.width + 2) {
            StartSlideAnimation(screen.x + screen.width - tabWidth, true, DockState::DockedRight_Collapsed);
            return;
        } else if (m_y < screen.y - 2) {
            StartSlideAnimation(screen.y, false, DockState::DockedTop_Collapsed);
            return;
        }

        // 磁力对齐吸附常驻模式 (Snapped Resident)
        if (m_x >= screen.x && m_x <= screen.x + SNAP_THRESHOLD) {
            m_x = screen.x;
            m_dockState = DockState::Snapped_Left;
            XMoveWindow(dpy, m_window, m_x, m_y);
        } else if (m_x + m_width >= screen.x + screen.width - SNAP_THRESHOLD && m_x + m_width <= screen.x + screen.width) {
            m_x = screen.x + screen.width - m_width;
            m_dockState = DockState::Snapped_Right;
            XMoveWindow(dpy, m_window, m_x, m_y);
        } else if (m_y >= screen.y && m_y <= screen.y + SNAP_THRESHOLD) {
            m_y = screen.y;
            m_dockState = DockState::Snapped_Top;
            XMoveWindow(dpy, m_window, m_x, m_y);
        } else {
            m_dockState = DockState::Floating;
        }
    }
}

void FloatingWindow::StartSlideAnimation(int targetPos, bool isHorizontal, DockState finalState) {
    auto* dpy = X11App::Instance().GetDisplay();
    float dpiScale = X11App::Instance().GetDisplayDpiScale();
    int baseW = static_cast<int>(std::round(AppConstants::FloatingWindowDimensions::BASE_WIDTH * dpiScale));
    int baseH = static_cast<int>(std::round(AppConstants::FloatingWindowDimensions::BASE_HEIGHT * dpiScale));
    int tabW = static_cast<int>(std::round(AppConstants::FloatingWindowDimensions::DOCK_TAB_WIDTH * dpiScale));

    m_isAnimating = true;
    m_animIsHorizontal = isHorizontal;
    m_animStartPos = isHorizontal ? m_x : m_y;
    m_animTargetPos = targetPos;
    m_animCurrentFrame = 0;
    m_animTotalFrames = 10; // ~160ms 极致丝滑
    m_animFinalState = finalState;

    // 若是从展开进入收起折叠形态，在位移前先行原子切换尺寸，杜绝超界窗口被 Mutter 拦截推回屏幕内
    bool toCollapsed = (finalState == DockState::DockedLeft_Collapsed ||
                        finalState == DockState::DockedRight_Collapsed ||
                        finalState == DockState::DockedTop_Collapsed);
    if (toCollapsed && dpy && m_window && m_pRenderTarget) {
        int targetW = (finalState == DockState::DockedTop_Collapsed) ? baseW : tabW;
        int targetH = (finalState == DockState::DockedTop_Collapsed) ? tabW : baseH;
        if (m_width != targetW || m_height != targetH) {
            m_width = targetW;
            m_height = targetH;
            m_pRenderTarget->Resize(m_width, m_height);
            XResizeWindow(dpy, m_window, m_width, m_height);
        }
    }
}

void FloatingWindow::OnAnimationTick() {
    auto* dpy = X11App::Instance().GetDisplay();
    if (!dpy || !m_window) return;

    // 1. 移开鼠标 400ms 后自动平滑折叠检测
    if (!m_isDragging && !m_isAnimating && !m_isMouseHovered) {
        if (m_dockState == DockState::DockedLeft_Expanded ||
            m_dockState == DockState::DockedRight_Expanded ||
            m_dockState == DockState::DockedTop_Expanded) {
            m_outsideTicks++;
            if (m_outsideTicks >= 25) { // 25 * 16ms = 400ms
                m_outsideTicks = 0;
                auto screen = X11App::Instance().GetScreenForPoint(m_x + m_width / 2, m_y + m_height / 2);
                float dpiScale = X11App::Instance().GetDisplayDpiScale();
                int tabW = static_cast<int>(std::round(AppConstants::FloatingWindowDimensions::DOCK_TAB_WIDTH * dpiScale));
                if (m_dockState == DockState::DockedLeft_Expanded) {
                    StartSlideAnimation(screen.x, true, DockState::DockedLeft_Collapsed);
                } else if (m_dockState == DockState::DockedRight_Expanded) {
                    StartSlideAnimation(screen.x + screen.width - tabW, true, DockState::DockedRight_Collapsed);
                } else if (m_dockState == DockState::DockedTop_Expanded) {
                    StartSlideAnimation(screen.y, false, DockState::DockedTop_Collapsed);
                }
            }
        }
    }

    // 2. 60FPS 平滑插值动画步进
    if (m_isAnimating) {
        m_animCurrentFrame++;
        float t = static_cast<float>(m_animCurrentFrame) / static_cast<float>(m_animTotalFrames);
        // 三次平滑缓动 (Cubic Ease-Out)
        float ease = 1.0f - std::pow(1.0f - t, 3.0f);

        int currentPos = static_cast<int>(m_animStartPos + (m_animTargetPos - m_animStartPos) * ease);
        if (m_animIsHorizontal) {
            m_x = currentPos;
        } else {
            m_y = currentPos;
        }
        XMoveWindow(dpy, m_window, m_x, m_y);

        if (m_animCurrentFrame >= m_animTotalFrames) {
            m_isAnimating = false;
            m_dockState = m_animFinalState;
            Render();
        }
    }
}

void FloatingWindow::SimulateClick(int x, int y, int button) {
    XEvent ev{};
    ev.type = ButtonPress;
    ev.xbutton.button = button;
    ev.xbutton.x = x;
    ev.xbutton.y = y;
    HandleEvent(ev);
}

#endif
