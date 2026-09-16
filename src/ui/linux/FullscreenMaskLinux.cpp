#include "../FullscreenMask.hpp"
#include "X11App.hpp"
#include "../../graphics/ExerciseLayout.hpp"
#include "../../core/ConfigManager.hpp"
#include "../../platform/ThemeManager.hpp"
#include "../../graphics/D2DContext.hpp"
#include "X11Utils.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cstring>

#ifndef _WIN32

extern StateMachine* g_pStateMachine;

bool FullscreenMask::Initialize(void* /*hInstance*/) {
    auto& app = X11App::Instance();
    if (!app.Initialize()) return false;

    Display* dpy = app.GetDisplay();
    int screen = app.GetScreen();
    m_width = DisplayWidth(dpy, screen);
    m_height = DisplayHeight(dpy, screen);

    Visual* visual = app.GetVisual32();
    int depth = app.GetDepth32();
    Colormap cmap = app.GetColormap32();

    XSetWindowAttributes attrs = {};
    attrs.colormap = cmap;
    attrs.background_pixmap = None;
    attrs.border_pixel = 0;
    attrs.override_redirect = True; // 全屏独占覆盖全监视器
    attrs.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask;

    unsigned long mask = CWColormap | CWBackPixmap | CWBorderPixel | CWEventMask | CWOverrideRedirect;

    m_window = XCreateWindow(
        dpy, RootWindow(dpy, screen),
        0, 0, m_width, m_height, 0,
        depth, InputOutput, visual, mask, &attrs
    );

    if (!m_window) return false;

    // 注入全屏最高层级与模态屏保属性
    Atom wmState = app.GetAtom("_NET_WM_STATE");
    Atom stateFullscreen = app.GetAtom("_NET_WM_STATE_FULLSCREEN");
    Atom stateAbove = app.GetAtom("_NET_WM_STATE_ABOVE");
    Atom states[2] = { stateFullscreen, stateAbove };
    XChangeProperty(dpy, m_window, wmState, XA_ATOM, 32, PropModeReplace, (unsigned char*)states, 2);
    X11Utils::SetWindowUtf8Title(dpy, m_window, "坐立提醒 · 全屏休息");
    X11Utils::SetWindowClass(dpy, m_window, "sitstandreminder", "SitStandReminder");

    m_gc = XCreateGC(dpy, m_window, 0, nullptr);
    m_pRenderTarget = std::make_unique<LinuxRenderTarget>(m_width, m_height);
    m_pBgRenderTarget = std::make_unique<LinuxRenderTarget>(m_width, m_height);
    m_bgDirty = true;

    // 注册事件与 60FPS 动画帧循环
    app.RegisterWindow(m_window, [this](const XEvent& ev) { HandleEvent(ev); });
    app.AddAnimationCallback(this, [this]() { OnAnimationTick(); });

    return true;
}

void FullscreenMask::Destroy() {
    if (m_window) {
        auto& app = X11App::Instance();
        Display* dpy = app.GetDisplay();
        app.RemoveAnimationCallback(this);
        app.UnregisterWindow(m_window);
        if (dpy) {
            XUngrabKeyboard(dpy, CurrentTime);
        }
        DestroyXImage();
        if (m_gc && dpy) {
            XFreeGC(dpy, m_gc);
            m_gc = nullptr;
        }
        if (dpy) {
            XDestroyWindow(dpy, m_window);
        }
        m_window = 0;
        m_isVisible = false;
        m_pRenderTarget.reset();
        m_pBgRenderTarget.reset();
    }
}

