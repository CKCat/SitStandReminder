#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#else
#include <X11/Xlib.h>
#include "../graphics/D2DCompat.hpp"
#include "../graphics/linux/LinuxCanvas.hpp"
#endif

#include <string>
#include <memory>
#include "../core/ConfigManager.hpp"

class SettingsWindow {
public:
    static SettingsWindow& Instance() {
        static SettingsWindow instance;
        return instance;
    }

#ifdef _WIN32
    bool Create(HINSTANCE hInstance);
    void Show(HINSTANCE hInstance = nullptr);
    HWND GetHwnd() const { return m_hwnd; }
    bool IsVisible() const { return m_hwnd != nullptr && IsWindowVisible(m_hwnd); }
#else
    bool Create(void* hInstance = nullptr);
    void Show(void* hInstance = nullptr);
    Window GetWindow() const { return m_window; }
    bool IsVisible() const { return m_visible; }
    void HandleEvent(const XEvent& ev);
    const ReminderConfig& GetTempConfig() const { return m_tempConfig; }
    int GetSelectedPreset() const { return m_selectedPreset; }
    void SimulateClick(int x, int y);
#endif

    void Close();
    void Destroy();
    void OnThemeChanged();
    void LoadConfigToUI();

private:
    SettingsWindow() = default;
    ~SettingsWindow() { Destroy(); }

#ifdef _WIN32
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void CreateControls(HWND hwnd);
    void UpdateLayout(float dpiScale, bool recenter = true);
    void SaveConfigFromUI();
    ReminderConfig GetConfigFromUI() const;
    void UpdateDirtyState();
    void CleanRegistryAndExit();
    void DrawModernButton(LPDRAWITEMSTRUCT pDis, bool isDark);
    void DrawModernComboBox(LPDRAWITEMSTRUCT pDis, bool isDark);
    void DrawModernCheckBox(LPDRAWITEMSTRUCT pDis, bool isDark);
    void SetupTooltips();

    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    HWND m_hTooltip = nullptr;
    float m_dpiScale = 1.0f;
    bool m_isUpdatingTheme = false;
    bool m_isLoading = false;
    ReminderConfig m_originalConfig;

    HWND m_hGroupPreset = nullptr;
    HWND m_hGroupCustom = nullptr;
    HWND m_hGroupTheme = nullptr;
    HWND m_hGroupOptions = nullptr;

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
    HWND m_hBorderWidthCombo = nullptr;

    HWND m_hWorkMinEdit = nullptr;
    HWND m_hStandMinEdit = nullptr;
    HWND m_hRestSecEdit = nullptr;
    HWND m_hModeCombo = nullptr;
    HWND m_hThemeCombo = nullptr;
    HWND m_hMascotCombo = nullptr;
    HWND m_hTrayCombo = nullptr;
    HWND m_hChkStand = nullptr;
    HWND m_hChkBlock = nullptr;
    HWND m_hChkStrong = nullptr;
    HWND m_hChkTop = nullptr;
    HWND m_hChkAutoStart = nullptr;
    HWND m_hChkEdgeDock = nullptr;

    HWND m_hBtnPreset1 = nullptr;
    HWND m_hBtnPreset2 = nullptr;
    HWND m_hBtnPreset3 = nullptr;
    HWND m_hBtnPreset4 = nullptr;
    
    HWND m_hBtnSave = nullptr;
    HWND m_hBtnCancel = nullptr;
    HWND m_hBtnClean = nullptr;

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

    bool m_chkStandVal = true;
    bool m_chkBlockVal = true;
    bool m_chkStrongVal = true;
    bool m_chkTopVal = false;
    bool m_chkAutoStartVal = false;
    bool m_chkEdgeDockVal = true;

#else
    // Linux X11 设置中心
    void Render();
    void SaveConfigFromUI();

    Window m_window = 0;
    GC m_gc = nullptr;
    std::unique_ptr<LinuxRenderTarget> m_pRenderTarget;
    bool m_visible = false;
    int m_width = 560;
    int m_height = 660;

    ReminderConfig m_tempConfig;
    int m_selectedPreset = -1;
    bool m_shortcutFeedback = false;
#endif
};
