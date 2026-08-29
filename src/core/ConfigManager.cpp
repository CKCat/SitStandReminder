#include "ConfigManager.hpp"
#include <shlwapi.h>

ConfigManager::ConfigManager() {
    Load();
}

void ConfigManager::Load() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, m_regKeyPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        // 辅助 lambda: 每次读取前自动重置 dwSize/dwType，防止链式参数污染
        auto ReadDword = [&](const wchar_t* name, DWORD& outVal) -> bool {
            DWORD dwType = REG_DWORD;
            DWORD dwSize = sizeof(DWORD);
            return RegQueryValueExW(hKey, name, nullptr, &dwType, reinterpret_cast<LPBYTE>(&outVal), &dwSize) == ERROR_SUCCESS
                && dwType == REG_DWORD;
        };

        DWORD dwValue = 0;
        if (ReadDword(L"WorkMinutes", dwValue))   m_config.workMinutes = static_cast<int>(dwValue);
        if (ReadDword(L"RestSeconds", dwValue))    m_config.restSeconds = static_cast<int>(dwValue);
        if (ReadDword(L"StandMinutes", dwValue))   m_config.standMinutes = static_cast<int>(dwValue);
        if (ReadDword(L"EnableStand", dwValue))    m_config.enableStand = (dwValue != 0);
        if (ReadDword(L"BlockInput", dwValue))     m_config.blockInput = (dwValue != 0);
        if (ReadDword(L"StrongReminder", dwValue)) m_config.strongReminder = (dwValue != 0);
        if (ReadDword(L"AlwaysTopMost", dwValue))  m_config.alwaysTopMost = (dwValue != 0);
        if (ReadDword(L"MultiScreen", dwValue))    m_config.multiScreen = (dwValue != 0);
        if (ReadDword(L"EnableSound", dwValue))    m_config.enableSound = (dwValue != 0);
        if (ReadDword(L"ThemeMode", dwValue) && dwValue <= 2)       m_config.themeMode = static_cast<ThemeMode>(dwValue);
        if (ReadDword(L"ExerciseMode", dwValue) && dwValue <= 3)   m_config.exerciseMode = static_cast<ExerciseMode>(dwValue);
        if (ReadDword(L"MascotTheme", dwValue) && dwValue <= 3)    m_config.mascotTheme = static_cast<MascotTheme>(dwValue);
        if (ReadDword(L"TrayDisplayMode", dwValue) && dwValue <= 3) m_config.trayDisplayMode = static_cast<TrayDisplayMode>(dwValue);
        if (ReadDword(L"EnableEdgeDock", dwValue)) m_config.enableEdgeDock = (dwValue != 0);

        RegCloseKey(hKey);
    }
    m_config.autoStart = IsAutoStartEnabled();
}

void ConfigManager::Save() {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, m_regKeyPath.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD dwVal = 0;

        dwVal = static_cast<DWORD>(m_config.workMinutes);
        RegSetValueExW(hKey, L"WorkMinutes", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        dwVal = static_cast<DWORD>(m_config.restSeconds);
        RegSetValueExW(hKey, L"RestSeconds", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        dwVal = static_cast<DWORD>(m_config.standMinutes);
        RegSetValueExW(hKey, L"StandMinutes", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        dwVal = m_config.enableStand ? 1 : 0;
        RegSetValueExW(hKey, L"EnableStand", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        dwVal = m_config.blockInput ? 1 : 0;
        RegSetValueExW(hKey, L"BlockInput", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        dwVal = m_config.strongReminder ? 1 : 0;
        RegSetValueExW(hKey, L"StrongReminder", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        dwVal = m_config.alwaysTopMost ? 1 : 0;
        RegSetValueExW(hKey, L"AlwaysTopMost", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        dwVal = m_config.multiScreen ? 1 : 0;
        RegSetValueExW(hKey, L"MultiScreen", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        dwVal = m_config.enableSound ? 1 : 0;
        RegSetValueExW(hKey, L"EnableSound", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        dwVal = static_cast<DWORD>(m_config.themeMode);
        RegSetValueExW(hKey, L"ThemeMode", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        dwVal = static_cast<DWORD>(m_config.exerciseMode);
        RegSetValueExW(hKey, L"ExerciseMode", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        dwVal = static_cast<DWORD>(m_config.mascotTheme);
        RegSetValueExW(hKey, L"MascotTheme", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        dwVal = static_cast<DWORD>(m_config.trayDisplayMode);
        RegSetValueExW(hKey, L"TrayDisplayMode", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        dwVal = m_config.enableEdgeDock ? 1 : 0;
        RegSetValueExW(hKey, L"EnableEdgeDock", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        RegCloseKey(hKey);
    }
    SetAutoStartEnabled(m_config.autoStart);
}

bool ConfigManager::IsAutoStartEnabled() const {
    HKEY hKey = nullptr;
    const wchar_t* runKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    bool enabled = false;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, runKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t path[MAX_PATH] = { 0 };
        DWORD dwSize = sizeof(path);
        if (RegQueryValueExW(hKey, AppConstants::Identity::RUN_AUTORUN_NAME, nullptr, nullptr, reinterpret_cast<LPBYTE>(path), &dwSize) == ERROR_SUCCESS) {
            path[MAX_PATH - 1] = L'\0'; // 强制 null 终止，防止注册表字符串无终止符导致越界读取
            enabled = (wcslen(path) > 0);
        } else {
            dwSize = sizeof(path); // 重置缓冲区大小，防止首次查询污染
            memset(path, 0, sizeof(path));
            if (RegQueryValueExW(hKey, L"SedentaryReminderNative", nullptr, nullptr, reinterpret_cast<LPBYTE>(path), &dwSize) == ERROR_SUCCESS) {
                path[MAX_PATH - 1] = L'\0';
                enabled = (wcslen(path) > 0);
            }
        }
        RegCloseKey(hKey);
    }
    return enabled;
}

void ConfigManager::SetAutoStartEnabled(bool enable) {
    HKEY hKey = nullptr;
    const wchar_t* runKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    if (RegOpenKeyExW(HKEY_CURRENT_USER, runKey, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH] = { 0 };
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            wchar_t quotedPath[MAX_PATH + 4] = { 0 };
            swprintf_s(quotedPath, L"\"%s\"", exePath);
            RegSetValueExW(hKey, AppConstants::Identity::RUN_AUTORUN_NAME, 0, REG_SZ, reinterpret_cast<const BYTE*>(quotedPath), (static_cast<DWORD>(wcslen(quotedPath)) + 1) * sizeof(wchar_t));
            RegDeleteValueW(hKey, L"SedentaryReminderNative"); // 清理旧键名
        } else {
            RegDeleteValueW(hKey, AppConstants::Identity::RUN_AUTORUN_NAME);
            RegDeleteValueW(hKey, L"SedentaryReminderNative");
        }
        RegCloseKey(hKey);
    }
}

void ConfigManager::ApplyPreset(int workMin, int standMin, int restSec) {
    m_config.workMinutes = workMin;
    m_config.standMinutes = standMin;
    m_config.enableStand = (standMin > 0);
    m_config.restSeconds = restSec;
    Save();
}

