#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <functional>
#include "../core/StateMachine.hpp"

#define WM_TRAY_NOTIFY (WM_USER + 101)

// 菜单命令 ID
#define IDM_TRAY_START_WORK      2001
#define IDM_TRAY_START_STAND     2002
#define IDM_TRAY_START_REST      2003
#define IDM_TRAY_PAUSE_RESUME    2004
#define IDM_TRAY_POSTPONE_5M     2005
#define IDM_TRAY_SKIP            2006
#define IDM_TRAY_PRESET_1        2011 // 45m 坐 / 15m 站
#define IDM_TRAY_PRESET_2        2012 // 50m 坐 / 10m 站
#define IDM_TRAY_PRESET_3        2013 // 25m 番茄工作法
#define IDM_TRAY_PRESET_4        2014 // 60m 深度办公
#define IDM_TRAY_SETTINGS        2020
#define IDM_TRAY_EXIT            2021

struct TrayMenuItem {
    UINT id;
    std::wstring text;
    bool isSeparator;
    bool isChecked;
};

class TrayWindow {
public:
    static TrayWindow& Instance() {
        static TrayWindow instance;
        return instance;
    }

    bool Create(HINSTANCE hInstance);
    void Destroy();

    void UpdateTooltip(const std::wstring& text);
    void ShowBalloon(const std::wstring& title, const std::wstring& msg, DWORD flags = NIIF_INFO);
    void ShowContextMenu(const POINT* pPt = nullptr);
    void UpdateDynamicIcon(AppState state, int remainingSec, int totalSec, const std::wstring& tooltip = L"");
    void RefreshTrayDisplayMode();

    HWND GetHwnd() const { return m_hwnd; }

private:
    TrayWindow() = default;
    ~TrayWindow() { Destroy(); }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void MeasureMenuItem(MEASUREITEMSTRUCT* pMis);
    void DrawMenuItem(DRAWITEMSTRUCT* pDis, bool isDark);
    void OnAnimationTick();

    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    NOTIFYICONDATAW m_nid = { 0 };
    bool m_added = false;

    HICON m_hDynamicIcon = nullptr;
    int m_animFrame = 0;
    AppState m_lastState = AppState::Working;
    int m_lastRemainingSec = 0;
    int m_lastTotalSec = 0;
    UINT_PTR m_animTimerId = 0;

    std::vector<TrayMenuItem> m_menuItems;
    HFONT m_hMenuFont = nullptr;
    HFONT m_hMenuBoldFont = nullptr;
    float m_lastMenuDpiScale = 0.0f;
    HBRUSH m_hMenuDarkBgBrush = nullptr;
    HBRUSH m_hMenuLightBgBrush = nullptr;
};

