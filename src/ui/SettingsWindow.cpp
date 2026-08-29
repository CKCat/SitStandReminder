#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "SettingsWindow.hpp"
#include "FloatingWindow.hpp"
#include "TrayWindow.hpp"
#include "resource.h"
#include "../platform/ThemeManager.hpp"
#include "../graphics/D2DContext.hpp"
#include "../core/StateMachine.hpp"
#include "../core/AppConstants.hpp"
#include <cwchar>
#include <algorithm>

extern StateMachine* g_pStateMachine;

#define IDC_BTN_PRESET1   3001
#define IDC_BTN_PRESET2   3002
#define IDC_BTN_PRESET3   3003
#define IDC_BTN_PRESET4   3004
#define IDC_BTN_SAVE      3005
#define IDC_BTN_CANCEL    3006
#define IDC_COMBO_MODE    3007
#define IDC_COMBO_THEME   3008
#define IDC_COMBO_MASCOT  3009
#define IDC_COMBO_TRAY    3010

#define IDC_CHK_STAND       3011
#define IDC_CHK_BLOCK       3012
#define IDC_CHK_STRONG      3013
#define IDC_CHK_TOP         3014
#define IDC_CHK_AUTOSTART   3015
#define IDC_CHK_EDGEDOCK    3016

bool SettingsWindow::Create(HINSTANCE hInstance) {
    if (m_hwnd && IsWindow(m_hwnd)) return true;
    m_hInstance = hInstance ? hInstance : GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = AppConstants::Identity::CLASS_SETTINGS;
    wc.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    // GDI 资源初始化 (类内常驻缓存，彻底消除 WM_DRAWITEM 每次系统调用分配开销)
    if (!m_hDarkBgBrush) m_hDarkBgBrush = CreateSolidBrush(RGB(28, 30, 36));
    if (!m_hDarkEditBrush) m_hDarkEditBrush = CreateSolidBrush(RGB(40, 44, 54));
    if (!m_hLightBgBrush) m_hLightBgBrush = CreateSolidBrush(RGB(248, 249, 251));
    if (!m_hLightEditBrush) m_hLightEditBrush = CreateSolidBrush(RGB(255, 255, 255));
    if (!m_hAccentBrush) m_hAccentBrush = CreateSolidBrush(RGB(16, 185, 129));
    if (!m_hDividerPenLight) m_hDividerPenLight = CreatePen(PS_SOLID, 1, RGB(229, 231, 235));
    if (!m_hDividerPenDark) m_hDividerPenDark = CreatePen(PS_SOLID, 1, RGB(48, 52, 64));
    if (!m_hAccentPen) m_hAccentPen = CreatePen(PS_SOLID, 1, RGB(16, 185, 129));
    if (!m_hAccentThickPen) m_hAccentThickPen = CreatePen(PS_SOLID, 2, RGB(16, 185, 129));
    if (!m_hWhiteCheckPen) m_hWhiteCheckPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));

    int baseW = 500;
    int baseH = 590;

    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int posX = (workArea.right - workArea.left - baseW) / 2;
    int posY = (workArea.bottom - workArea.top - baseH) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        wc.lpszClassName,
        AppConstants::Identity::TITLE_SETTINGS,
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        posX, posY, baseW, baseH,
        nullptr, nullptr, m_hInstance, this
    );

    if (!m_hwnd) return false;

    ThemeManager::Instance().ApplyThemeToWindow(m_hwnd);
    CreateControls(m_hwnd);

    float dpiScale = D2DContext::GetWindowDpiScale(m_hwnd);
    UpdateLayout(dpiScale);
    LoadConfigToUI();
    return true;
}

