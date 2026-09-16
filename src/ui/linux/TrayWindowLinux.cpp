#include "../TrayWindow.hpp"
#include "X11App.hpp"
#include "../FloatingWindow.hpp"
#include "../SettingsWindow.hpp"
#include "../../graphics/D2DContext.hpp"
#include "../../platform/ThemeManager.hpp"
#include "../../core/ConfigManager.hpp"
#include "X11Utils.hpp"
#include "StatusNotifierItemLinux.hpp"
#include <algorithm>
#include <iostream>
#include <cstring>
#include <time.h>

#ifndef _WIN32

extern StateMachine* g_pStateMachine;

bool TrayWindow::Create(void* /*hInstance*/) {
    auto& app = X11App::Instance();
    if (!app.Initialize()) return false;

    Display* dpy = app.GetDisplay();
    int screen = app.GetScreen();

    // 1. 初始化菜单项
    m_menuItems = {
        { IDM_TRAY_START_WORK,   L"坐姿工作 (Start Work)", false, false },
        { IDM_TRAY_START_STAND,  L"站立办公 (Start Stand)", false, false },
        { IDM_TRAY_START_REST,   L"全屏工间操 (Take Break)", false, false },
        { 0,                     L"", true, false },
        { IDM_TRAY_PAUSE_RESUME, L"暂停 / 继续 (Pause)", false, false },
        { IDM_TRAY_POSTPONE_5M,  L"延后 5 分钟 (+5m)", false, false },
        { IDM_TRAY_SKIP,         L"跳过当前阶段 (Skip)", false, false },
        { 0,                     L"", true, false },
        { IDM_TRAY_PRESET_1,     L"预设：45m 坐 / 15m 站", false, false },
        { IDM_TRAY_PRESET_2,     L"预设：50m 坐 / 10m 站", false, false },
        { IDM_TRAY_PRESET_3,     L"预设：25m 番茄工作法", false, false },
        { IDM_TRAY_PRESET_4,     L"预设：60m 深度办公", false, false },
        { 0,                     L"", true, false },
        { IDM_TRAY_SETTINGS,     L"设置中心 (Settings)...", false, false },
        { IDM_TRAY_EXIT,         L"退出 (Exit)", false, false }
    };

    // 2. 创建自绘 Fluent 上下文菜单窗口
    float dpiScale = app.GetDisplayDpiScale();
    int menuWLog = 230;
    int totalHLog = 8;
    for (const auto& it : m_menuItems) {
        totalHLog += it.isSeparator ? 9 : 25;
    }
    totalHLog += 8;

    m_menuW = static_cast<int>(std::round(menuWLog * dpiScale));
    m_menuH = static_cast<int>(std::round(totalHLog * dpiScale));

    Visual* visual = app.GetVisual32();
    int depth = app.GetDepth32();
    Colormap cmap = app.GetColormap32();

    XSetWindowAttributes menuAttrs = {};
    menuAttrs.colormap = cmap;
    menuAttrs.background_pixmap = None;
    menuAttrs.border_pixel = 0;
    menuAttrs.override_redirect = True;
    menuAttrs.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask |
                           ButtonReleaseMask | PointerMotionMask | LeaveWindowMask |
                           KeyPressMask | FocusChangeMask;

    unsigned long menuMask = CWColormap | CWBackPixmap | CWBorderPixel | CWEventMask | CWOverrideRedirect;

    m_menuWindow = XCreateWindow(
        dpy, RootWindow(dpy, screen),
        0, 0, m_menuW, m_menuH, 0,
        depth, InputOutput, visual, menuMask, &menuAttrs
    );

    m_menuGC = XCreateGC(dpy, m_menuWindow, 0, nullptr);
    m_menuRT = std::make_unique<LinuxRenderTarget>(m_menuW, m_menuH);

    app.RegisterWindow(m_menuWindow, [this](const XEvent& ev) { HandleMenuEvent(ev); });

    // 3. 优先初始化现代 Linux 桌面 D-Bus StatusNotifierItem (SNI) 协议 (Ubuntu/Debian GNOME/KDE 原生标准)
    bool sniReady = StatusNotifierItemLinux::Instance().Initialize(
        [this](int /*x*/, int /*y*/) {
            ShowContextMenu();
        },
        [](int /*x*/, int /*y*/) {
            SettingsWindow::Instance().Show();
        }
    );

    StatusNotifierItemLinux::Instance().SetMenuCommandHandler(
        [this](uint32_t cmdId) {
            ExecuteCommand(cmdId);
        }
    );

    if (!m_trayRT) {
        m_trayRT = std::make_unique<LinuxRenderTarget>(24, 24);
    }

    // 4. 若 SNI 注册成功，则坚决不向 X11 发送停靠请求，彻底避免 GNOME 下出现双托盘图标与未知窗口；仅当 SNI 不可用时才 fallback
    if (!sniReady) {
        char trayAtomName[32];
        snprintf(trayAtomName, sizeof(trayAtomName), "_NET_SYSTEM_TRAY_S%d", screen);
        Atom traySelection = XInternAtom(dpy, trayAtomName, False);
        Window trayManager = XGetSelectionOwner(dpy, traySelection);

        if (trayManager != None) {
            XSetWindowAttributes trayAttrs = {};
            trayAttrs.colormap = cmap;
            trayAttrs.background_pixmap = None;
            trayAttrs.border_pixel = 0;
            trayAttrs.override_redirect = False;
            trayAttrs.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask;

            unsigned long trayMask = CWColormap | CWBackPixmap | CWBorderPixel | CWEventMask;

            m_trayWindow = XCreateWindow(
                dpy, RootWindow(dpy, screen),
                0, 0, 24, 24, 0,
                depth, InputOutput, visual, trayMask, &trayAttrs
            );

            if (m_trayWindow) {
                // 注入 XEmbed 协议规范属性 (Protocol Version = 0, Flags = XEMBED_MAPPED)、UTF-8 标题与 WM_CLASS
                long xembedData[2] = { 0, 1 };
                Atom xembedAtom = XInternAtom(dpy, "_XEMBED_INFO", False);
                XChangeProperty(dpy, m_trayWindow, xembedAtom, xembedAtom, 32, PropModeReplace, reinterpret_cast<unsigned char*>(xembedData), 2);
                X11Utils::SetWindowUtf8Title(dpy, m_trayWindow, "坐立提醒");
                X11Utils::SetWindowClass(dpy, m_trayWindow, "sitstandreminder", "SitStandReminder");

                m_trayGC = XCreateGC(dpy, m_trayWindow, 0, nullptr);

                // 发送 _NET_SYSTEM_TRAY_OPCODE 协议停靠面板
                XEvent dockEv;
                memset(&dockEv, 0, sizeof(dockEv));
                dockEv.xclient.type = ClientMessage;
                dockEv.xclient.window = trayManager;
                dockEv.xclient.message_type = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
                dockEv.xclient.format = 32;
                dockEv.xclient.data.l[0] = CurrentTime;
                dockEv.xclient.data.l[1] = 0; // SYSTEM_TRAY_REQUEST_DOCK
                dockEv.xclient.data.l[2] = m_trayWindow;
                XSendEvent(dpy, trayManager, False, NoEventMask, &dockEv);

                app.RegisterWindow(m_trayWindow, [this](const XEvent& ev) { HandleTrayEvent(ev); });
                XMapWindow(dpy, m_trayWindow);
            }
        }
    }

    // 立即渲染第一帧托盘动态图标
    RenderTray();
    app.AddAnimationCallback(this, [this]() { OnAnimationTick(); });

    return true;
}