void FullscreenMask::Show(bool show) {
    if (!m_window) return;
    auto* dpy = X11App::Instance().GetDisplay();

    if (show) {
        m_isVisible = true;
        m_bgDirty = true;
        m_lastExercisePhase = -1;
        m_lastFrameTime = std::chrono::steady_clock::now();
        m_lastRenderTime = std::chrono::steady_clock::now();
        m_currentStage = 0;

        m_neckRenderer.Reset();
        m_eyeRenderer.Reset();

        auto& config = ConfigManager::Instance().GetConfig();
        float totalSec = (m_totalSeconds > 0) ? static_cast<float>(m_totalSeconds) : static_cast<float>(config.restSeconds);

        if (config.exerciseMode == ExerciseMode::Comprehensive) {
            float neckSec = (std::max)(30.0f, totalSec / 2.0f);
            float eyeSec = (std::max)(30.0f, totalSec - neckSec);
            m_neckRenderer.SetTotalDuration(neckSec);
            m_eyeRenderer.SetTotalDuration(eyeSec);
        } else if (config.exerciseMode == ExerciseMode::NeckOnly) {
            m_neckRenderer.SetTotalDuration(totalSec);
        } else if (config.exerciseMode == ExerciseMode::EyeOnly) {
            m_eyeRenderer.SetTotalDuration(totalSec);
        }

        XMapRaised(dpy, m_window);

        // 如果开启防误触输入拦截，独占捕获全部键盘按键；检查返回值，若失败则安全降级设置焦点
        if (config.blockInput) {
            int grabStatus = XGrabKeyboard(dpy, m_window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
            if (grabStatus != GrabSuccess) {
                XSetInputFocus(dpy, m_window, RevertToParent, CurrentTime);
            }
        } else {
            XSetInputFocus(dpy, m_window, RevertToParent, CurrentTime);
        }

        Render();
    } else {
        m_isVisible = false;
        if (dpy) {
            XUngrabKeyboard(dpy, CurrentTime);
        }
        XUnmapWindow(dpy, m_window);
    }
}

void FullscreenMask::UpdateDisplay(int remainingSec, int totalSec, int stageIndex, const std::wstring& stageName) {
    if (stageIndex != m_currentStage) {
        m_currentStage = stageIndex;
        m_lastExercisePhase = -1;
        if (m_currentStage == 1) {
            m_eyeRenderer.Reset();
        } else if (m_currentStage == 0) {
            m_neckRenderer.Reset();
        }
    }
    m_remainingSeconds = remainingSec;
    m_totalSeconds = totalSec;
    m_currentStageName = stageName;
    m_bgDirty = true;
}

void FullscreenMask::OnEscape() {
    Show(false);
    if (g_pStateMachine) {
        g_pStateMachine->ExitRestEarly();
    }
}

void FullscreenMask::HandleEvent(const XEvent& ev) {
    switch (ev.type) {
    case Expose:
        Render();
        break;

    case KeyPress: {
        KeySym sym = XLookupKeysym(const_cast<XKeyEvent*>(&ev.xkey), 0);
        // 按 ESC 键安全紧急退出全屏工间操
        if (sym == XK_Escape) {
            OnEscape();
        }
        break;
    }
    }
}

void FullscreenMask::OnAnimationTick() {
    if (!m_isVisible || !m_window) return;

    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration_cast<std::chrono::microseconds>(now - m_lastFrameTime).count() / 1000000.0f;
    m_lastFrameTime = now;

    // 真实物理时间推进：允许合理的物理时间差 (0.001s ~ 0.25s)
    dt = std::clamp(dt, 0.001f, 0.25f);

    int currentPhase = 0;
    auto mode = ConfigManager::Instance().GetConfig().exerciseMode;
    if (mode == ExerciseMode::Comprehensive) {
        if (m_currentStage == 0) {
            m_neckRenderer.Update(dt);
            currentPhase = m_neckRenderer.GetCurrentPhase();
        } else {
            m_eyeRenderer.Update(dt);
            currentPhase = m_eyeRenderer.GetCurrentPhase();
        }
    } else if (mode == ExerciseMode::NeckOnly) {
        m_neckRenderer.Update(dt);
        currentPhase = m_neckRenderer.GetCurrentPhase();
    } else if (mode == ExerciseMode::EyeOnly) {
        m_eyeRenderer.Update(dt);
        currentPhase = m_eyeRenderer.GetCurrentPhase();
    }

    if (currentPhase != m_lastExercisePhase) {
        m_lastExercisePhase = currentPhase;
        m_bgDirty = true;
    }

    // 稳帧调度控制 (Frame Pacing)：按 ~30FPS (32ms 间隔) 执行重绘
    auto elapsedRender = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastRenderTime).count();
    if (elapsedRender >= 32) {
        m_lastRenderTime = now;
        Render();
    }
}

void FullscreenMask::DestroyXImage() {
    if (m_ximage) {
        m_ximage->data = nullptr;
        XDestroyImage(m_ximage);
        m_ximage = nullptr;
    }
}

