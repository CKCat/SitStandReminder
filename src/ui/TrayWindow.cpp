#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "TrayWindow.hpp"
#include "SettingsWindow.hpp"
#include "FloatingWindow.hpp"
#include "resource.h"
#include "../graphics/D2DContext.hpp"
#include "../graphics/DynamicTrayIcon.hpp"
#include "../core/ConfigManager.hpp"
#include "../core/AppConstants.hpp"
#include "../platform/ThemeManager.hpp"
#include <shellscalingapi.h>
#include <wtsapi32.h>
#include <cwchar>
#include <algorithm>

extern StateMachine* g_pStateMachine;

#define IDT_TRAY_ANIMATION 4001

static UINT s_uTaskbarCreated = 0;

static float GetTrayDpiScale() {
    UINT dpi = 96;
    POINT pt;
    if (GetCursorPos(&pt)) {
        HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        if (hMon) {
            UINT dpiX = 96, dpiY = 96;
            if (SUCCEEDED(GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
                dpi = dpiX;
            }
        }
    }
    return dpi / 96.0f;
}

bool TrayWindow::Create(HINSTANCE hInstance) {
    if (m_hwnd) return true;
    m_hInstance = hInstance ? hInstance : GetModuleHandleW(nullptr);

    if (s_uTaskbarCreated == 0) {
        s_uTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    }

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = AppConstants::Identity::CLASS_TRAY;
    wc.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        AppConstants::Identity::TITLE_TRAY,
        WS_POPUP, 0, 0, 0, 0,
        nullptr, nullptr, m_hInstance, this
    );

    if (!m_hwnd) return false;

    // 允许跨权限前置唤醒消息穿透 UIPI (User Interface Privilege Isolation)
    ChangeWindowMessageFilterEx(m_hwnd, WM_COMMAND, MSGFLT_ALLOW, nullptr);

    ThemeManager::Instance().ApplyThemeToWindow(m_hwnd);

    // 注册 Windows 会话锁屏/解锁通知
    WTSRegisterSessionNotification(m_hwnd, NOTIFY_FOR_THIS_SESSION);

    memset(&m_nid, 0, sizeof(m_nid));
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAY_NOTIFY;
    m_nid.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wcscpy_s(m_nid.szTip, AppConstants::Identity::TRAY_DEFAULT_TIP);

    m_added = Shell_NotifyIconW(NIM_ADD, &m_nid);
    RefreshTrayDisplayMode();
    return m_added;
}

