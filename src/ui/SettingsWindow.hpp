#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <string>
#include "../core/ConfigManager.hpp"

class SettingsWindow {
public:
    static SettingsWindow& Instance() {
        static SettingsWindow instance;
        return instance;
    }

    bool Create(HINSTANCE hInstance);
    void Show(HINSTANCE hInstance);
    void Close();
    void OnThemeChanged();
    void LoadConfigToUI();

    HWND GetHwnd() const { return m_hwnd; }

private:
    SettingsWindow() = default;
    ~SettingsWindow() { Close(); }
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void CreateControls(HWND hwnd);
    void UpdateLayout(float dpiScale);
    void SaveConfigFromUI();
    void DrawModernButton(LPDRAWITEMSTRUCT pDis, bool isDark);
    void DrawModernComboBox(LPDRAWITEMSTRUCT pDis, bool isDark);
    void DrawModernCheckBox(LPDRAWITEMSTRUCT pDis, bool isDark);
    void SetupTooltips();

    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    HWND m_hTooltip = nullptr;
    bool m_isUpdatingTheme = false;

    // 分组标题
    HWND m_hGroupPreset = nullptr;
    HWND m_hGroupCustom = nullptr;
    HWND m_hGroupTheme = nullptr;
    HWND m_hGroupOptions = nullptr;

    // 静态标签
    HWND m_hLblWork = nullptr;
    HWND m_hLblStand = nullptr;
    HWND m_hLblRest = nullptr;
    HWND m_hLblWorkUnit = nullptr;
    HWND m_hLblStandUnit = nullptr;
    HWND m_hLblRestUnit = nullptr;
    HWND m_hLblMode = nullptr;
    HWND m_hLblTheme = nullptr;
    HWND m_hLblMascot = nullptr;
    HWND m_hLblTray = nullptr;
    HWND m_hLblBorderWidth = nullptr;

    // 控件句柄
    HWND m_hWorkMinEdit = nullptr;
    HWND m_hStandMinEdit = nullptr;
    HWND m_hRestSecEdit = nullptr;
    HWND m_hModeCombo = nullptr;
    HWND m_hThemeCombo = nullptr;
    HWND m_hMascotCombo = nullptr;
    HWND m_hTrayCombo = nullptr;
    HWND m_hBorderWidthCombo = nullptr;
    HWND m_hChkStand = nullptr;
    HWND m_hChkBlock = nullptr;
    HWND m_hChkStrong = nullptr;
    HWND m_hChkTop = nullptr;
    HWND m_hChkAutoStart = nullptr;
    HWND m_hChkEdgeDock = nullptr;

    // 预设按钮 (BS_OWNERDRAW)
    HWND m_hBtnPreset1 = nullptr; // 45m/15m/60s
    HWND m_hBtnPreset2 = nullptr; // 50m/10m/60s
    HWND m_hBtnPreset3 = nullptr; // 25m/5m/30s (番茄)
    HWND m_hBtnPreset4 = nullptr; // 60m/20m/60s
    
    HWND m_hBtnSave = nullptr;
    HWND m_hBtnCancel = nullptr;

    HFONT m_hFont = nullptr;
    HFONT m_hBoldFont = nullptr;
    HFONT m_hSectionFont = nullptr;

    HBRUSH m_hDarkBgBrush = nullptr;
    HBRUSH m_hDarkEditBrush = nullptr;
    HBRUSH m_hLightBgBrush = nullptr;
    HBRUSH m_hLightEditBrush = nullptr;
    HBRUSH m_hAccentBrush = nullptr;
    HPEN m_hDividerPenLight = nullptr;
    HPEN m_hDividerPenDark = nullptr;
    HPEN m_hAccentPen = nullptr;
    HPEN m_hAccentThickPen = nullptr;
    HPEN m_hWhiteCheckPen = nullptr;

    int m_selectedPreset = -1;

    // 复选框交互状态
    bool m_chkStandVal = true;
    bool m_chkBlockVal = true;
    bool m_chkStrongVal = true;
    bool m_chkTopVal = false;
    bool m_chkAutoStartVal = false;
    bool m_chkEdgeDockVal = true;
};

