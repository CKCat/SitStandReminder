#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <d2d1.h>
#include <functional>
#include "../core/ConfigManager.hpp"

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

struct ThemeColors {
    D2D1_COLOR_F background;
    D2D1_COLOR_F cardBackground;
    D2D1_COLOR_F cardBorder;
    D2D1_COLOR_F textPrimary;
    D2D1_COLOR_F textSecondary;
    D2D1_COLOR_F textMuted;
    D2D1_COLOR_F accent;
    D2D1_COLOR_F forestGreen;  // 坐姿绿
    D2D1_COLOR_F skyBlue;      // 站立蓝
    D2D1_COLOR_F alertRed;     // 临界预警红
};

class ThemeManager {
public:
    static ThemeManager& Instance() {
        static ThemeManager instance;
        return instance;
    }

    bool IsSystemDark() const;
    bool IsEffectiveDark() const;
    bool IsTaskbarDark() const;
    bool IsEffectiveTaskbarDark() const;

    void ApplyThemeToWindow(HWND hwnd) const;
    const ThemeColors& GetColors() const { return m_colors; }
    
    void Refresh();

    using ThemeChangedCallback = std::function<void(bool isDark)>;
    void SetOnThemeChanged(ThemeChangedCallback cb) { m_onThemeChanged = std::move(cb); }

private:
    ThemeManager();
    ~ThemeManager() = default;

    void UpdateColors();

    bool m_isDark = false;
    ThemeColors m_colors;
    ThemeChangedCallback m_onThemeChanged;
};