void TrayWindow::Destroy() {
    if (m_hwnd) {
        WTSUnRegisterSessionNotification(m_hwnd);
    }
    if (m_animTimerId && m_hwnd) {
        KillTimer(m_hwnd, IDT_TRAY_ANIMATION);
        m_animTimerId = 0;
    }
    if (m_added) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_added = false;
    }
    if (m_hDynamicIcon) {
        DestroyIcon(m_hDynamicIcon);
        m_hDynamicIcon = nullptr;
    }
    if (m_hMenuFont) { DeleteObject(m_hMenuFont); m_hMenuFont = nullptr; }
    if (m_hMenuBoldFont) { DeleteObject(m_hMenuBoldFont); m_hMenuBoldFont = nullptr; }
    if (m_hMenuDarkBgBrush) { DeleteObject(m_hMenuDarkBgBrush); m_hMenuDarkBgBrush = nullptr; }
    if (m_hMenuLightBgBrush) { DeleteObject(m_hMenuLightBgBrush); m_hMenuLightBgBrush = nullptr; }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void TrayWindow::RefreshTrayDisplayMode() {
    if (!m_added || !m_hwnd) return;
    auto mode = ConfigManager::Instance().GetConfig().trayDisplayMode;

    if (mode == TrayDisplayMode::DefaultIcon) {
        if (m_animTimerId) {
            KillTimer(m_hwnd, IDT_TRAY_ANIMATION);
            m_animTimerId = 0;
        }
        if (m_hDynamicIcon) {
            DestroyIcon(m_hDynamicIcon);
            m_hDynamicIcon = nullptr;
        }
        m_nid.uFlags = NIF_ICON;
        m_nid.hIcon = LoadIconW(m_hInstance ? m_hInstance : GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON));
        Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    } else if (mode == TrayDisplayMode::DynamicCountdown) {
        if (m_animTimerId) {
            KillTimer(m_hwnd, IDT_TRAY_ANIMATION);
            m_animTimerId = 0;
        }
        HICON hIcon = DynamicTrayIcon::Instance().CreateCountdownIcon(
            m_lastRemainingSec, m_lastTotalSec, m_lastState, ThemeManager::Instance().IsEffectiveTaskbarDark()
        );
        if (hIcon) {
            m_nid.uFlags = NIF_ICON;
            m_nid.hIcon = hIcon;
            Shell_NotifyIconW(NIM_MODIFY, &m_nid);
            if (m_hDynamicIcon) DestroyIcon(m_hDynamicIcon);
            m_hDynamicIcon = hIcon;
        }
    } else if (mode == TrayDisplayMode::RunCatHealth) {
        int interval = 150;
        if (m_lastState == AppState::Resting) interval = 350;
        else if (m_lastRemainingSec <= 300) interval = 120;
        else if (m_lastState == AppState::Standing) interval = 140;

        m_animTimerId = SetTimer(m_hwnd, IDT_TRAY_ANIMATION, interval, nullptr);
        OnAnimationTick();
    } else if (mode == TrayDisplayMode::RunCatCpu) {
        m_animTimerId = SetTimer(m_hwnd, IDT_TRAY_ANIMATION, 150, nullptr);
        OnAnimationTick();
    }
}

void TrayWindow::UpdateDynamicIcon(AppState state, int remainingSec, int totalSec, const std::wstring& tooltip) {
    m_lastState = state;
    m_lastRemainingSec = remainingSec;
    m_lastTotalSec = totalSec;

    auto mode = ConfigManager::Instance().GetConfig().trayDisplayMode;
    if (mode == TrayDisplayMode::DynamicCountdown) {
        HICON hIcon = DynamicTrayIcon::Instance().CreateCountdownIcon(
            remainingSec, totalSec, state, ThemeManager::Instance().IsEffectiveTaskbarDark()
        );
        if (hIcon) {
            m_nid.uFlags = NIF_ICON;
            m_nid.hIcon = hIcon;
            if (!tooltip.empty()) {
                wcsncpy_s(m_nid.szTip, tooltip.c_str(), _TRUNCATE);
                m_nid.uFlags |= NIF_TIP;
            }
            Shell_NotifyIconW(NIM_MODIFY, &m_nid);
            if (m_hDynamicIcon) DestroyIcon(m_hDynamicIcon);
            m_hDynamicIcon = hIcon;
        }
    } else {
        if (!tooltip.empty()) {
            UpdateTooltip(tooltip);
        }
        if (mode == TrayDisplayMode::RunCatHealth) {
            int targetInterval = 150;
            if (state == AppState::Resting) targetInterval = 350;
            else if (remainingSec <= 300) targetInterval = 120;
            else if (state == AppState::Standing) targetInterval = 140;

            if (!m_animTimerId) {
                m_animTimerId = SetTimer(m_hwnd, IDT_TRAY_ANIMATION, targetInterval, nullptr);
            }
        } else if (mode == TrayDisplayMode::RunCatCpu) {
            if (!m_animTimerId) {
                m_animTimerId = SetTimer(m_hwnd, IDT_TRAY_ANIMATION, 150, nullptr);
            }
        }
    }
}