void TrayWindow::Destroy() {
    StatusNotifierItemLinux::Instance().Shutdown();
    auto& app = X11App::Instance();
    app.RemoveAnimationCallback(this);
    if (m_menuWindow) {
        HideMenu();
        app.UnregisterWindow(m_menuWindow);
        if (m_menuGC) {
            XFreeGC(app.GetDisplay(), m_menuGC);
            m_menuGC = nullptr;
        }
        XDestroyWindow(app.GetDisplay(), m_menuWindow);
        m_menuWindow = 0;
    }
    if (m_trayWindow) {
        app.UnregisterWindow(m_trayWindow);
        if (m_trayGC) XFreeGC(app.GetDisplay(), m_trayGC);
        XDestroyWindow(app.GetDisplay(), m_trayWindow);
        m_trayWindow = 0;
    }
}

void TrayWindow::UpdateTooltip(const std::wstring& text) {
    m_tooltip = text;
    std::string utf8Text = X11Utils::WStringToUtf8(text);
    StatusNotifierItemLinux::Instance().SetTooltip("坐立提醒", utf8Text);
}

void TrayWindow::ShowBalloon(const std::wstring& /*title*/, const std::wstring& /*msg*/, uint32_t /*flags*/) {
    // 可选桌面气泡通知
}

void TrayWindow::UpdateDynamicIcon(AppState state, int remainingSec, int totalSec, const std::wstring& tooltip) {
    m_lastState = state;
    m_lastRemainingSec = remainingSec;
    m_lastTotalSec = totalSec;
    m_animFrame++;
    if (!tooltip.empty()) {
        UpdateTooltip(tooltip);
    }
    RenderTray();
}