void SettingsWindow::Show(HINSTANCE hInstance) {
    if (!m_hwnd || !IsWindow(m_hwnd)) {
        m_hwnd = nullptr;
        Create(hInstance ? hInstance : GetModuleHandleW(nullptr));
    }
    if (m_hwnd && IsWindow(m_hwnd)) {
        ThemeManager::Instance().ApplyThemeToWindow(m_hwnd);
        float dpiScale = D2DContext::GetWindowDpiScale(m_hwnd);
        UpdateLayout(dpiScale);
        LoadConfigToUI();
        ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void SettingsWindow::OnThemeChanged() {
    if (!m_hwnd || !IsWindow(m_hwnd) || m_isUpdatingTheme) return;
    m_isUpdatingTheme = true;

    ThemeManager::Instance().ApplyThemeToWindow(m_hwnd);

    bool isDark = ThemeManager::Instance().IsEffectiveDark();
    const wchar_t* themeStr = isDark ? L"DarkMode_CFD" : L"Explorer";
    if (m_hModeCombo) SetWindowTheme(m_hModeCombo, themeStr, nullptr);
    if (m_hThemeCombo) SetWindowTheme(m_hThemeCombo, themeStr, nullptr);
    if (m_hMascotCombo) SetWindowTheme(m_hMascotCombo, themeStr, nullptr);
    if (m_hTrayCombo) SetWindowTheme(m_hTrayCombo, themeStr, nullptr);

    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    RedrawWindow(m_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME | RDW_UPDATENOW);

    m_isUpdatingTheme = false;
}

void SettingsWindow::Close() {
    if (m_hFont) { DeleteObject(m_hFont); m_hFont = nullptr; }
    if (m_hBoldFont) { DeleteObject(m_hBoldFont); m_hBoldFont = nullptr; }
    if (m_hSectionFont) { DeleteObject(m_hSectionFont); m_hSectionFont = nullptr; }
    if (m_hDarkBgBrush) { DeleteObject(m_hDarkBgBrush); m_hDarkBgBrush = nullptr; }
    if (m_hDarkEditBrush) { DeleteObject(m_hDarkEditBrush); m_hDarkEditBrush = nullptr; }
    if (m_hLightBgBrush) { DeleteObject(m_hLightBgBrush); m_hLightBgBrush = nullptr; }
    if (m_hLightEditBrush) { DeleteObject(m_hLightEditBrush); m_hLightEditBrush = nullptr; }
    if (m_hAccentBrush) { DeleteObject(m_hAccentBrush); m_hAccentBrush = nullptr; }
    if (m_hDividerPenLight) { DeleteObject(m_hDividerPenLight); m_hDividerPenLight = nullptr; }
    if (m_hDividerPenDark) { DeleteObject(m_hDividerPenDark); m_hDividerPenDark = nullptr; }
    if (m_hAccentPen) { DeleteObject(m_hAccentPen); m_hAccentPen = nullptr; }
    if (m_hAccentThickPen) { DeleteObject(m_hAccentThickPen); m_hAccentThickPen = nullptr; }
    if (m_hWhiteCheckPen) { DeleteObject(m_hWhiteCheckPen); m_hWhiteCheckPen = nullptr; }

    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    // 防御性清空所有子控件与浮层句柄，避免多轮生命周期中的野句柄引用
    m_hTooltip = nullptr;
    m_hGroupPreset = m_hGroupCustom = m_hGroupTheme = m_hGroupOptions = nullptr;
    m_hLblWork = m_hLblStand = m_hLblRest = nullptr;
    m_hLblWorkUnit = m_hLblStandUnit = m_hLblRestUnit = nullptr;
    m_hLblMode = m_hLblTheme = m_hLblMascot = m_hLblTray = nullptr;
    m_hWorkMinEdit = m_hStandMinEdit = m_hRestSecEdit = nullptr;
    m_hModeCombo = m_hThemeCombo = m_hMascotCombo = m_hTrayCombo = nullptr;
    m_hChkStand = m_hChkBlock = m_hChkStrong = m_hChkTop = m_hChkAutoStart = m_hChkEdgeDock = nullptr;
    m_hBtnPreset1 = m_hBtnPreset2 = m_hBtnPreset3 = m_hBtnPreset4 = nullptr;
    m_hBtnSave = m_hBtnCancel = nullptr;
}

void SettingsWindow::CreateControls(HWND hwnd) {
    // 1. 预设分组 (基于 AppConstants::PRESETS 单一事实源)
    m_hGroupPreset = CreateWindowW(L"STATIC", L"⚡ 科学办公周期预设", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);
    m_hBtnPreset1 = CreateWindowW(L"BUTTON", AppConstants::PRESETS[0].buttonLabel, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BTN_PRESET1), m_hInstance, nullptr);
    m_hBtnPreset2 = CreateWindowW(L"BUTTON", AppConstants::PRESETS[1].buttonLabel, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BTN_PRESET2), m_hInstance, nullptr);
    m_hBtnPreset3 = CreateWindowW(L"BUTTON", AppConstants::PRESETS[2].buttonLabel, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BTN_PRESET3), m_hInstance, nullptr);
    m_hBtnPreset4 = CreateWindowW(L"BUTTON", AppConstants::PRESETS[3].buttonLabel, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BTN_PRESET4), m_hInstance, nullptr);

    // 2. 自定义时长
    m_hGroupCustom = CreateWindowW(L"STATIC", L"⏱️ 自定义时长配置", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);
    
    m_hLblWork = CreateWindowW(L"STATIC", L"坐姿工作时长:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);
    m_hWorkMinEdit = CreateWindowExW(0, L"EDIT", L"45", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL | ES_CENTER, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);
    m_hLblWorkUnit = CreateWindowW(L"STATIC", L"分钟", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);

    m_hLblStand = CreateWindowW(L"STATIC", L"站立办公时长:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);
    m_hStandMinEdit = CreateWindowExW(0, L"EDIT", L"15", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL | ES_CENTER, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);
    m_hLblStandUnit = CreateWindowW(L"STATIC", L"分钟", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);

    m_hLblRest = CreateWindowW(L"STATIC", L"全屏休息时长:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);
    m_hRestSecEdit = CreateWindowExW(0, L"EDIT", L"60", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL | ES_CENTER, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);
    m_hLblRestUnit = CreateWindowW(L"STATIC", L"秒", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);

    // 设置输入框内边距
    SendMessageW(m_hWorkMinEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(8, 8));
    SendMessageW(m_hStandMinEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(8, 8));
    SendMessageW(m_hRestSecEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(8, 8));

    // 3. 模式与外观
    m_hGroupTheme = CreateWindowW(L"STATIC", L"🧘 提醒模式与伴侣外观", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);
    m_hLblMode = CreateWindowW(L"STATIC", L"工间休息操类型:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);
    m_hModeCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_COMBO_MODE), m_hInstance, nullptr);
    SendMessageW(m_hModeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"综合工间操"));
    SendMessageW(m_hModeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"科学颈椎操"));
    SendMessageW(m_hModeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"20-20-20 护眼操"));
    SendMessageW(m_hModeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"极简放空休息"));

    m_hLblTheme = CreateWindowW(L"STATIC", L"主题色彩偏好:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);
    m_hThemeCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_COMBO_THEME), m_hInstance, nullptr);
    SendMessageW(m_hThemeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"跟随 Windows 主题"));
    SendMessageW(m_hThemeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"浅色模式"));
    SendMessageW(m_hThemeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"深色模式"));

    m_hLblMascot = CreateWindowW(L"STATIC", L"桌面微件与形象:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);
    m_hMascotCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_COMBO_MASCOT), m_hInstance, nullptr);
    SendMessageW(m_hMascotCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"极简商务"));
    SendMessageW(m_hMascotCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"佛系水豚"));
    SendMessageW(m_hMascotCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"灵动像素猫"));
    SendMessageW(m_hMascotCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"赛博小助手"));

    m_hLblTray = CreateWindowW(L"STATIC", L"系统托盘显示风格:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);
    m_hTrayCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_COMBO_TRAY), m_hInstance, nullptr);
    SendMessageW(m_hTrayCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"经典静态图标"));
    SendMessageW(m_hTrayCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"动态数字倒计时"));
    SendMessageW(m_hTrayCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"灵动小猫 RunCat (状态感应)"));
    SendMessageW(m_hTrayCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"动力小猫 RunCat (CPU占用率)"));

    // 设置下拉控件行高 (Selection = 26px, Dropdown List = 24px)
    SendMessageW(m_hModeCombo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), 26);
    SendMessageW(m_hModeCombo, CB_SETITEMHEIGHT, 0, 24);
    SendMessageW(m_hThemeCombo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), 26);
    SendMessageW(m_hThemeCombo, CB_SETITEMHEIGHT, 0, 24);
    SendMessageW(m_hMascotCombo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), 26);
    SendMessageW(m_hMascotCombo, CB_SETITEMHEIGHT, 0, 24);
    SendMessageW(m_hTrayCombo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), 26);
    SendMessageW(m_hTrayCombo, CB_SETITEMHEIGHT, 0, 24);

    // 4. 高级选项 (双列精简排版，文案清爽易读)
    m_hGroupOptions = CreateWindowW(L"STATIC", L"🛡️ 行为与安全选项", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);
    m_hChkStand = CreateWindowW(L"BUTTON", L"启用站立工作循环", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_CHK_STAND), m_hInstance, nullptr);
    m_hChkBlock = CreateWindowW(L"BUTTON", L"休息时拦截键盘输入", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_CHK_BLOCK), m_hInstance, nullptr);
    m_hChkStrong = CreateWindowW(L"BUTTON", L"临界30秒红光强提醒", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_CHK_STRONG), m_hInstance, nullptr);
    m_hChkTop = CreateWindowW(L"BUTTON", L"悬浮窗总在最前端", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_CHK_TOP), m_hInstance, nullptr);
    m_hChkAutoStart = CreateWindowW(L"BUTTON", L"开机自动在托盘启动", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_CHK_AUTOSTART), m_hInstance, nullptr);
    m_hChkEdgeDock = CreateWindowW(L"BUTTON", L"开启屏幕边缘吸附折叠", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_CHK_EDGEDOCK), m_hInstance, nullptr);

    // 5. 底部操作按钮
    m_hBtnSave = CreateWindowW(L"BUTTON", L"✔ 保存并应用", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BTN_SAVE), m_hInstance, nullptr);
    m_hBtnCancel = CreateWindowW(L"BUTTON", L"关闭", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BTN_CANCEL), m_hInstance, nullptr);

    // 6. 安装悬浮气泡提示 (Tooltips)
    SetupTooltips();

    // 7. 初始化控件视觉样式主题
    bool isDark = ThemeManager::Instance().IsEffectiveDark();
    const wchar_t* themeStr = isDark ? L"DarkMode_CFD" : L"Explorer";
    SetWindowTheme(m_hModeCombo, themeStr, nullptr);
    SetWindowTheme(m_hThemeCombo, themeStr, nullptr);
    SetWindowTheme(m_hMascotCombo, themeStr, nullptr);
    SetWindowTheme(m_hTrayCombo, themeStr, nullptr);
}