void TrayWindow::OnAnimationTick() {
    if (!m_added || !m_hwnd) return;
    auto mode = ConfigManager::Instance().GetConfig().trayDisplayMode;
    if (mode != TrayDisplayMode::RunCatHealth && mode != TrayDisplayMode::RunCatCpu) return;

    m_animFrame++;
    bool isDark = ThemeManager::Instance().IsEffectiveTaskbarDark();

    if (mode == TrayDisplayMode::RunCatHealth) {
        HICON hIcon = DynamicTrayIcon::Instance().CreateRunCatIcon(m_animFrame, m_lastState, isDark, 0.0f);
        if (hIcon) {
            m_nid.uFlags = NIF_ICON;
            m_nid.hIcon = hIcon;
            Shell_NotifyIconW(NIM_MODIFY, &m_nid);
            if (m_hDynamicIcon) DestroyIcon(m_hDynamicIcon);
            m_hDynamicIcon = hIcon;
        }
    } else if (mode == TrayDisplayMode::RunCatCpu) {
        float cpu = DynamicTrayIcon::GetCpuUsage();
        HICON hIcon = DynamicTrayIcon::Instance().CreateRunCatIcon(m_animFrame, m_lastState, isDark, cpu);
        if (hIcon) {
            m_nid.uFlags = NIF_ICON;
            m_nid.hIcon = hIcon;
            Shell_NotifyIconW(NIM_MODIFY, &m_nid);
            if (m_hDynamicIcon) DestroyIcon(m_hDynamicIcon);
            m_hDynamicIcon = hIcon;
        }
        int nextInterval = static_cast<int>(200.0f - (cpu / 100.0f) * 100.0f);
        nextInterval = (std::max)(100, (std::min)(220, nextInterval));
        SetTimer(m_hwnd, IDT_TRAY_ANIMATION, nextInterval, nullptr);
    }
}