static float GetLinuxCpuUsage() {
    static unsigned long long prevUser = 0, prevNice = 0, prevSystem = 0, prevIdle = 0;
    static unsigned long long prevIowait = 0, prevIrq = 0, prevSoftirq = 0, prevSteal = 0;
    static bool firstCall = true;

    FILE* file = fopen("/proc/stat", "r");
    if (!file) return 0.0f;

    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    int res = fscanf(file, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                     &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
    fclose(file);
    if (res < 4) return 0.0f;

    if (firstCall) {
        prevUser = user; prevNice = nice; prevSystem = system; prevIdle = idle;
        prevIowait = iowait; prevIrq = irq; prevSoftirq = softirq; prevSteal = steal;
        firstCall = false;
        return 0.0f;
    }

    unsigned long long prevIdleTotal = prevIdle + prevIowait;
    unsigned long long idleTotal = idle + iowait;

    unsigned long long prevNonIdle = prevUser + prevNice + prevSystem + prevIrq + prevSoftirq + prevSteal;
    unsigned long long nonIdle = user + nice + system + irq + softirq + steal;

    unsigned long long prevTotal = prevIdleTotal + prevNonIdle;
    unsigned long long total = idleTotal + nonIdle;

    unsigned long long totald = total - prevTotal;
    unsigned long long idled = idleTotal - prevIdleTotal;

    prevUser = user; prevNice = nice; prevSystem = system; prevIdle = idle;
    prevIowait = iowait; prevIrq = irq; prevSoftirq = softirq; prevSteal = steal;

    if (totald == 0) return 0.0f;
    float percent = (static_cast<float>(totald - idled) * 100.0f) / static_cast<float>(totald);
    return std::clamp(percent, 0.0f, 100.0f);
}

void TrayWindow::OnAnimationTick() {
    auto mode = ConfigManager::Instance().GetConfig().trayDisplayMode;
    if (mode != TrayDisplayMode::RunCatHealth && mode != TrayDisplayMode::RunCatCpu) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    static auto lastCatTick = now;

    int intervalMs = 200;
    if (mode == TrayDisplayMode::RunCatCpu) {
        float cpu = GetLinuxCpuUsage();
        intervalMs = static_cast<int>(250.0f - (cpu / 100.0f) * 200.0f);
        intervalMs = std::clamp(intervalMs, 50, 300);
    } else {
        if (m_lastState == AppState::Resting) {
            intervalMs = 400; // 休息时慢节奏打盹
        } else if (m_lastRemainingSec <= 300 && m_lastRemainingSec > 0) {
            intervalMs = 100; // 快到期急迫快跑
        } else {
            intervalMs = 200; // 匀速日常奔跑
        }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCatTick).count();
    if (elapsed >= intervalMs) {
        lastCatTick = now;
        m_animFrame++;
        RenderTray();
    }
}

void TrayWindow::RefreshTrayDisplayMode() {
    RenderTray();
}

void TrayWindow::RenderTray() {
    if (!m_trayRT) {
        m_trayRT = std::make_unique<LinuxRenderTarget>(24, 24);
    }

    m_trayRT->Clear(D2D1::ColorF(0, 0, 0, 0.0f));

    const auto& config = ConfigManager::Instance().GetConfig();
    const auto& colors = ThemeManager::Instance().GetColors();
    bool isDark = ThemeManager::Instance().IsEffectiveDark();

    if (config.trayDisplayMode == TrayDisplayMode::RunCatHealth || config.trayDisplayMode == TrayDisplayMode::RunCatCpu) {
        // ---------------- 奔跑猫猫 (RunCat) 矢量渲染 ----------------
        float scale = 24.0f / 16.0f;
        float cx = 24.0f * 0.48f;
        float cy = 24.0f * 0.52f;

        D2D1_COLOR_F catColor = isDark ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f) : D2D1::ColorF(0.12f, 0.15f, 0.22f, 0.95f);
        D2D1_COLOR_F pinkColor = D2D1::ColorF(1.0f, 0.62f, 0.70f, 0.95f);
        D2D1_COLOR_F eyeColor = isDark ? D2D1::ColorF(0.1f, 0.1f, 0.12f, 0.95f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f);
        LinuxBrush catBrush(catColor);

        if (m_lastState == AppState::Resting) {
            // 打盹模式
            float sleepY = cy + 1.0f * scale;
            m_trayRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, sleepY), 4.5f * scale, 3.2f * scale), &catBrush);

            float headX = cx - 2.8f * scale;
            float headY = sleepY - 1.2f * scale;
            m_trayRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX, headY), 2.4f * scale, 2.4f * scale), &catBrush);

            LinuxBrush eyeBrush(eyeColor);
            m_trayRT->DrawLine(D2D1::Point2F(headX - 1.2f * scale, headY), D2D1::Point2F(headX - 0.4f * scale, headY - 0.5f * scale), &eyeBrush, 0.8f * scale);
            m_trayRT->DrawLine(D2D1::Point2F(headX - 0.4f * scale, headY - 0.5f * scale), D2D1::Point2F(headX + 0.4f * scale, headY), &eyeBrush, 0.8f * scale);

            // 耳朵
            m_trayRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX - 1.0f * scale, headY - 2.0f * scale), 0.8f * scale, 1.2f * scale), &catBrush);
            m_trayRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX + 0.5f * scale, headY - 2.0f * scale), 0.8f * scale, 1.2f * scale), &catBrush);

            // 打盹小 'z' 泡泡
            int zPhase = m_animFrame % 3;
            float zX = cx + 2.0f * scale + zPhase * 0.8f * scale;
            float zY = cy - 2.5f * scale - zPhase * 1.2f * scale;
            auto fmtZ = D2DContext::Instance().GetCachedTextFormat(L"sans-serif", 4.0f * scale, DWRITE_FONT_WEIGHT_BOLD);
            if (fmtZ) {
                D2D1_RECT_F zRc = D2D1::RectF(zX, zY, zX + 8 * scale, zY + 8 * scale);
                m_trayRT->DrawText(L"z", 1, fmtZ.Get(), zRc, &catBrush);
            }
        } else {
            // 奔跑模式 (5 帧经典流畅动画)
            int f = m_animFrame % 5;
            float legStroke = 1.2f * scale;
            float bodyAngle = 0.0f;
            float bodyY = cy;
            float fLeg1X = 0, fLeg1Y = 0, fLeg2X = 0, fLeg2Y = 0;
            float bLeg1X = 0, bLeg1Y = 0, bLeg2X = 0, bLeg2Y = 0;
            float tailY = cy - 1.5f * scale;

            switch (f) {
                case 0:
                    bodyAngle = -8.0f; bodyY = cy - 0.8f * scale;
                    fLeg1X = 3.5f; fLeg1Y = 3.8f; fLeg2X = 1.8f; fLeg2Y = 2.8f;
                    bLeg1X = -4.2f; bLeg1Y = 3.6f; bLeg2X = -2.8f; bLeg2Y = 2.4f;
                    tailY = cy - 3.5f * scale;
                    break;
                case 1:
                    bodyAngle = 0.0f; bodyY = cy;
                    fLeg1X = 2.8f; fLeg1Y = 3.8f; fLeg2X = 3.4f; fLeg2Y = 3.6f;
                    bLeg1X = -3.2f; bLeg1Y = 2.0f; bLeg2X = -1.5f; bLeg2Y = 2.8f;
                    tailY = cy - 2.0f * scale;
                    break;
                case 2:
                    bodyAngle = 8.0f; bodyY = cy + 0.6f * scale;
                    fLeg1X = 0.8f; fLeg1Y = 2.8f; fLeg2X = 1.4f; fLeg2Y = 2.5f;
                    bLeg1X = -1.2f; bLeg1Y = 3.0f; bLeg2X = -0.5f; bLeg2Y = 2.6f;
                    tailY = cy - 1.2f * scale;
                    break;
                case 3:
                    bodyAngle = -12.0f; bodyY = cy - 0.4f * scale;
                    fLeg1X = 3.2f; fLeg1Y = 1.8f; fLeg2X = 2.2f; fLeg2Y = 1.4f;
                    bLeg1X = -4.5f; bLeg1Y = 3.8f; bLeg2X = -3.5f; bLeg2Y = 3.6f;
                    tailY = cy - 2.8f * scale;
                    break;
                case 4:
                    bodyAngle = -6.0f; bodyY = cy - 1.4f * scale;
                    fLeg1X = 4.2f; fLeg1Y = 2.4f; fLeg2X = 2.8f; fLeg2Y = 2.0f;
                    bLeg1X = -3.8f; bLeg1Y = 2.4f; bLeg2X = -2.6f; bLeg2Y = 1.8f;
                    tailY = cy - 3.8f * scale;
                    break;
            }

            // 绘制后腿
            m_trayRT->DrawLine(D2D1::Point2F(cx - 2.5f * scale, bodyY), D2D1::Point2F(cx + bLeg1X * scale, cy + bLeg1Y * scale), &catBrush, legStroke);
            m_trayRT->DrawLine(D2D1::Point2F(cx - 1.8f * scale, bodyY), D2D1::Point2F(cx + bLeg2X * scale, cy + bLeg2Y * scale), &catBrush, legStroke);

            // 绘制猫身 (椭圆身体)
            m_trayRT->SetTransform(D2D1::Matrix3x2F::Rotation(bodyAngle, D2D1::Point2F(cx, bodyY)));
            m_trayRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, bodyY), 4.2f * scale, 2.6f * scale), &catBrush);
            m_trayRT->SetTransform(D2D1::Matrix3x2F::Identity());

            // 绘制前爪
            m_trayRT->DrawLine(D2D1::Point2F(cx + 2.0f * scale, bodyY), D2D1::Point2F(cx + fLeg1X * scale, cy + fLeg1Y * scale), &catBrush, legStroke);
            m_trayRT->DrawLine(D2D1::Point2F(cx + 1.2f * scale, bodyY), D2D1::Point2F(cx + fLeg2X * scale, cy + fLeg2Y * scale), &catBrush, legStroke);

            // 绘制猫头
            float headX = cx + 3.8f * scale;
            float headY = bodyY - 1.6f * scale;
            m_trayRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX, headY), 2.2f * scale, 2.2f * scale), &catBrush);

            // 眼睛
            LinuxBrush eyeBrush(eyeColor);
            m_trayRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX + 0.8f * scale, headY - 0.2f * scale), 0.5f * scale, 0.5f * scale), &eyeBrush);

            // 耳朵
            m_trayRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX - 0.8f * scale, headY - 2.0f * scale), 0.8f * scale, 1.2f * scale), &catBrush);
            m_trayRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX + 0.6f * scale, headY - 2.0f * scale), 0.8f * scale, 1.2f * scale), &catBrush);
            LinuxBrush pinkBrush(pinkColor);
            m_trayRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX + 0.6f * scale, headY - 2.0f * scale), 0.4f * scale, 0.7f * scale), &pinkBrush);

            // 尾巴
            m_trayRT->DrawLine(D2D1::Point2F(cx - 3.8f * scale, bodyY), D2D1::Point2F(cx - 5.5f * scale, tailY), &catBrush, 1.0f * scale);
        }
    } else if (config.trayDisplayMode == TrayDisplayMode::DefaultIcon) {
        // ---------------- 经典静态盾牌/图标 ----------------
        D2D1_POINT_2F center{ 12.0f, 12.0f };
        LinuxBrush shieldBrush(colors.forestGreen);
        m_trayRT->FillEllipse(D2D1::Ellipse(center, 9.0f, 9.0f), &shieldBrush);

        LinuxBrush innerBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.9f));
        m_trayRT->FillEllipse(D2D1::Ellipse(center, 4.0f, 4.0f), &innerBrush);

        // 经典模式设置命名图标双保险
        StatusNotifierItemLinux::Instance().SetIconName("sitstandreminder");
    } else {
        // ---------------- 动态数字倒计时与扇形进度弧 ----------------
        int remainingSec = m_lastRemainingSec;
        int totalSec = m_lastTotalSec;
        if (totalSec <= 0 && g_pStateMachine) {
            remainingSec = g_pStateMachine->GetRemainingSeconds();
            totalSec = g_pStateMachine->GetTotalSeconds();
        }
        if (totalSec <= 0) {
            remainingSec = config.workMinutes * 60;
            totalSec = config.workMinutes * 60;
        }

        float cx = 12.0f;
        float cy = 12.0f;
        float strokeW = 2.0f;
        float radius = 9.5f;

        // 1. 底轨
        LinuxBrush trackBrush(isDark ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.2f) : D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.15f));
        m_trayRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), &trackBrush, strokeW);

        // 2. 动态进度扇弧
        float progress = 1.0f;
        if (totalSec > 0) {
            progress = std::clamp(static_cast<float>(remainingSec) / static_cast<float>(totalSec), 0.0f, 1.0f);
        }

        if (progress > 0.005f) {
            D2D1_COLOR_F progColor;
            if (m_lastState == AppState::Resting) {
                progColor = D2D1::ColorF(0.96f, 0.25f, 0.35f, 0.95f);
            } else if (remainingSec <= 300) {
                progColor = D2D1::ColorF(0.96f, 0.62f, 0.04f, 0.95f);
            } else if (m_lastState == AppState::Standing) {
                progColor = D2D1::ColorF(0.05f, 0.65f, 0.91f, 0.95f);
            } else {
                progColor = D2D1::ColorF(0.06f, 0.72f, 0.50f, 0.95f);
            }

            LinuxBrush progBrush(progColor);
            if (progress >= 0.999f) {
                m_trayRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), &progBrush, strokeW);
            } else {
                float startRad = -90.0f * 3.14159265f / 180.0f;
                float sweepRad = progress * 360.0f * 3.14159265f / 180.0f;
                int segments = (std::max)(6, static_cast<int>(progress * 36));
                for (int i = 0; i < segments; ++i) {
                    float t0 = static_cast<float>(i) / segments;
                    float t1 = static_cast<float>(i + 1) / segments;
                    float a0 = startRad + t0 * sweepRad;
                    float a1 = startRad + t1 * sweepRad;
                    m_trayRT->DrawLine(
                        D2D1::Point2F(cx + radius * cosf(a0), cy + radius * sinf(a0)),
                        D2D1::Point2F(cx + radius * cosf(a1), cy + radius * sinf(a1)),
                        &progBrush, strokeW
                    );
                }
            }
        }

        // 3. 中央剩余分钟数
        int mins = (remainingSec + 59) / 60;
        if (mins <= 0 && remainingSec > 0) mins = 1;
        wchar_t minStr[16];
        swprintf(minStr, 16, L"%d", mins);

        auto fmtMin = D2DContext::Instance().GetCachedTextFormat(
            L"sans-serif", 9.5f, DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER,
            DWRITE_PARAGRAPH_ALIGNMENT_CENTER
        );
        LinuxBrush textBrush(isDark ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f) : D2D1::ColorF(0.08f, 0.12f, 0.18f, 0.95f));
        if (fmtMin) {
            m_trayRT->DrawText(minStr, wcslen(minStr), fmtMin.Get(), D2D1::RectF(0, 0, 24, 24), &textBrush);
        }
    }

    // 1. 同步导出像素流至现代 D-Bus StatusNotifierItem (SNI) 托盘
    StatusNotifierItemLinux::Instance().SetIconPixmap(
        24, 24, reinterpret_cast<const uint32_t*>(m_trayRT->GetPixels())
    );

    // 2. 如果存在 XEmbed 传统托盘窗口，同步上屏
    if (m_trayWindow && m_trayGC) {
        auto* dpy = X11App::Instance().GetDisplay();
        if (dpy) {
            Visual* visual = X11App::Instance().GetVisual32();
            int depth = X11App::Instance().GetDepth32();

            XImage* ximage = XCreateImage(
                dpy, visual, depth, ZPixmap, 0,
                reinterpret_cast<char*>(m_trayRT->GetPixels()),
                24, 24, 32, 0
            );

            if (ximage) {
                XPutImage(dpy, m_trayWindow, m_trayGC, ximage, 0, 0, 0, 0, 24, 24);
                ximage->data = nullptr;
                XDestroyImage(ximage);
            }
        }
    }
}

