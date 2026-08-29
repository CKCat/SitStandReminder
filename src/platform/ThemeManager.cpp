#include "ThemeManager.hpp"
#include <uxtheme.h>

ThemeManager::ThemeManager() {
    Refresh();
}

bool ThemeManager::IsSystemDark() const {
    HKEY hKey = nullptr;
    const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    DWORD useLightTheme = 1;
    DWORD dwSize = sizeof(useLightTheme);

    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&useLightTheme), &dwSize);
        RegCloseKey(hKey);
    }
    return (useLightTheme == 0);
}

bool ThemeManager::IsTaskbarDark() const {
    HKEY hKey = nullptr;
    const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    DWORD systemUsesLightTheme = 0; // 默认深色任务栏 (Windows 经典基线)
    DWORD dwSize = sizeof(systemUsesLightTheme);

    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        // 优先读取专属任务栏与系统的 SystemUsesLightTheme 键值
        if (RegQueryValueExW(hKey, L"SystemUsesLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&systemUsesLightTheme), &dwSize) != ERROR_SUCCESS) {
            dwSize = sizeof(systemUsesLightTheme);
            RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&systemUsesLightTheme), &dwSize);
        }
        RegCloseKey(hKey);
    }
    return (systemUsesLightTheme == 0);
}

bool ThemeManager::IsEffectiveDark() const {
    auto themeMode = ConfigManager::Instance().GetConfig().themeMode;
    if (themeMode == ThemeMode::Dark) return true;
    if (themeMode == ThemeMode::Light) return false;
    return IsSystemDark();
}

bool ThemeManager::IsEffectiveTaskbarDark() const {
    auto themeMode = ConfigManager::Instance().GetConfig().themeMode;
    if (themeMode == ThemeMode::Dark) return true;
    if (themeMode == ThemeMode::Light) return false;
    return IsTaskbarDark();
}

void ThemeManager::Refresh() {
    bool oldDark = m_isDark;
    m_isDark = IsEffectiveDark();
    UpdateColors();

    if (m_onThemeChanged && oldDark != m_isDark) {
        m_onThemeChanged(m_isDark);
    }
}

void ThemeManager::UpdateColors() {
    m_colors.forestGreen = D2D1::ColorF(0.18f, 0.54f, 0.34f, 1.0f); // #2E8B57
    m_colors.skyBlue     = D2D1::ColorF(0.12f, 0.53f, 0.90f, 1.0f); // #1E88E5
    m_colors.alertRed    = D2D1::ColorF(0.92f, 0.26f, 0.21f, 1.0f); // #EB4335

    if (m_isDark) {
        m_colors.background     = D2D1::ColorF(0.12f, 0.12f, 0.14f, 1.0f);
        m_colors.cardBackground = D2D1::ColorF(0.18f, 0.18f, 0.20f, 0.95f);
        m_colors.cardBorder     = D2D1::ColorF(0.28f, 0.28f, 0.32f, 1.0f);
        m_colors.textPrimary    = D2D1::ColorF(0.95f, 0.95f, 0.96f, 1.0f);
        m_colors.textSecondary  = D2D1::ColorF(0.72f, 0.72f, 0.76f, 1.0f);
        m_colors.textMuted      = D2D1::ColorF(0.50f, 0.50f, 0.55f, 1.0f);
        m_colors.accent         = D2D1::ColorF(0.38f, 0.65f, 0.98f, 1.0f);
    } else {
        m_colors.background     = D2D1::ColorF(0.96f, 0.96f, 0.98f, 1.0f);
        m_colors.cardBackground = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.96f);
        m_colors.cardBorder     = D2D1::ColorF(0.88f, 0.88f, 0.90f, 1.0f);
        m_colors.textPrimary    = D2D1::ColorF(0.10f, 0.10f, 0.12f, 1.0f);
        m_colors.textSecondary  = D2D1::ColorF(0.40f, 0.40f, 0.45f, 1.0f);
        m_colors.textMuted      = D2D1::ColorF(0.60f, 0.60f, 0.65f, 1.0f);
        m_colors.accent         = D2D1::ColorF(0.15f, 0.45f, 0.90f, 1.0f);
    }
}

enum class PreferredAppMode {
    Default,
    AllowDark,
    ForceDark,
    ForceLight,
    Max
};

using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode appMode);
using fnAllowDarkModeForWindow = bool(WINAPI*)(HWND hWnd, bool allow);
using fnFlushMenuThemes = void(WINAPI*)();

void ThemeManager::ApplyThemeToWindow(HWND hwnd) const {
    if (!hwnd) return;
    BOOL darkMode = m_isDark ? TRUE : FALSE;
    // 1. 优先调用 Windows 10 20H1+ 及 Windows 11 标准属性 (20)
    if (FAILED(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode)))) {
        // 回退调用 Windows 10 1809~1909 属性 (19)
        DwmSetWindowAttribute(hwnd, 19, &darkMode, sizeof(darkMode));
    }

    // 2. Windows 11 原生硬件级圆角与抗锯齿阴影注入 (DWMWA_WINDOW_CORNER_PREFERENCE = 33, DWMWCP_ROUND = 2)
    DWORD cornerPref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &cornerPref, sizeof(cornerPref));

    HMODULE hUxtheme = GetModuleHandleW(L"uxtheme.dll");
    if (!hUxtheme) hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hUxtheme) {
        auto pAllowDarkModeForWindow = reinterpret_cast<fnAllowDarkModeForWindow>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133)));
        if (pAllowDarkModeForWindow) {
            pAllowDarkModeForWindow(hwnd, m_isDark);
        }
        auto pSetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135)));
        if (pSetPreferredAppMode) {
            pSetPreferredAppMode(m_isDark ? PreferredAppMode::ForceDark : PreferredAppMode::ForceLight);
        }
        auto pFlushMenuThemes = reinterpret_cast<fnFlushMenuThemes>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136)));
        if (pFlushMenuThemes) {
            pFlushMenuThemes();
        }
    }

    SetWindowTheme(hwnd, m_isDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
}