void TrayWindow::UpdateTooltip(const std::wstring& text) {
    if (!m_added) return;
    if (wcscmp(m_nid.szTip, text.c_str()) == 0) return; // 文本未变时不触发无意义的 IPC 跨进程通知
    wcsncpy_s(m_nid.szTip, text.c_str(), _TRUNCATE);
    m_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void TrayWindow::ShowBalloon(const std::wstring& title, const std::wstring& msg, DWORD flags) {
    if (!m_added) return;
    m_nid.uFlags = NIF_INFO;
    wcsncpy_s(m_nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(m_nid.szInfo, msg.c_str(), _TRUNCATE);
    m_nid.dwInfoFlags = flags;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void TrayWindow::MeasureMenuItem(MEASUREITEMSTRUCT* pMis) {
    if (!pMis) return;
    auto* item = reinterpret_cast<TrayMenuItem*>(pMis->itemData);
    if (!item) return;

    float scale = GetTrayDpiScale();

    if (item->isSeparator) {
        pMis->itemWidth = static_cast<UINT>(196 * scale);
        pMis->itemHeight = static_cast<UINT>(8 * scale);
    } else {
        pMis->itemWidth = static_cast<UINT>(196 * scale);
        pMis->itemHeight = static_cast<UINT>(32 * scale);
    }
}

void TrayWindow::DrawMenuItem(DRAWITEMSTRUCT* pDis, bool isDark) {
    if (!pDis) return;
    auto* item = reinterpret_cast<TrayMenuItem*>(pDis->itemData);
    if (!item) return;

    float scale = GetTrayDpiScale();
    HDC hdc = pDis->hDC;
    RECT rc = pDis->rcItem;

    if (!m_hMenuDarkBgBrush) m_hMenuDarkBgBrush = CreateSolidBrush(RGB(28, 30, 36));
    if (!m_hMenuLightBgBrush) m_hMenuLightBgBrush = CreateSolidBrush(RGB(248, 249, 251));

    // 1. 擦除背景
    FillRect(hdc, &rc, isDark ? m_hMenuDarkBgBrush : m_hMenuLightBgBrush);

    if (item->isSeparator) {
        // 分割线
        HPEN hPen = CreatePen(PS_SOLID, 1, isDark ? RGB(52, 58, 70) : RGB(225, 230, 238));
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        int midY = rc.top + (rc.bottom - rc.top) / 2;
        MoveToEx(hdc, rc.left + static_cast<int>(12 * scale), midY, nullptr);
        LineTo(hdc, rc.right - static_cast<int>(12 * scale), midY);
        SelectObject(hdc, oldPen);
        DeleteObject(hPen);
        return;
    }

    bool isSelected = (pDis->itemState & ODS_SELECTED);

    // 2. 悬停高光圆角条
    if (isSelected) {
        HBRUSH hHoverBrush = CreateSolidBrush(isDark ? RGB(45, 52, 65) : RGB(232, 238, 248));
        HPEN hHoverPen = CreatePen(PS_SOLID, 1, isDark ? RGB(65, 75, 92) : RGB(210, 220, 235));
        HGDIOBJ oldBrush = SelectObject(hdc, hHoverBrush);
        HGDIOBJ oldPen = SelectObject(hdc, hHoverPen);
        int padX = static_cast<int>(5 * scale);
        int padY = static_cast<int>(2 * scale);
        int radius = static_cast<int>(5 * scale);
        RoundRect(hdc, rc.left + padX, rc.top + padY, rc.right - padX, rc.bottom - padY, radius, radius);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(hHoverPen);
        DeleteObject(hHoverBrush);
    }

    // 3. 激活状态翡翠绿圆点 ●
    if (item->isChecked) {
        HBRUSH hDotBrush = CreateSolidBrush(RGB(16, 185, 129));
        HPEN hDotPen = CreatePen(PS_SOLID, 1, RGB(16, 185, 129));
        HGDIOBJ oldBrush = SelectObject(hdc, hDotBrush);
        HGDIOBJ oldPen = SelectObject(hdc, hDotPen);
        int dotX = rc.left + static_cast<int>(15 * scale);
        int dotY = rc.top + (rc.bottom - rc.top) / 2;
        int r = static_cast<int>(3.5f * scale);
        Ellipse(hdc, dotX - r, dotY - r, dotX + r, dotY + r);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(hDotPen);
        DeleteObject(hDotBrush);
    }

    // 4. 文字绘制 (复用已缓存的 ClearType 微软雅黑字体，零瞬时分配)
    bool isDefault = (pDis->itemState & ODS_DEFAULT) != 0 || (item->id == IDM_TRAY_SETTINGS);
    HFONT hFont = isDefault ? (m_hMenuBoldFont ? m_hMenuBoldFont : m_hMenuFont) : m_hMenuFont;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, isDark ? (isSelected ? RGB(255, 255, 255) : RGB(226, 232, 240))
                             : (isSelected ? RGB(15, 23, 42) : RGB(51, 65, 85)));
    HGDIOBJ oldFont = hFont ? SelectObject(hdc, hFont) : nullptr;

    RECT textRc = rc;
    textRc.left += static_cast<int>(28 * scale);
    textRc.right -= static_cast<int>(12 * scale);
    DrawTextW(hdc, item->text.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (oldFont) SelectObject(hdc, oldFont);
}

void TrayWindow::ShowContextMenu(const POINT* pPt) {
    ThemeManager::Instance().ApplyThemeToWindow(m_hwnd);

    // 动态维护/缓存匹配当前 DPI 的菜单字体
    float scale = GetTrayDpiScale();
    if (!m_hMenuFont || !m_hMenuBoldFont || std::abs(m_lastMenuDpiScale - scale) > 0.01f) {
        if (m_hMenuFont) DeleteObject(m_hMenuFont);
        if (m_hMenuBoldFont) DeleteObject(m_hMenuBoldFont);
        int fontHeight = static_cast<int>(-14.0f * scale);
        m_hMenuFont = CreateFontW(
            fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI"
        );
        m_hMenuBoldFont = CreateFontW(
            fontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI"
        );
        m_lastMenuDpiScale = scale;
    }

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    bool isPaused = (g_pStateMachine && g_pStateMachine->GetState() == AppState::Paused);
    AppState curState = g_pStateMachine ? g_pStateMachine->GetState() : AppState::Working;

    auto& cfg = ConfigManager::Instance().GetConfig();
    int activePresetId = AppConstants::FindMatchingPresetId(cfg.workMinutes, cfg.standMinutes, cfg.restSeconds);

    m_menuItems = {
        // 1. 状态与操练切换组 (带 ● 翡翠绿圆点)
        { IDM_TRAY_START_WORK, L"坐姿办公", false, curState == AppState::Working },
        { IDM_TRAY_START_STAND, L"站立办公", false, curState == AppState::Standing },
        { IDM_TRAY_START_REST, L"工间操放松", false, curState == AppState::Resting },
        { 0, L"", true, false },

        // 2. 实时流转控制
        { IDM_TRAY_PAUSE_RESUME, isPaused ? L"继续计时" : L"暂停计时", false, false },
        { IDM_TRAY_POSTPONE_5M, L"延后 5 分钟", false, false },
        { IDM_TRAY_SKIP, L"跳过当前阶段", false, false },
        { 0, L"", true, false }
    };

    // 3. 快速周期预设 (从单一事实源 PRESETS 自动构建)
    for (const auto& p : AppConstants::PRESETS) {
        m_menuItems.push_back({ p.menuCmdId, p.menuLabel, false, (p.id == activePresetId) });
    }

    // 4. 设置与退出
    m_menuItems.push_back({ 0, L"", true, false });
    m_menuItems.push_back({ IDM_TRAY_SETTINGS, L"设置中心...", false, false });
    m_menuItems.push_back({ 0, L"", true, false });
    m_menuItems.push_back({ IDM_TRAY_EXIT, std::wstring(L"退出") + AppConstants::Identity::DISPLAY_NAME, false, false });

    for (size_t i = 0; i < m_menuItems.size(); ++i) {
        auto& item = m_menuItems[i];
        if (item.isSeparator) {
            AppendMenuW(hMenu, MF_OWNERDRAW | MF_SEPARATOR, 0, reinterpret_cast<LPCWSTR>(&m_menuItems[i]));
        } else {
            AppendMenuW(hMenu, MF_OWNERDRAW, item.id, reinterpret_cast<LPCWSTR>(&m_menuItems[i]));
        }
    }

    // 设置“设置中心”为缺省菜单项 (与托盘双击行为严格统一)
    SetMenuDefaultItem(hMenu, IDM_TRAY_SETTINGS, FALSE);

    POINT pt;
    if (pPt) {
        pt = *pPt;
    } else {
        GetCursorPos(&pt);
    }

    SetForegroundWindow(m_hwnd);
    TrackPopupMenuEx(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, m_hwnd, nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK TrayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    try {
        // 1. 响应 Explorer 重启自动恢复托盘图标
        if (s_uTaskbarCreated != 0 && msg == s_uTaskbarCreated) {
            TrayWindow::Instance().m_added = Shell_NotifyIconW(NIM_ADD, &TrayWindow::Instance().m_nid);
            TrayWindow::Instance().RefreshTrayDisplayMode();
            return 0;
        }

        switch (msg) {
            case WM_WTSSESSION_CHANGE: {
                if (wParam == WTS_SESSION_LOCK) {
                    if (g_pStateMachine) g_pStateMachine->OnSystemSuspendOrLock();
                    // 锁屏状态下智能挂起托盘与悬浮窗动画定时器消除后台空转功耗
                    FloatingWindow::Instance().StopAnimation();
                    if (TrayWindow::Instance().m_animTimerId) {
                        KillTimer(hwnd, IDT_TRAY_ANIMATION);
                        TrayWindow::Instance().m_animTimerId = 0;
                    }
                } else if (wParam == WTS_SESSION_UNLOCK) {
                    if (g_pStateMachine) g_pStateMachine->OnSystemResumeOrUnlock();
                    TrayWindow::Instance().RefreshTrayDisplayMode();
                }
                return 0;
            }

            case WM_POWERBROADCAST: {
                if (wParam == PBT_APMSUSPEND) {
                    if (g_pStateMachine) g_pStateMachine->OnSystemSuspendOrLock();
                    // 系统休眠期间智能挂起托盘与悬浮窗动画定时器
                    FloatingWindow::Instance().StopAnimation();
                    if (TrayWindow::Instance().m_animTimerId) {
                        KillTimer(hwnd, IDT_TRAY_ANIMATION);
                        TrayWindow::Instance().m_animTimerId = 0;
                    }
                } else if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
                    if (g_pStateMachine) g_pStateMachine->OnSystemResumeOrUnlock();
                    TrayWindow::Instance().RefreshTrayDisplayMode();
                }
                return TRUE;
            }

            case WM_TRAY_NOTIFY:
                if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
                    SettingsWindow::Instance().Show(GetModuleHandleW(nullptr));
                } else if (LOWORD(lParam) == WM_RBUTTONUP) {
                    TrayWindow::Instance().ShowContextMenu();
                } else if (LOWORD(lParam) == WM_LBUTTONUP) {
                    // 左键单击：唤醒并展示桌面微件/悬浮窗（严格遵循用户 alwaysTopMost 配置）
                    FloatingWindow::Instance().Show(true);
                    HWND hFloat = FloatingWindow::Instance().GetHwnd();
                    if (hFloat && IsWindow(hFloat)) {
                        SetForegroundWindow(hFloat);
                    }
                }
                return 0;

            case WM_MEASUREITEM: {
                auto* pMis = reinterpret_cast<LPMEASUREITEMSTRUCT>(lParam);
                if (pMis && pMis->CtlType == ODT_MENU) {
                    TrayWindow::Instance().MeasureMenuItem(pMis);
                    return TRUE;
                }
                break;
            }

            case WM_DRAWITEM: {
                auto* pDis = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
                if (pDis && pDis->CtlType == ODT_MENU) {
                    bool isDark = ThemeManager::Instance().IsEffectiveDark();
                    TrayWindow::Instance().DrawMenuItem(pDis, isDark);
                    return TRUE;
                }
                break;
            }

            case WM_TIMER:
                if (wParam == IDT_TRAY_ANIMATION) {
                    TrayWindow::Instance().OnAnimationTick();
                    return 0;
                }
                break;

            case WM_SETTINGCHANGE:
            case WM_THEMECHANGED:
                TrayWindow::Instance().RefreshTrayDisplayMode();
                return 0;

            case WM_COMMAND: {
                WORD id = LOWORD(wParam);
                if (!g_pStateMachine) return 0;

                switch (id) {
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
                    case IDM_TRAY_PRESET_2:
                    case IDM_TRAY_PRESET_3:
                    case IDM_TRAY_PRESET_4: {
                        if (const auto* pPreset = AppConstants::GetPresetByMenuCmd(id)) {
                            ConfigManager::Instance().ApplyPreset(pPreset->workMinutes, pPreset->standMinutes, pPreset->restSeconds);
                            ConfigManager::Instance().Save();
                            g_pStateMachine->SetConfig(ConfigManager::Instance().GetConfig());
                            g_pStateMachine->StartWork();
                            FloatingWindow::Instance().OnConfigChanged();
                            TrayWindow::Instance().RefreshTrayDisplayMode();
                            if (SettingsWindow::Instance().GetHwnd()) {
                                SettingsWindow::Instance().LoadConfigToUI();
                            }
                        }
                        break;
                    }
                    case IDM_TRAY_SETTINGS:
                        SettingsWindow::Instance().Show(GetModuleHandleW(nullptr));
                        break;
                    case IDM_TRAY_EXIT:
                        PostQuitMessage(0);
                        break;
                }
                return 0;
            }
        }
    } catch (...) {
        // C++ 异常屏障
#ifdef _DEBUG
        OutputDebugStringW(L"[SitStandReminder] Exception caught in TrayWindow::WndProc\n");
#endif
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