void TrayWindow::HideMenu() {
    if (!m_menuVisible) return;
    auto* dpy = X11App::Instance().GetDisplay();
    if (dpy && m_menuWindow) {
        XUngrabPointer(dpy, CurrentTime);
        XUngrabKeyboard(dpy, CurrentTime);
        XUnmapWindow(dpy, m_menuWindow);
        XFlush(dpy);
    }
    m_menuVisible = false;
    m_menuHoverIndex = -1;
}

D2D1_RECT_F TrayWindow::GetMenuItemRect(size_t index) const {
    float dpiScale = X11App::Instance().GetDisplayDpiScale();
    float y = 8.0f * dpiScale;
    for (size_t i = 0; i < m_menuItems.size() && i <= index; ++i) {
        float h = (m_menuItems[i].isSeparator ? 9.0f : 25.0f) * dpiScale;
        if (i == index) {
            return D2D1::RectF(6.0f * dpiScale, y, m_menuW - 6.0f * dpiScale, y + (m_menuItems[i].isSeparator ? 9.0f : 24.0f) * dpiScale);
        }
        y += h;
    }
    return D2D1::RectF(0, 0, 0, 0);
}

int TrayWindow::GetMenuItemAt(int my) const {
    float dpiScale = X11App::Instance().GetDisplayDpiScale();
    int topPad = static_cast<int>(std::round(8.0f * dpiScale));
    if (my < topPad) return -1;
    int y = topPad;
    for (size_t i = 0; i < m_menuItems.size(); ++i) {
        int h = static_cast<int>(std::round((m_menuItems[i].isSeparator ? 9.0f : 25.0f) * dpiScale));
        if (my >= y && my < y + h) {
            if (m_menuItems[i].isSeparator) {
                return -1;
            }
            return static_cast<int>(i);
        }
        y += h;
    }
    return -1;
}