void FullscreenMask::RenderBackground() {
    if (!m_pBgRenderTarget) return;

    // 清屏深邃沉浸暗黑背景 (#0D0E12)
    m_pBgRenderTarget->Clear(D2D1::ColorF(0.05f, 0.06f, 0.08f, 0.98f));

    float dpiScale = X11App::Instance().GetDisplayDpiScale();

    // 多显示器拓扑适配：以主屏为中心排版工间操教学要领与状态提示
    auto screens = X11App::Instance().GetScreenGeometries();
    auto primary = X11App::Instance().GetPrimaryScreen();
    float topBarH = 68.0f * dpiScale;

    // 对所有副显示器绘制舒缓沉浸提示，杜绝黑屏死机错觉
    for (const auto& s : screens) {
        if (!s.isPrimary && (s.x != primary.x || s.y != primary.y)) {
            auto fmtSub = D2DContext::Instance().GetCachedTextFormat(
                L"sans-serif", 16.0f * dpiScale, DWRITE_FONT_WEIGHT_REGULAR,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER
            );
            LinuxBrush subBrush(D2D1::ColorF(0.40f, 0.45f, 0.52f, 0.8f));
            D2D1_RECT_F subRc = D2D1::RectF(
                static_cast<float>(s.x), static_cast<float>(s.y),
                static_cast<float>(s.x + s.width), static_cast<float>(s.y + s.height)
            );
            const wchar_t* subTip = L"工间休息进行中 · 请随主屏幕提示舒展颈部与双眼";
            m_pBgRenderTarget->DrawText(subTip, wcslen(subTip), fmtSub.Get(), subRc, &subBrush);
        }
    }

    // 1. 顶部当前阶段文案 (加粗大字号，清新薄荷青绿)
    if (!m_currentStageName.empty()) {
        auto fmtStage = D2DContext::Instance().GetCachedTextFormat(L"sans-serif", 20.0f * dpiScale, DWRITE_FONT_WEIGHT_BOLD);
        LinuxBrush stageBrush(D2D1::ColorF(0.65f, 0.98f, 0.80f));
        D2D1_RECT_F stageRect = D2D1::RectF(
            primary.x + 32.0f * dpiScale,
            primary.y + 18.0f * dpiScale,
            primary.x + primary.width - 260.0f * dpiScale,
            primary.y + topBarH
        );
        m_pBgRenderTarget->DrawText(m_currentStageName.c_str(), m_currentStageName.length(), fmtStage.Get(), stageRect, &stageBrush);
    }

    // 2. 右上角动态倒计时 (纯白加粗，视觉与状态机完全实时同步)
    int minutes = m_remainingSeconds / 60;
    int seconds = m_remainingSeconds % 60;
    wchar_t timeBuf[32];
    swprintf(timeBuf, 32, L"剩余 %02d:%02d", minutes, seconds);
    auto fmtTime = D2DContext::Instance().GetCachedTextFormat(
        L"sans-serif", 20.0f * dpiScale, DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING
    );
    LinuxBrush timeBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f));
    D2D1_RECT_F timeRect = D2D1::RectF(
        primary.x + primary.width - 260.0f * dpiScale,
        primary.y + 18.0f * dpiScale,
        primary.x + primary.width - 32.0f * dpiScale,
        primary.y + topBarH
    );
    m_pBgRenderTarget->DrawText(timeBuf, wcslen(timeBuf), fmtTime.Get(), timeRect, &timeBrush);

    // 3. 绘制底部 ESC 快捷键指引提示
    auto fmtTip = D2DContext::Instance().GetCachedTextFormat(
        L"sans-serif", 13.0f * dpiScale, DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER
    );
    LinuxBrush mutedBrush(D2D1::ColorF(0.72f, 0.78f, 0.85f, 0.9f));
    D2D1_RECT_F tipRect = D2D1::RectF(
        static_cast<float>(primary.x),
        static_cast<float>(primary.y + primary.height - 36.0f * dpiScale),
        static_cast<float>(primary.x + primary.width),
        static_cast<float>(primary.y + primary.height)
    );
    const wchar_t* tipStr = L"如遇紧急事务需处理，可按【ESC 键】随时退出全屏工间操";
    m_pBgRenderTarget->DrawText(tipStr, wcslen(tipStr), fmtTip.Get(), tipRect, &mutedBrush);

    // 4. 绘制工间操静态底板与要领说明 (卡片大底板、标题、动作要领、医学依据)
    float padX = (std::max)(24.0f * dpiScale, primary.width * 0.035f);
    float bottomMargin = 46.0f * dpiScale;
    D2D1_RECT_F contentBounds = D2D1::RectF(
        primary.x + padX,
        primary.y + topBarH + 6.0f * dpiScale,
        primary.x + primary.width - padX,
        primary.y + primary.height - bottomMargin
    );

    auto mode = ConfigManager::Instance().GetConfig().exerciseMode;
    if (mode == ExerciseMode::Comprehensive) {
        if (m_currentStage == 0) {
            m_neckRenderer.RenderStatic(m_pBgRenderTarget.get(), contentBounds, dpiScale);
        } else {
            m_eyeRenderer.RenderStatic(m_pBgRenderTarget.get(), contentBounds, dpiScale);
        }
    } else if (mode == ExerciseMode::NeckOnly) {
        m_neckRenderer.RenderStatic(m_pBgRenderTarget.get(), contentBounds, dpiScale);
    } else if (mode == ExerciseMode::EyeOnly) {
        m_eyeRenderer.RenderStatic(m_pBgRenderTarget.get(), contentBounds, dpiScale);
    } else if (mode == ExerciseMode::Simple) {
        auto fmtSimple = D2DContext::Instance().GetCachedTextFormat(
            L"sans-serif", 26.0f * dpiScale, DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER
        );
        LinuxBrush simpleBrush(D2D1::ColorF(0.70f, 0.95f, 0.85f, 0.95f));
        float totalH = contentBounds.bottom - contentBounds.top;
        D2D1_RECT_F bannerRect = D2D1::RectF(
            contentBounds.left,
            contentBounds.top + totalH * 0.44f,
            contentBounds.right,
            contentBounds.top + totalH * 0.56f
        );
        const wchar_t* simpleMsg = L"暂别屏幕，极目远眺或闭目深呼吸，让身心重获活力";
        m_pBgRenderTarget->DrawText(simpleMsg, wcslen(simpleMsg), fmtSimple.Get(), bannerRect, &simpleBrush);
    }
}