void SettingsWindow::UpdateLayout(float dpiScale) {
    if (!m_hwnd) return;

    // Res-1 修复: 先创建新字体，保留旧句柄引用，待 WM_SETFONT 应用后再安全删除
    HFONT hOldFont = m_hFont;
    HFONT hOldBold = m_hBoldFont;
    HFONT hOldSection = m_hSectionFont;

    int fontH = static_cast<int>(-13.5f * dpiScale);
    int secH = static_cast<int>(-15.0f * dpiScale);

    m_hFont = CreateFontW(fontH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    m_hBoldFont = CreateFontW(fontH, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    m_hSectionFont = CreateFontW(secH, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");

    auto S = [dpiScale](int val) { return static_cast<int>(val * dpiScale); };

    // 1. 精确计算含标题栏的外框尺寸并绝对居中屏幕可用工作区
    int clientW = S(496);
    int clientH = S(558);

    RECT winRc = { 0, 0, clientW, clientH };
    AdjustWindowRectEx(&winRc, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT);
    int totalW = winRc.right - winRc.left;
    int totalH = winRc.bottom - winRc.top;

    HMONITOR hMon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfoW(hMon, &mi);
    RECT workArea = mi.rcWork;

    int posX = workArea.left + (workArea.right - workArea.left - totalW) / 2;
    int posY = workArea.top + (workArea.bottom - workArea.top - totalH) / 2;

    SetWindowPos(m_hwnd, nullptr, posX, posY, totalW, totalH, SWP_NOZORDER | SWP_NOACTIVATE);

    // 1. 预设区域
    SetWindowPos(m_hGroupPreset, nullptr, S(24), S(14), S(448), S(20), SWP_NOZORDER);
    SetWindowPos(m_hBtnPreset1, nullptr, S(24), S(36), S(220), S(30), SWP_NOZORDER);
    SetWindowPos(m_hBtnPreset2, nullptr, S(252), S(36), S(220), S(30), SWP_NOZORDER);
    SetWindowPos(m_hBtnPreset3, nullptr, S(24), S(70), S(220), S(30), SWP_NOZORDER);
    SetWindowPos(m_hBtnPreset4, nullptr, S(252), S(70), S(220), S(30), SWP_NOZORDER);

    // 2. 自定义时长区域
    SetWindowPos(m_hGroupCustom, nullptr, S(24), S(110), S(448), S(20), SWP_NOZORDER);
    SetWindowPos(m_hLblWork, nullptr, S(32), S(134), S(140), S(22), SWP_NOZORDER);
    SetWindowPos(m_hWorkMinEdit, nullptr, S(230), S(130), S(80), S(24), SWP_NOZORDER);
    SetWindowPos(m_hLblWorkUnit, nullptr, S(320), S(134), S(60), S(22), SWP_NOZORDER);

    SetWindowPos(m_hLblStand, nullptr, S(32), S(162), S(140), S(22), SWP_NOZORDER);
    SetWindowPos(m_hStandMinEdit, nullptr, S(230), S(158), S(80), S(24), SWP_NOZORDER);
    SetWindowPos(m_hLblStandUnit, nullptr, S(320), S(162), S(60), S(22), SWP_NOZORDER);

    SetWindowPos(m_hLblRest, nullptr, S(32), S(190), S(140), S(22), SWP_NOZORDER);
    SetWindowPos(m_hRestSecEdit, nullptr, S(230), S(186), S(80), S(24), SWP_NOZORDER);
    SetWindowPos(m_hLblRestUnit, nullptr, S(320), S(190), S(120), S(22), SWP_NOZORDER);

    // 3. 模式与外观区域
    SetWindowPos(m_hGroupTheme, nullptr, S(24), S(224), S(448), S(20), SWP_NOZORDER);
    SetWindowPos(m_hLblMode, nullptr, S(32), S(248), S(130), S(22), SWP_NOZORDER);
    SetWindowPos(m_hModeCombo, nullptr, S(170), S(244), S(302), S(120), SWP_NOZORDER);

    SetWindowPos(m_hLblTheme, nullptr, S(32), S(278), S(130), S(22), SWP_NOZORDER);
    SetWindowPos(m_hThemeCombo, nullptr, S(170), S(274), S(302), S(100), SWP_NOZORDER);

    SetWindowPos(m_hLblMascot, nullptr, S(32), S(308), S(130), S(22), SWP_NOZORDER);
    SetWindowPos(m_hMascotCombo, nullptr, S(170), S(304), S(302), S(100), SWP_NOZORDER);

    SetWindowPos(m_hLblTray, nullptr, S(32), S(338), S(130), S(22), SWP_NOZORDER);
    SetWindowPos(m_hTrayCombo, nullptr, S(170), S(334), S(302), S(100), SWP_NOZORDER);

    // 4. 高级选项区域 (双列 3 行精简舒展排版)
    SetWindowPos(m_hGroupOptions, nullptr, S(24), S(372), S(448), S(20), SWP_NOZORDER);
    SetWindowPos(m_hChkStand, nullptr, S(32), S(396), S(216), S(22), SWP_NOZORDER);
    SetWindowPos(m_hChkBlock, nullptr, S(256), S(396), S(216), S(22), SWP_NOZORDER);

    SetWindowPos(m_hChkStrong, nullptr, S(32), S(424), S(216), S(22), SWP_NOZORDER);
    SetWindowPos(m_hChkTop, nullptr, S(256), S(424), S(216), S(22), SWP_NOZORDER);

    SetWindowPos(m_hChkAutoStart, nullptr, S(32), S(452), S(216), S(22), SWP_NOZORDER);
    SetWindowPos(m_hChkEdgeDock, nullptr, S(256), S(452), S(216), S(22), SWP_NOZORDER);

    // 5. 底部操作按钮 (留足 26px 呼吸底边距)
    SetWindowPos(m_hBtnSave, nullptr, S(256), S(498), S(130), S(34), SWP_NOZORDER);
    SetWindowPos(m_hBtnCancel, nullptr, S(394), S(498), S(78), S(34), SWP_NOZORDER);

    // 应用字体
    SendMessageW(m_hGroupPreset, WM_SETFONT, reinterpret_cast<WPARAM>(m_hSectionFont), TRUE);
    SendMessageW(m_hGroupCustom, WM_SETFONT, reinterpret_cast<WPARAM>(m_hSectionFont), TRUE);
    SendMessageW(m_hGroupTheme, WM_SETFONT, reinterpret_cast<WPARAM>(m_hSectionFont), TRUE);
    SendMessageW(m_hGroupOptions, WM_SETFONT, reinterpret_cast<WPARAM>(m_hSectionFont), TRUE);

    HWND normalControls[] = {
        m_hLblWork, m_hWorkMinEdit, m_hLblWorkUnit,
        m_hLblStand, m_hStandMinEdit, m_hLblStandUnit,
        m_hLblRest, m_hRestSecEdit, m_hLblRestUnit,
        m_hLblMode, m_hModeCombo, m_hLblTheme, m_hThemeCombo,
        m_hLblMascot, m_hMascotCombo, m_hLblTray, m_hTrayCombo,
        m_hChkStand, m_hChkBlock, m_hChkStrong, m_hChkTop, m_hChkAutoStart, m_hChkEdgeDock
    };

    for (HWND hCtrl : normalControls) {
        SendMessageW(hCtrl, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    }

    // Res-1 修复: 新字体已完全应用到所有子控件，现在安全销毁旧字体句柄
    if (hOldFont) DeleteObject(hOldFont);
    if (hOldBold) DeleteObject(hOldBold);
    if (hOldSection) DeleteObject(hOldSection);
}

void SettingsWindow::DrawModernButton(LPDRAWITEMSTRUCT pDis, bool isDark) {
    if (!pDis) return;

    HDC hdc = pDis->hDC;
    RECT rc = pDis->rcItem;
    bool isPressed = (pDis->itemState & ODS_SELECTED) != 0;
    bool isFocused = (pDis->itemState & ODS_FOCUS) != 0;

    wchar_t btnText[64] = { 0 };
    GetWindowTextW(pDis->hwndItem, btnText, 64);

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    RECT localRc = { 0, 0, rc.right - rc.left, rc.bottom - rc.top };
    FillRect(memDC, &localRc, isDark ? m_hDarkBgBrush : m_hLightBgBrush);

    COLORREF bgColor, borderColor, textColor;

    bool isCurrentPresetActive = false;
    if (pDis->CtlID == IDC_BTN_PRESET1 && m_selectedPreset == 1) isCurrentPresetActive = true;
    else if (pDis->CtlID == IDC_BTN_PRESET2 && m_selectedPreset == 2) isCurrentPresetActive = true;
    else if (pDis->CtlID == IDC_BTN_PRESET3 && m_selectedPreset == 3) isCurrentPresetActive = true;
    else if (pDis->CtlID == IDC_BTN_PRESET4 && m_selectedPreset == 4) isCurrentPresetActive = true;

    if (pDis->CtlID == IDC_BTN_SAVE) {
        // 主按钮：翡翠绿
        bgColor = isPressed ? RGB(5, 150, 105) : RGB(16, 185, 129);
        borderColor = isPressed ? RGB(4, 120, 87) : RGB(16, 185, 129);
        textColor = RGB(255, 255, 255);
    } else if (pDis->CtlID == IDC_BTN_CANCEL) {
        // 取消按钮
        bgColor = isDark ? (isPressed ? RGB(52, 56, 68) : RGB(38, 42, 52))
                         : (isPressed ? RGB(225, 228, 234) : RGB(240, 242, 246));
        borderColor = isDark ? RGB(70, 78, 92) : RGB(210, 214, 222);
        textColor = isDark ? RGB(220, 224, 232) : RGB(55, 65, 81);
    } else {
        // 预设卡片按钮 (Preset 1~4)
        if (isCurrentPresetActive) {
            bgColor = isDark ? (isPressed ? RGB(20, 52, 44) : RGB(26, 60, 50))
                             : (isPressed ? RGB(209, 250, 229) : RGB(236, 253, 245));
            borderColor = RGB(16, 185, 129);
            textColor = isDark ? RGB(52, 211, 153) : RGB(5, 150, 105);
        } else {
            bgColor = isDark ? (isPressed ? RGB(48, 54, 66) : (isFocused ? RGB(42, 48, 60) : RGB(34, 38, 48)))
                             : (isPressed ? RGB(232, 236, 242) : (isFocused ? RGB(244, 247, 252) : RGB(255, 255, 255)));
            borderColor = isDark ? (isFocused ? RGB(96, 165, 250) : RGB(58, 65, 78))
                                 : (isFocused ? RGB(59, 130, 246) : RGB(220, 224, 230));
            textColor = isDark ? RGB(240, 244, 248) : RGB(31, 41, 55);
        }
    }

    HBRUSH hBtnBrush = CreateSolidBrush(bgColor);
    HPEN hPen = CreatePen(PS_SOLID, isCurrentPresetActive ? 2 : 1, borderColor);
    HGDIOBJ oldBrush = SelectObject(memDC, hBtnBrush);
    HGDIOBJ oldPen = SelectObject(memDC, hPen);

    RoundRect(memDC, localRc.left, localRc.top, localRc.right, localRc.bottom, 8, 8);

    SelectObject(memDC, oldPen);
    SelectObject(memDC, oldBrush);
    DeleteObject(hPen);
    DeleteObject(hBtnBrush);

    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, textColor);
    HGDIOBJ oldFont = SelectObject(memDC, m_hBoldFont);
    DrawTextW(memDC, btnText, -1, &localRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(memDC, oldFont);

    BitBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void SettingsWindow::DrawModernComboBox(LPDRAWITEMSTRUCT pDis, bool isDark) {
    if (!pDis || pDis->itemID == static_cast<UINT>(-1)) return;

    HDC hdc = pDis->hDC;
    RECT rc = pDis->rcItem;

    wchar_t itemText[128] = { 0 };
    SendMessageW(pDis->hwndItem, CB_GETLBTEXT, pDis->itemID, reinterpret_cast<LPARAM>(itemText));

    bool isSelected = (pDis->itemState & ODS_SELECTED);

    COLORREF bgColor, textColor, borderColor;
    if (isDark) {
        bgColor = isSelected ? RGB(45, 55, 72) : RGB(34, 38, 48);
        textColor = isSelected ? RGB(255, 255, 255) : RGB(226, 232, 240);
        borderColor = RGB(65, 75, 90);
    } else {
        bgColor = isSelected ? RGB(225, 238, 255) : RGB(255, 255, 255);
        textColor = isSelected ? RGB(15, 23, 42) : RGB(51, 65, 85);
        borderColor = RGB(210, 215, 225);
    }

    HBRUSH hBgBrush = CreateSolidBrush(bgColor);
    FillRect(hdc, &rc, hBgBrush);
    DeleteObject(hBgBrush);

    // 文字绘制
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textColor);
    HGDIOBJ oldFont = SelectObject(hdc, m_hFont);

    RECT textRc = rc;
    textRc.left += 8;
    textRc.right -= 8;
    DrawTextW(hdc, itemText, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
}

void SettingsWindow::DrawModernCheckBox(LPDRAWITEMSTRUCT pDis, bool isDark) {
    if (!pDis) return;

    HDC hdc = pDis->hDC;
    RECT rc = pDis->rcItem;

    bool isChecked = false;
    if (pDis->CtlID == IDC_CHK_STAND) isChecked = m_chkStandVal;
    else if (pDis->CtlID == IDC_CHK_BLOCK) isChecked = m_chkBlockVal;
    else if (pDis->CtlID == IDC_CHK_STRONG) isChecked = m_chkStrongVal;
    else if (pDis->CtlID == IDC_CHK_TOP) isChecked = m_chkTopVal;
    else if (pDis->CtlID == IDC_CHK_AUTOSTART) isChecked = m_chkAutoStartVal;
    else if (pDis->CtlID == IDC_CHK_EDGEDOCK) isChecked = m_chkEdgeDockVal;

    // 1. 擦除背景为窗口背景底色
    FillRect(hdc, &rc, isDark ? m_hDarkBgBrush : m_hLightBgBrush);

    // 2. 绘制 16x16 Fluent 圆角方框
    int boxSize = 16;
    int boxY = rc.top + (rc.bottom - rc.top - boxSize) / 2;
    RECT boxRc = { rc.left, boxY, rc.left + boxSize, boxY + boxSize };

    if (isChecked) {
        // 选中态：高雅翡翠绿底 + 纯白对勾 (复用常驻画刷与画笔，零动态分配)
        HGDIOBJ oldBrush = SelectObject(hdc, m_hAccentBrush);
        HGDIOBJ oldPen = SelectObject(hdc, m_hAccentPen);
        RoundRect(hdc, boxRc.left, boxRc.top, boxRc.right, boxRc.bottom, 4, 4);

        // 绘制纯白加粗对勾 ✔ (复用常驻对勾笔)
        SelectObject(hdc, m_hWhiteCheckPen);
        POINT pts[3] = {
            { boxRc.left + 3, boxRc.top + 8 },
            { boxRc.left + 6, boxRc.top + 12 },
            { boxRc.left + 12, boxRc.top + 4 }
        };
        Polyline(hdc, pts, 3);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
    } else {
        // 未选中态：深色微光背景/纯白背景 + 柔和暗灰圆角边框
        HGDIOBJ oldBrush = SelectObject(hdc, isDark ? m_hDarkEditBrush : m_hLightEditBrush);
        HPEN hBorderPen = isDark ? m_hDividerPenDark : m_hDividerPenLight;
        HGDIOBJ oldPen = SelectObject(hdc, hBorderPen);
        RoundRect(hdc, boxRc.left, boxRc.top, boxRc.right, boxRc.bottom, 4, 4);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
    }

    // 3. 绘制选项文字 (高对比清晰显示，彻底告别黑字)
    wchar_t btnText[128] = { 0 };
    GetWindowTextW(pDis->hwndItem, btnText, 128);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, isDark ? RGB(226, 232, 240) : RGB(30, 41, 59));
    HGDIOBJ oldFont = SelectObject(hdc, m_hFont);

    RECT textRc = rc;
    textRc.left += boxSize + 10;
    DrawTextW(hdc, btnText, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

void SettingsWindow::SetupTooltips() {
    if (m_hTooltip) {
        DestroyWindow(m_hTooltip);
        m_hTooltip = nullptr;
    }

    m_hTooltip = CreateWindowExW(
        WS_EX_TOPMOST,
        TOOLTIPS_CLASS,
        nullptr,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        m_hwnd,
        nullptr,
        m_hInstance,
        nullptr
    );

    if (!m_hTooltip) return;

    SetWindowPos(m_hTooltip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SendMessageW(m_hTooltip, TTM_SETMAXTIPWIDTH, 0, 320);

    auto AddTip = [this](HWND hCtrl, const wchar_t* text) {
        if (!hCtrl || !text) return;
        TOOLINFOW ti = { sizeof(TOOLINFOW) };
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = m_hwnd;
        ti.uId = reinterpret_cast<UINT_PTR>(hCtrl);
        ti.lpszText = const_cast<LPWSTR>(text);
        SendMessageW(m_hTooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
    };

    AddTip(m_hBtnPreset1, L"经典推荐：45分钟专注 + 15分钟站立 + 90秒综合工间操");
    AddTip(m_hBtnPreset2, L"轻量循环：50分钟专注 + 10分钟站立 + 90秒综合工间操");
    AddTip(m_hBtnPreset3, L"高效番茄钟：25分钟极度专注 + 5分钟站立 + 60秒护眼操");
    AddTip(m_hBtnPreset4, L"深度攻坚：60分钟深度沉浸 + 20分钟站立 + 90秒综合工间操");

    AddTip(m_hWorkMinEdit, L"连续坐姿工作时长（分钟），到达后提醒休息或站立");
    AddTip(m_hStandMinEdit, L"站立办公时长（分钟），配合升降桌使用");
    AddTip(m_hRestSecEdit, L"全屏工间操放松时长（秒）：综合操满血推荐 90 秒(最少 60 秒)，护眼操标准 60 秒，颈椎操推荐 40 秒(最少 32 秒)，极简放空无限制");
    AddTip(m_hLblRestUnit, L"全屏工间操放松时长（秒）：综合操满血推荐 90 秒(最少 60 秒)，护眼操标准 60 秒，颈椎操推荐 40 秒(最少 32 秒)，极简放空无限制");

    AddTip(m_hModeCombo, L"工间操模式：包含综合操(推荐90s/最少60s)、颈椎保养(推荐40s/最少32s)、20-20-20护眼(60s)或极简放空");
    AddTip(m_hThemeCombo, L"界面主题：自动跟随 Windows 系统、浅色模式或深色模式");
    AddTip(m_hMascotCombo, L"桌面形象：佛系水豚(头顶橘子)、灵动像素猫(敲键盘)、赛博小助手或工效坐姿");
    AddTip(m_hTrayCombo, L"系统托盘风格：标准应用图标、微缩倒计时进度环、或状态感应/CPU动力的 RunCat 灵动小猫");

    AddTip(m_hChkStand, L"坐姿工作倒计时结束后，自动切换为站立办公倒计时，促进全身血液循环");
    AddTip(m_hChkBlock, L"全屏休息时安全拦截键盘输入防止误触，按 ESC 键可紧急解锁恢复桌面");
    AddTip(m_hChkStrong, L"倒计时最后 30 秒悬浮窗泛出警示红光，提醒提前收尾工作");
    AddTip(m_hChkTop, L"让倒计时悬浮窗始终保持在所有窗口最顶层显示，避免被其他软件遮挡");
    AddTip(m_hChkAutoStart, L"电脑开机登录时自动在系统托盘后台静默启动");
    AddTip(m_hChkEdgeDock, L"拖拽浮窗贴近屏幕左右边缘时，自动折叠收起为 32px 灵动小拉手");
}

void SettingsWindow::LoadConfigToUI() {
    auto& config = ConfigManager::Instance().GetConfig();

    wchar_t buf[32];
    swprintf_s(buf, L"%d", config.workMinutes);
    SetWindowTextW(m_hWorkMinEdit, buf);

    swprintf_s(buf, L"%d", config.standMinutes);
    SetWindowTextW(m_hStandMinEdit, buf);

    swprintf_s(buf, L"%d", config.restSeconds);
    SetWindowTextW(m_hRestSecEdit, buf);

    SendMessageW(m_hModeCombo, CB_SETCURSEL, static_cast<WPARAM>(config.exerciseMode), 0);
    SendMessageW(m_hThemeCombo, CB_SETCURSEL, static_cast<WPARAM>(config.themeMode), 0);
    SendMessageW(m_hMascotCombo, CB_SETCURSEL, static_cast<WPARAM>(config.mascotTheme), 0);
    SendMessageW(m_hTrayCombo, CB_SETCURSEL, static_cast<WPARAM>(config.trayDisplayMode), 0);

    m_chkStandVal = config.enableStand;
    m_chkBlockVal = config.blockInput;
    m_chkStrongVal = config.strongReminder;
    m_chkTopVal = config.alwaysTopMost;
    m_chkAutoStartVal = config.autoStart;
    m_chkEdgeDockVal = config.enableEdgeDock;

    if (m_hChkStand) InvalidateRect(m_hChkStand, nullptr, TRUE);
    if (m_hChkBlock) InvalidateRect(m_hChkBlock, nullptr, TRUE);
    if (m_hChkStrong) InvalidateRect(m_hChkStrong, nullptr, TRUE);
    if (m_hChkTop) InvalidateRect(m_hChkTop, nullptr, TRUE);
    if (m_hChkAutoStart) InvalidateRect(m_hChkAutoStart, nullptr, TRUE);
    if (m_hChkEdgeDock) InvalidateRect(m_hChkEdgeDock, nullptr, TRUE);

    // 自动判定当前激活的预设 (基于 AppConstants 纯函数统一计算)
    m_selectedPreset = AppConstants::FindMatchingPresetId(config.workMinutes, config.standMinutes, config.restSeconds);

    if (m_hBtnPreset1) InvalidateRect(m_hBtnPreset1, nullptr, TRUE);
    if (m_hBtnPreset2) InvalidateRect(m_hBtnPreset2, nullptr, TRUE);
    if (m_hBtnPreset3) InvalidateRect(m_hBtnPreset3, nullptr, TRUE);
    if (m_hBtnPreset4) InvalidateRect(m_hBtnPreset4, nullptr, TRUE);
}

void SettingsWindow::SaveConfigFromUI() {
    auto& config = ConfigManager::Instance().GetConfig();

    LRESULT modeIdx = SendMessageW(m_hModeCombo, CB_GETCURSEL, 0, 0);
    if (modeIdx != CB_ERR) config.exerciseMode = static_cast<ExerciseMode>(modeIdx);

    LRESULT themeIdx = SendMessageW(m_hThemeCombo, CB_GETCURSEL, 0, 0);
    if (themeIdx != CB_ERR) config.themeMode = static_cast<ThemeMode>(themeIdx);

    LRESULT mascotIdx = SendMessageW(m_hMascotCombo, CB_GETCURSEL, 0, 0);
    if (mascotIdx != CB_ERR) config.mascotTheme = static_cast<MascotTheme>(mascotIdx);

    LRESULT trayIdx = SendMessageW(m_hTrayCombo, CB_GETCURSEL, 0, 0);
    if (trayIdx != CB_ERR) config.trayDisplayMode = static_cast<TrayDisplayMode>(trayIdx);

    wchar_t buf[32];
    GetWindowTextW(m_hWorkMinEdit, buf, 32);
    config.workMinutes = (std::max)(1, _wtoi(buf));

    GetWindowTextW(m_hStandMinEdit, buf, 32);
    config.standMinutes = (std::max)(0, _wtoi(buf));

    GetWindowTextW(m_hRestSecEdit, buf, 32);
    int minRest = ReminderConfig::GetMinRestSecondsForMode(config.exerciseMode);
    config.restSeconds = (std::max)(minRest, _wtoi(buf));

    config.enableStand = m_chkStandVal;
    config.blockInput = m_chkBlockVal;
    config.strongReminder = m_chkStrongVal;
    config.alwaysTopMost = m_chkTopVal;
    config.autoStart = m_chkAutoStartVal;
    config.enableEdgeDock = m_chkEdgeDockVal;

    ConfigManager::Instance().Save();

    if (g_pStateMachine) {
        g_pStateMachine->SetConfig(config);
    }

    // 立即刷新主题管理器与全系统 UI 窗口与托盘
    ThemeManager::Instance().Refresh();
    OnThemeChanged();
    FloatingWindow::Instance().OnThemeChanged();
    FloatingWindow::Instance().OnConfigChanged();
    TrayWindow::Instance().RefreshTrayDisplayMode();
}

LRESULT CALLBACK SettingsWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    try {
        auto& self = SettingsWindow::Instance();

        switch (msg) {
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED: {
            if (!self.m_isUpdatingTheme) {
                self.m_isUpdatingTheme = true;
                ThemeManager::Instance().Refresh();
                self.OnThemeChanged();
                FloatingWindow::Instance().OnThemeChanged();
                TrayWindow::Instance().RefreshTrayDisplayMode();
                self.m_isUpdatingTheme = false;
            }
            return 0;
        }

        case WM_ERASEBKGND: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            RECT rc;
            GetClientRect(hwnd, &rc);
            bool isDark = ThemeManager::Instance().IsEffectiveDark();
            FillRect(hdc, &rc, isDark ? self.m_hDarkBgBrush : self.m_hLightBgBrush);
            return 1;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            bool isDark = ThemeManager::Instance().IsEffectiveDark();
            float scale = D2DContext::GetWindowDpiScale(hwnd);
            HPEN hPen = isDark ? self.m_hDividerPenDark : self.m_hDividerPenLight;
            HGDIOBJ oldPen = SelectObject(hdc, hPen);
            RECT rc;
            GetClientRect(hwnd, &rc);

            int xL = static_cast<int>(24 * scale);
            int xR = rc.right - static_cast<int>(24 * scale);

            // 分区水平微光分割线
            int dividers[] = {
                static_cast<int>(104 * scale),
                static_cast<int>(218 * scale),
                static_cast<int>(366 * scale),
                static_cast<int>(486 * scale)
            };

            for (int y : dividers) {
                MoveToEx(hdc, xL, y, nullptr);
                LineTo(hdc, xR, y);
            }

            SelectObject(hdc, oldPen);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            auto* pDis = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
            if (pDis) {
                bool isDark = ThemeManager::Instance().IsEffectiveDark();
                if (pDis->CtlType == ODT_BUTTON) {
                    if (pDis->CtlID >= IDC_CHK_STAND && pDis->CtlID <= IDC_CHK_EDGEDOCK) {
                        self.DrawModernCheckBox(pDis, isDark);
                    } else {
                        self.DrawModernButton(pDis, isDark);
                    }
                    return TRUE;
                } else if (pDis->CtlType == ODT_COMBOBOX) {
                    self.DrawModernComboBox(pDis, isDark);
                    return TRUE;
                }
            }
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            HWND hCtrl = reinterpret_cast<HWND>(lParam);
            SetBkMode(hdc, TRANSPARENT);
            bool isDark = ThemeManager::Instance().IsEffectiveDark();

            if (isDark) {
                // 一级高光标题 (#F8FAFC)
                if (hCtrl == self.m_hGroupPreset || hCtrl == self.m_hGroupCustom ||
                    hCtrl == self.m_hGroupTheme || hCtrl == self.m_hGroupOptions) {
                    SetTextColor(hdc, RGB(248, 250, 252));
                }
                // 三级弱化说明 / 单位 (#94A3B8)
                else if (hCtrl == self.m_hLblWorkUnit || hCtrl == self.m_hLblStandUnit || hCtrl == self.m_hLblRestUnit) {
                    SetTextColor(hdc, RGB(148, 163, 184));
                }
                // 二级主体文本与复选框 (#CBD5E1)
                else {
                    SetTextColor(hdc, RGB(203, 213, 225));
                }
                SetBkColor(hdc, RGB(28, 30, 36));
                return reinterpret_cast<INT_PTR>(self.m_hDarkBgBrush);
            } else {
                if (hCtrl == self.m_hGroupPreset || hCtrl == self.m_hGroupCustom ||
                    hCtrl == self.m_hGroupTheme || hCtrl == self.m_hGroupOptions) {
                    SetTextColor(hdc, RGB(17, 24, 39));
                } else if (hCtrl == self.m_hLblWorkUnit || hCtrl == self.m_hLblStandUnit || hCtrl == self.m_hLblRestUnit) {
                    SetTextColor(hdc, RGB(107, 114, 128));
                } else {
                    SetTextColor(hdc, RGB(55, 65, 81));
                }
                SetBkColor(hdc, RGB(248, 249, 251));
                return reinterpret_cast<INT_PTR>(self.m_hLightBgBrush);
            }
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            bool isDark = ThemeManager::Instance().IsEffectiveDark();
            if (isDark) {
                SetTextColor(hdc, RGB(245, 248, 252));
                SetBkColor(hdc, RGB(40, 44, 54));
                return reinterpret_cast<INT_PTR>(self.m_hDarkEditBrush);
            } else {
                SetTextColor(hdc, RGB(17, 24, 39));
                SetBkColor(hdc, RGB(255, 255, 255));
                return reinterpret_cast<INT_PTR>(self.m_hLightEditBrush);
            }
        }

        case WM_DPICHANGED: {
            float dpiScale = LOWORD(wParam) / 96.0f;
            auto* lprc = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(hwnd, nullptr, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            self.UpdateLayout(dpiScale);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            WORD code = HIWORD(wParam);

            if (code == CBN_SELCHANGE && reinterpret_cast<HWND>(lParam) == self.m_hModeCombo) {
                auto newMode = static_cast<ExerciseMode>(SendMessageW(self.m_hModeCombo, CB_GETCURSEL, 0, 0));
                int recSec = ReminderConfig::GetRecommendedRestSecondsForMode(newMode);
                wchar_t newBuf[32];
                swprintf_s(newBuf, L"%d", recSec);
                SetWindowTextW(self.m_hRestSecEdit, newBuf);
                return 0;
            }

            if (code == EN_CHANGE && (reinterpret_cast<HWND>(lParam) == self.m_hWorkMinEdit ||
                                      reinterpret_cast<HWND>(lParam) == self.m_hStandMinEdit ||
                                      reinterpret_cast<HWND>(lParam) == self.m_hRestSecEdit)) {
                wchar_t bufW[32] = { 0 }, bufS[32] = { 0 }, bufR[32] = { 0 };
                GetWindowTextW(self.m_hWorkMinEdit, bufW, 32);
                GetWindowTextW(self.m_hStandMinEdit, bufS, 32);
                GetWindowTextW(self.m_hRestSecEdit, bufR, 32);
                int w = _wtoi(bufW);
                int s = _wtoi(bufS);
                int r = _wtoi(bufR);

                int newPreset = AppConstants::FindMatchingPresetId(w, s, r);
                if (newPreset != self.m_selectedPreset) {
                    self.m_selectedPreset = newPreset;
                    if (self.m_hBtnPreset1) InvalidateRect(self.m_hBtnPreset1, nullptr, TRUE);
                    if (self.m_hBtnPreset2) InvalidateRect(self.m_hBtnPreset2, nullptr, TRUE);
                    if (self.m_hBtnPreset3) InvalidateRect(self.m_hBtnPreset3, nullptr, TRUE);
                    if (self.m_hBtnPreset4) InvalidateRect(self.m_hBtnPreset4, nullptr, TRUE);
                }
                return 0;
            }

            switch (id) {
                case IDC_BTN_PRESET1:
                case IDC_BTN_PRESET2:
                case IDC_BTN_PRESET3:
                case IDC_BTN_PRESET4: {
                    int presetId = static_cast<int>(id - IDC_BTN_PRESET1 + 1);
                    if (const auto* pPreset = AppConstants::GetPresetById(presetId)) {
                        ConfigManager::Instance().ApplyPreset(pPreset->workMinutes, pPreset->standMinutes, pPreset->restSeconds);
                        ConfigManager::Instance().Save();
                        self.LoadConfigToUI();
                        if (g_pStateMachine) {
                            g_pStateMachine->SetConfig(ConfigManager::Instance().GetConfig());
                            g_pStateMachine->StartWork();
                        }
                        FloatingWindow::Instance().OnConfigChanged();
                        TrayWindow::Instance().RefreshTrayDisplayMode();
                    }
                    break;
                }
                case IDC_CHK_STAND:
                    self.m_chkStandVal = !self.m_chkStandVal;
                    InvalidateRect(self.m_hChkStand, nullptr, TRUE);
                    break;
                case IDC_CHK_BLOCK:
                    self.m_chkBlockVal = !self.m_chkBlockVal;
                    InvalidateRect(self.m_hChkBlock, nullptr, TRUE);
                    break;
                case IDC_CHK_STRONG:
                    self.m_chkStrongVal = !self.m_chkStrongVal;
                    InvalidateRect(self.m_hChkStrong, nullptr, TRUE);
                    break;
                case IDC_CHK_TOP:
                    self.m_chkTopVal = !self.m_chkTopVal;
                    InvalidateRect(self.m_hChkTop, nullptr, TRUE);
                    break;
                case IDC_CHK_AUTOSTART:
                    self.m_chkAutoStartVal = !self.m_chkAutoStartVal;
                    InvalidateRect(self.m_hChkAutoStart, nullptr, TRUE);
                    break;
                case IDC_CHK_EDGEDOCK:
                    self.m_chkEdgeDockVal = !self.m_chkEdgeDockVal;
                    InvalidateRect(self.m_hChkEdgeDock, nullptr, TRUE);
                    break;
                case IDOK:
                case IDC_BTN_SAVE:
                    self.SaveConfigFromUI();
                    SetWindowTextW(self.m_hBtnSave, L"✓ 已应用");
                    InvalidateRect(self.m_hBtnSave, nullptr, TRUE);
                    SetTimer(hwnd, 9991, 1200, nullptr);
                    break;
                case IDC_BTN_CANCEL:
                case IDCANCEL:
                    ShowWindow(hwnd, SW_HIDE);
                    break;
            }
            return 0;
        }

        case WM_TIMER:
            if (wParam == 9991) {
                KillTimer(hwnd, 9991);
                SetWindowTextW(self.m_hBtnSave, L"✔ 保存并应用");
                InvalidateRect(self.m_hBtnSave, nullptr, TRUE);
                return 0;
            }
            break;

        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;

        case WM_DESTROY:
            self.m_hwnd = nullptr;
            return 0;
        }
    } catch (...) {
        // C++ 异常屏障: 拦截业务层异常逃逸出 C ABI 回调
#ifdef _DEBUG
        OutputDebugStringW(L"[SitStandReminder] Exception caught in SettingsWindow::WndProc\n");
#endif
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