void TrayWindow::ShowContextMenu(const void* /*pPt*/) {
    auto* dpy = X11App::Instance().GetDisplay();
    if (!dpy || !m_menuWindow) return;

    if (m_menuVisible) {
        HideMenu();
        return;
    }

    Window root, child;
    int rootX, rootY, winX, winY;
    unsigned int mask;
    XQueryPointer(dpy, RootWindow(dpy, DefaultScreen(dpy)), &root, &child, &rootX, &rootY, &winX, &winY, &mask);

    int screenW = DisplayWidth(dpy, DefaultScreen(dpy));
    int screenH = DisplayHeight(dpy, DefaultScreen(dpy));

    int mx = (rootX + m_menuW > screenW) ? (rootX - m_menuW) : rootX;
    int my = (rootY + m_menuH > screenH) ? (rootY - m_menuH) : rootY;

    XMoveResizeWindow(dpy, m_menuWindow, mx, my, m_menuW, m_menuH);
    XMapRaised(dpy, m_menuWindow);
    XSync(dpy, False);
    m_menuVisible = true;
    m_menuHoverIndex = -1;
    RenderMenu();

    // 严密解决 X11 GrabNotViewable 异步竞争：重试抓取全局指针与键盘
    for (int retry = 0; retry < 50; ++retry) {
        int res = XGrabPointer(dpy, m_menuWindow, False,
            ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
            GrabModeAsync, GrabModeAsync,
            None, None, CurrentTime);
        if (res == GrabSuccess) break;
        struct timespec ts = { 0, 5 * 1000 * 1000 };
        nanosleep(&ts, nullptr);
    }

    for (int retry = 0; retry < 50; ++retry) {
        int res = XGrabKeyboard(dpy, m_menuWindow, False,
            GrabModeAsync, GrabModeAsync, CurrentTime);
        if (res == GrabSuccess) break;
        struct timespec ts = { 0, 5 * 1000 * 1000 };
        nanosleep(&ts, nullptr);
    }
}