void FullscreenMask::Render() {
    if (!m_window || !m_pRenderTarget || !m_pBgRenderTarget) return;
    auto* dpy = X11App::Instance().GetDisplay();
    if (!dpy) return;

    float dpiScale = X11App::Instance().GetDisplayDpiScale();
    auto primary = X11App::Instance().GetPrimaryScreen();
    float topBarH = 68.0f * dpiScale;
    float padX = (std::max)(24.0f * dpiScale, primary.width * 0.035f);
    float bottomMargin = 46.0f * dpiScale;
    D2D1_RECT_F contentBounds = D2D1::RectF(
        primary.x + padX,
        primary.y + topBarH + 6.0f * dpiScale,
        primary.x + primary.width - padX,
        primary.y + primary.height - bottomMargin
    );

    int cbX = static_cast<int>(contentBounds.left);
    int cbY = static_cast<int>(contentBounds.top);
    int cbW = static_cast<int>(contentBounds.right - contentBounds.left);
    int cbH = static_cast<int>(contentBounds.bottom - contentBounds.top);

    Visual* visual = X11App::Instance().GetVisual32();
    int depth = X11App::Instance().GetDepth32();

    // 规范的 XImage 尺寸检查与安全重建，绝不篡改 XImage 内部私有成员
    if (!m_ximage || m_ximage->width != m_width || m_ximage->height != m_height) {
        DestroyXImage();
        m_ximage = XCreateImage(
            dpy, visual, depth, ZPixmap, 0,
            reinterpret_cast<char*>(m_pRenderTarget->GetPixels()),
            m_width, m_height, 32, 0
        );
    } else {
        m_ximage->data = reinterpret_cast<char*>(m_pRenderTarget->GetPixels());
    }

    bool fullRepaint = m_bgDirty;
    if (m_bgDirty) {
        RenderBackground();
        m_bgDirty = false;
        // 背景变动时全量同步
        std::memcpy(m_pRenderTarget->GetPixels(), m_pBgRenderTarget->GetPixels(), m_width * m_height * sizeof(uint32_t));
    } else {
        // 高频动态帧：快速单行恢复 contentBounds 视口区域底色
        const uint32_t* bgPixels = m_pBgRenderTarget->GetPixels();
        uint32_t* fgPixels = m_pRenderTarget->GetPixels();
        for (int y = cbY; y < cbY + cbH; ++y) {
            if (y >= 0 && y < m_height && cbX >= 0 && (cbX + cbW) <= m_width) {
                std::memcpy(fgPixels + y * m_width + cbX, bgPixels + y * m_width + cbX, cbW * sizeof(uint32_t));
            }
        }
    }

    // 绘制工间操核心动态人偶视窗
    m_pRenderTarget->PushAxisAlignedClip(contentBounds, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    auto mode = ConfigManager::Instance().GetConfig().exerciseMode;
    if (mode == ExerciseMode::Comprehensive) {
        if (m_currentStage == 0) {
            m_neckRenderer.RenderDynamic(m_pRenderTarget.get(), contentBounds, dpiScale);
        } else {
            m_eyeRenderer.RenderDynamic(m_pRenderTarget.get(), contentBounds, dpiScale);
        }
    } else if (mode == ExerciseMode::NeckOnly) {
        m_neckRenderer.RenderDynamic(m_pRenderTarget.get(), contentBounds, dpiScale);
    } else if (mode == ExerciseMode::EyeOnly) {
        m_eyeRenderer.RenderDynamic(m_pRenderTarget.get(), contentBounds, dpiScale);
    } else if (mode == ExerciseMode::Simple) {
        auto layout = ExerciseLayout::Calculate(contentBounds, dpiScale);
        D2D1_POINT_2F center{ layout.canvasCenterX, layout.canvasCenterY };
        float radius = (std::min)(primary.width, primary.height) * 0.15f;
        LinuxBrush ringBrush(ThemeManager::Instance().GetColors().forestGreen);
        m_pRenderTarget->DrawEllipse(D2D1::Ellipse(center, radius, radius), &ringBrush, 3.0f * dpiScale);
    }
    m_pRenderTarget->PopAxisAlignedClip();

    if (m_ximage) {
        if (fullRepaint) {
            XPutImage(dpy, m_window, m_gc, m_ximage, 0, 0, 0, 0, m_width, m_height);
        } else {
            // 局部脏矩形提交
            XPutImage(dpy, m_window, m_gc, m_ximage, cbX, cbY, cbX, cbY, cbW, cbH);
        }
        XFlush(dpy);
    }

    const char* dumpPrefix = std::getenv("DUMP_FULLSCREEN_PREFIX");
    if (dumpPrefix && dumpPrefix[0] != '\0') {
        static int dumpFrameCount = 0;
        if (dumpFrameCount < 5) {
            char filename[256];
            std::snprintf(filename, sizeof(filename), "%s_frame%d.bmp", dumpPrefix, dumpFrameCount++);
            std::ofstream bmpFile(filename, std::ios::binary);
            if (bmpFile.is_open()) {
#pragma pack(push, 1)
                struct BMPHeader {
                    uint16_t bfType = 0x4D42;
                    uint32_t bfSize = 0;
                    uint16_t bfReserved1 = 0;
                    uint16_t bfReserved2 = 0;
                    uint32_t bfOffBits = 54;
                    uint32_t biSize = 40;
                    int32_t  biWidth = 0;
                    int32_t  biHeight = 0;
                    uint16_t biPlanes = 1;
                    uint16_t biBitCount = 32;
                    uint32_t biCompression = 0;
                    uint32_t biSizeImage = 0;
                    int32_t  biXPelsPerMeter = 0;
                    int32_t  biYPelsPerMeter = 0;
                    uint32_t biClrUsed = 0;
                    uint32_t biClrImportant = 0;
                } hdr;
#pragma pack(pop)
                uint32_t imageSize = m_width * m_height * 4;
                hdr.bfSize = 54 + imageSize;
                hdr.biWidth = static_cast<int32_t>(m_width);
                hdr.biHeight = -static_cast<int32_t>(m_height);
                hdr.biSizeImage = imageSize;
                bmpFile.write(reinterpret_cast<const char*>(&hdr), 54);
                bmpFile.write(reinterpret_cast<const char*>(m_pRenderTarget->GetPixels()), imageSize);
            }
        }
    }
}

#endif
