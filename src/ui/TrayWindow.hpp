#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#define WM_TRAY_NOTIFY (WM_USER + 101)
#else
#include <X11/Xlib.h>
#include "../graphics/D2DCompat.hpp"
#include "../graphics/linux/LinuxCanvas.hpp"
#endif

#include <string>
#include <functional>
#include <vector>
#include <memory>
#include "../core/StateMachine.hpp"

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
    uint32_t id;
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

#ifdef _WIN32
    bool Create(HINSTANCE hInstance);
    HWND GetHwnd() const { return m_hwnd; }
    void ShowContextMenu(const POINT* pPt = nullptr);
#else
    bool Create(void* hInstance = nullptr);
    Window GetHwnd() const { return m_trayWindow; }
    void ShowContextMenu(const void* pPt = nullptr);
#endif

    void Destroy();
    void UpdateTooltip(const std::wstring& text);
#ifdef _WIN32
    void ShowBalloon(const std::wstring& title, const std::wstring& msg, DWORD flags = 0);
#else
    void ShowBalloon(const std::wstring& title, const std::wstring& msg, uint32_t flags = 0);
#endif
    void UpdateDynamicIcon(AppState state, int remainingSec, int totalSec, const std::wstring& tooltip = L"");
    void RefreshTrayDisplayMode();
    bool IsMenuVisible() const {
#ifdef _WIN32
        return false;
#else
        return m_menuVisible;
#endif
    }
#ifndef _WIN32
    void HideMenu();
    int GetMenuItemAt(int my) const;
    D2D1_RECT_F GetMenuItemRect(size_t index) const;
    const std::vector<TrayMenuItem>& GetMenuItems() const { return m_menuItems; }
    void ExecuteCommand(uint32_t cmdId);
#endif

private:
    TrayWindow() = default;
    ~TrayWindow() { Destroy(); }

#ifdef _WIN32
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

#else
    // Linux X11 托盘与弹出菜单
    void HandleTrayEvent(const XEvent& ev);
    void HandleMenuEvent(const XEvent& ev);
    void RenderMenu();
    void RenderTray();
    void OnAnimationTick();

    Window m_trayWindow = 0;
    Window m_menuWindow = 0;
    GC m_trayGC = nullptr;
    GC m_menuGC = nullptr;
    std::unique_ptr<LinuxRenderTarget> m_trayRT;
    std::unique_ptr<LinuxRenderTarget> m_menuRT;

    bool m_menuVisible = false;
    int m_menuW = 220;
    int m_menuH = 340;
    int m_menuHoverIndex = -1;

    AppState m_lastState = AppState::Working;
    int m_lastRemainingSec = 0;
    int m_lastTotalSec = 0;
    int m_animFrame = 0;
    std::wstring m_tooltip;
    std::vector<TrayMenuItem> m_menuItems;
#endif
};