void TrayWindow::HandleTrayEvent(const XEvent& ev) {
    if (ev.type == ButtonPress) {
        if (ev.xbutton.button == Button1) {
            // 左键切换悬浮窗显示或打开设置
            FloatingWindow::Instance().Show(true);
        } else if (ev.xbutton.button == Button3) {
            // 右键弹出菜单
            ShowContextMenu();
        }
    } else if (ev.type == Expose) {
        RenderTray();
    }
}

void TrayWindow::HandleMenuEvent(const XEvent& ev) {
    switch (ev.type) {
    case Expose:
        RenderMenu();
        break;

    case KeyPress: {
        KeySym sym = XLookupKeysym(const_cast<XKeyEvent*>(&ev.xkey), 0);
        if (sym == XK_Escape) {
            HideMenu();
        }
        break;
    }

    case FocusOut:
        HideMenu();
        break;

    case LeaveNotify:
        m_menuHoverIndex = -1;
        RenderMenu();
        break;

    case PointerMotionMask:
    case MotionNotify: {
        int mx = ev.xmotion.x;
        int my = ev.xmotion.y;
        if (mx < 0 || mx >= m_menuW || my < 0 || my >= m_menuH) {
            if (m_menuHoverIndex != -1) {
                m_menuHoverIndex = -1;
                RenderMenu();
            }
            break;
        }
        int idx = GetMenuItemAt(my);
        if (m_menuHoverIndex != idx) {
            m_menuHoverIndex = idx;
            RenderMenu();
        }
        break;
    }

    case ButtonPress: {
        int mx = ev.xbutton.x;
        int my = ev.xbutton.y;

        // 无论点击何处，均收起菜单
        HideMenu();

        // 若点击在菜单矩形范围外部，仅收起菜单而不触发任何命令
        if (mx < 0 || mx >= m_menuW || my < 0 || my >= m_menuH) {
            break;
        }

        int idx = GetMenuItemAt(my);
        if (idx >= 0 && idx < static_cast<int>(m_menuItems.size())) {
            uint32_t cmdId = m_menuItems[idx].id;
            if (cmdId != 0) {
                ExecuteCommand(cmdId);
            }
        }
        break;
    }
    }
}

void TrayWindow::ExecuteCommand(uint32_t cmdId) {
    if (!g_pStateMachine) return;

    switch (cmdId) {
    case IDM_TRAY_START_WORK:
        g_pStateMachine->StartWork();
        break;
    case IDM_TRAY_START_STAND:
        g_pStateMachine->StartStand();
        break;
    case IDM_TRAY_START_REST:
        g_pStateMachine->StartRest();
        break;
    case IDM_TRAY_PAUSE_RESUME:
        if (g_pStateMachine->GetState() == AppState::Paused) {
            g_pStateMachine->Resume();
        } else {
            g_pStateMachine->Pause();
        }
        break;
    case IDM_TRAY_POSTPONE_5M:
        g_pStateMachine->Postpone(5);
        break;
    case IDM_TRAY_SKIP:
        g_pStateMachine->SkipCurrent();
        break;
    case IDM_TRAY_PRESET_1:
        ConfigManager::Instance().ApplyPreset(45, 15, 90);
        g_pStateMachine->SetConfig(ConfigManager::Instance().GetConfig());
        FloatingWindow::Instance().OnConfigChanged();
        break;
    case IDM_TRAY_PRESET_2:
        ConfigManager::Instance().ApplyPreset(50, 10, 90);
        g_pStateMachine->SetConfig(ConfigManager::Instance().GetConfig());
        FloatingWindow::Instance().OnConfigChanged();
        break;
    case IDM_TRAY_PRESET_3:
        ConfigManager::Instance().ApplyPreset(25, 5, 60);
        g_pStateMachine->SetConfig(ConfigManager::Instance().GetConfig());
        FloatingWindow::Instance().OnConfigChanged();
        break;
    case IDM_TRAY_PRESET_4:
        ConfigManager::Instance().ApplyPreset(60, 20, 90);
        g_pStateMachine->SetConfig(ConfigManager::Instance().GetConfig());
        FloatingWindow::Instance().OnConfigChanged();
        break;
    case IDM_TRAY_SETTINGS:
        SettingsWindow::Instance().Show();
        break;
    case IDM_TRAY_EXIT:
        X11App::Instance().ExitEventLoop();
        break;
    }
}

void TrayWindow::RenderMenu() {
    if (!m_menuWindow || !m_menuRT) return;
    auto* dpy = X11App::Instance().GetDisplay();
    if (!dpy) return;

    float dpiScale = X11App::Instance().GetDisplayDpiScale();
    float logW = 230.0f;
    int totalHLog = 8;
    for (const auto& it : m_menuItems) {
        totalHLog += it.isSeparator ? 9 : 25;
    }
    totalHLog += 8;
    float logH = static_cast<float>(totalHLog);

    const auto& colors = ThemeManager::Instance().GetColors();
    bool isDark = ThemeManager::Instance().IsEffectiveDark();

    // 填充菜单背景与外边框
    m_menuRT->Clear(colors.cardBackground);
    m_menuRT->SetTransform(D2D1::Matrix3x2F::Scale(dpiScale, dpiScale));

    LinuxBrush borderBrush(colors.cardBorder);
    m_menuRT->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, logW - 0.5f, logH - 0.5f), 8.0f, 8.0f), &borderBrush, 1.0f);

    LinuxBrush textBrush(colors.textPrimary);
    LinuxBrush textSecBrush(colors.textSecondary);
    LinuxBrush accentBrush(colors.forestGreen);
    LinuxBrush hoverBrush(D2D1::ColorF(isDark ? 0.30f : 0.88f, isDark ? 0.30f : 0.88f, isDark ? 0.34f : 0.92f, 0.5f));

    auto fmtItem = D2DContext::Instance().GetCachedTextFormat(
        L"sans-serif", 12.0f, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING,
        DWRITE_PARAGRAPH_ALIGNMENT_CENTER
    );

    int curPresetId = AppConstants::FindMatchingPresetId(
        ConfigManager::Instance().GetConfig().workMinutes,
        ConfigManager::Instance().GetConfig().standMinutes,
        ConfigManager::Instance().GetConfig().restSeconds
    );

    int y = 8;
    for (size_t i = 0; i < m_menuItems.size(); ++i) {
        const auto& item = m_menuItems[i];
        if (item.isSeparator) {
            m_menuRT->DrawLine(D2D1::Point2F(12, y + 4), D2D1::Point2F(logW - 12, y + 4), &borderBrush, 1.0f);
            y += 9;
        } else {
            if (static_cast<int>(i) == m_menuHoverIndex) {
                D2D1_ROUNDED_RECT hoverRect = D2D1::RoundedRect(D2D1::RectF(6, y, logW - 6, y + 24), 4.0f, 4.0f);
                m_menuRT->FillRoundedRectangle(hoverRect, &hoverBrush);
            }

            // 检查当前预设或状态指示圆点
            bool isActive = false;
            if (item.id == IDM_TRAY_PRESET_1 && curPresetId == 1) isActive = true;
            if (item.id == IDM_TRAY_PRESET_2 && curPresetId == 2) isActive = true;
            if (item.id == IDM_TRAY_PRESET_3 && curPresetId == 3) isActive = true;
            if (item.id == IDM_TRAY_PRESET_4 && curPresetId == 4) isActive = true;

            if (isActive) {
                m_menuRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(14, y + 12), 3.0f, 3.0f), &accentBrush);
            }

            D2D1_RECT_F textRect = D2D1::RectF(24, y, logW - 12, y + 24);
            m_menuRT->DrawText(item.text.c_str(), item.text.length(), fmtItem.Get(), textRect, &textBrush);
            y += 25;
        }
    }

    // 恢复变换矩阵
    m_menuRT->SetTransform(D2D1::Matrix3x2F::Identity());

    Visual* visual = X11App::Instance().GetVisual32();
    int depth = X11App::Instance().GetDepth32();

    XImage* ximage = XCreateImage(
        dpy, visual, depth, ZPixmap, 0,
        reinterpret_cast<char*>(m_menuRT->GetPixels()),
        m_menuW, m_menuH, 32, 0
    );

    if (ximage) {
        XPutImage(dpy, m_menuWindow, m_menuGC, ximage, 0, 0, 0, 0, m_menuW, m_menuH);
        ximage->data = nullptr;
        XDestroyImage(ximage);
    }
}

#endif
