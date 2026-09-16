#include "ConfigManager.hpp"

#ifdef _WIN32
#include <shlwapi.h>
#else
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>
#include <unistd.h>

static std::filesystem::path GetLinuxConfigDir() {
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig && *xdgConfig) {
        return std::filesystem::path(xdgConfig) / AppConstants::Identity::CONFIG_DIR_NAME;
    }
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return std::filesystem::path(home) / ".config" / AppConstants::Identity::CONFIG_DIR_NAME;
    }
    return std::filesystem::current_path() / ".config" / AppConstants::Identity::CONFIG_DIR_NAME;
}

static std::filesystem::path GetLinuxAutostartPath() {
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path base;
    if (xdgConfig && *xdgConfig) {
        base = std::filesystem::path(xdgConfig);
    } else {
        const char* home = std::getenv("HOME");
        base = (home && *home) ? (std::filesystem::path(home) / ".config") : (std::filesystem::current_path() / ".config");
    }
    return base / "autostart" / AppConstants::Identity::DESKTOP_ENTRY_NAME;
}

static std::string GetExecutablePath() {
    char buf[1024];
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return std::string(buf);
    }
    return "";
}
#endif

ConfigManager::ConfigManager() {
    Load();
}

void ConfigManager::Load() {
#ifdef _WIN32
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
        if (ReadDword(L"BorderWidth", dwValue) && dwValue <= 3)     m_config.borderWidth = static_cast<BorderWidth>(dwValue);
        if (ReadDword(L"EnableEdgeDock", dwValue)) m_config.enableEdgeDock = (dwValue != 0);

        RegCloseKey(hKey);
    }
    m_config.autoStart = IsAutoStartEnabled();
#else
    auto configPath = GetLinuxConfigDir() / AppConstants::Identity::CONFIG_FILE_NAME;
    std::ifstream file(configPath);
    if (file.is_open()) {
        ParseIniStream(file, m_config);
    }
    m_config.autoStart = IsAutoStartEnabled();
#endif
}

void ConfigManager::ParseIniStream(std::istream& file, ReminderConfig& config) {
    std::string line;
    std::map<std::string, std::string> kv;
    while (std::getline(file, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        char firstChar = line[start];
        if (firstChar == '#' || firstChar == ';' || firstChar == '[') {
            continue;
        }
        auto eq = line.find('=', start);
        if (eq != std::string::npos) {
            std::string k = line.substr(start, eq - start);
            std::string v = line.substr(eq + 1);
            while (!k.empty() && (k.back() == ' ' || k.back() == '\t' || k.back() == '\r')) k.pop_back();
            while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\r')) v.pop_back();
            while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.erase(0, 1);
            kv[k] = v;
        }
    }
    auto getInt = [&](const std::string& k, int def) -> int {
        auto it = kv.find(k);
        if (it != kv.end()) {
            try { return std::stoi(it->second); } catch (...) {}
        }
        return def;
    };
    auto getBool = [&](const std::string& k, bool def) -> bool {
        auto it = kv.find(k);
        if (it != kv.end()) {
            return (it->second == "1" || it->second == "true" || it->second == "True");
        }
        return def;
    };

    config.workMinutes = getInt("WorkMinutes", config.workMinutes);
    config.restSeconds = getInt("RestSeconds", config.restSeconds);
    config.standMinutes = getInt("StandMinutes", config.standMinutes);
    config.enableStand = getBool("EnableStand", config.enableStand);
    config.blockInput = getBool("BlockInput", config.blockInput);
    config.strongReminder = getBool("StrongReminder", config.strongReminder);
    config.alwaysTopMost = getBool("AlwaysTopMost", config.alwaysTopMost);
    config.multiScreen = getBool("MultiScreen", config.multiScreen);
    config.enableSound = getBool("EnableSound", config.enableSound);
    int tm = getInt("ThemeMode", static_cast<int>(config.themeMode));
    if (tm >= 0 && tm <= 2) config.themeMode = static_cast<ThemeMode>(tm);
    int em = getInt("ExerciseMode", static_cast<int>(config.exerciseMode));
    if (em >= 0 && em <= 3) config.exerciseMode = static_cast<ExerciseMode>(em);
    int mt = getInt("MascotTheme", static_cast<int>(config.mascotTheme));
    if (mt >= 0 && mt <= 3) config.mascotTheme = static_cast<MascotTheme>(mt);
    int tdm = getInt("TrayDisplayMode", static_cast<int>(config.trayDisplayMode));
    if (tdm >= 0 && tdm <= 3) config.trayDisplayMode = static_cast<TrayDisplayMode>(tdm);
    int bw = getInt("BorderWidth", static_cast<int>(config.borderWidth));
    if (bw >= 0 && bw <= 3) config.borderWidth = static_cast<BorderWidth>(bw);
    config.enableEdgeDock = getBool("EnableEdgeDock", config.enableEdgeDock);
}

void ConfigManager::Save() {
#ifdef _WIN32
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

        dwVal = static_cast<DWORD>(m_config.borderWidth);
        RegSetValueExW(hKey, L"BorderWidth", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        dwVal = m_config.enableEdgeDock ? 1 : 0;
        RegSetValueExW(hKey, L"EnableEdgeDock", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwVal), sizeof(DWORD));

        RegCloseKey(hKey);
    }
    SetAutoStartEnabled(m_config.autoStart);
#else
    auto dir = GetLinuxConfigDir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    auto configPath = dir / AppConstants::Identity::CONFIG_FILE_NAME;
    auto tmpPath = dir / (std::string(AppConstants::Identity::CONFIG_FILE_NAME) + ".tmp");
    {
        std::ofstream file(tmpPath, std::ios::out | std::ios::trunc);
        if (file.is_open()) {
            file << "# SitStandReminder Configuration\n";
            file << "WorkMinutes=" << m_config.workMinutes << "\n";
            file << "RestSeconds=" << m_config.restSeconds << "\n";
            file << "StandMinutes=" << m_config.standMinutes << "\n";
            file << "EnableStand=" << (m_config.enableStand ? 1 : 0) << "\n";
            file << "BlockInput=" << (m_config.blockInput ? 1 : 0) << "\n";
            file << "StrongReminder=" << (m_config.strongReminder ? 1 : 0) << "\n";
            file << "AlwaysTopMost=" << (m_config.alwaysTopMost ? 1 : 0) << "\n";
            file << "MultiScreen=" << (m_config.multiScreen ? 1 : 0) << "\n";
            file << "EnableSound=" << (m_config.enableSound ? 1 : 0) << "\n";
            file << "ThemeMode=" << static_cast<int>(m_config.themeMode) << "\n";
            file << "ExerciseMode=" << static_cast<int>(m_config.exerciseMode) << "\n";
            file << "MascotTheme=" << static_cast<int>(m_config.mascotTheme) << "\n";
            file << "TrayDisplayMode=" << static_cast<int>(m_config.trayDisplayMode) << "\n";
            file << "BorderWidth=" << static_cast<int>(m_config.borderWidth) << "\n";
            file << "EnableEdgeDock=" << (m_config.enableEdgeDock ? 1 : 0) << "\n";
            file.flush();
        }
    }
    // 原子替换，防止系统崩溃或断电导致配置文件 0 字节损坏
    if (std::filesystem::exists(tmpPath)) {
        std::filesystem::rename(tmpPath, configPath, ec);
    }
    SetAutoStartEnabled(m_config.autoStart);
#endif
}

bool ConfigManager::IsAutoStartEnabled() const {
#ifdef _WIN32
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
#else
    auto path = GetLinuxAutostartPath();
    std::error_code ec;
    return std::filesystem::exists(path, ec);
#endif
}

void ConfigManager::SetAutoStartEnabled(bool enable) {
#ifdef _WIN32
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
#else
    auto path = GetLinuxAutostartPath();
    std::error_code ec;
    if (enable) {
        std::filesystem::create_directories(path.parent_path(), ec);
        std::string exe = GetExecutablePath();
        if (exe.empty()) exe = "SitStandReminder";
        std::ofstream f(path);
        if (f.is_open()) {
            f << "[Desktop Entry]\n";
            f << "Type=Application\n";
            f << "Version=1.5\n";
            f << "Name=SitStandReminder\n";
            f << "Name[zh_CN]=坐立提醒\n";
            f << "Comment=科学坐立与工位健康伴侣\n";
            f << "Exec=\"" << exe << "\"\n";
            f << "Icon=sitstandreminder\n";
            f << "Terminal=false\n";
            f << "Categories=Utility;Health;\n";
            f << "X-GNOME-Autostart-enabled=true\n";
        }
    } else {
        std::filesystem::remove(path, ec);
    }
#endif
}

bool ConfigManager::InstallDesktopShortcuts(bool toDesktop, bool toMenu) {
#ifdef _WIN32
    (void)toDesktop;
    (void)toMenu;
    return true;
#else
    std::string exe = GetExecutablePath();
    if (exe.empty()) exe = "SitStandReminder";

    // 确定图标位置（若本地存在则使用绝对路径，否则使用系统注册的图标名称）
    std::string iconPath = "sitstandreminder";
    std::filesystem::path exeDir = std::filesystem::path(exe).parent_path();
    std::filesystem::path localIcon = exeDir / "sitstandreminder.png";
    std::error_code ec;
    if (std::filesystem::exists(localIcon, ec)) {
        iconPath = localIcon.string();
    } else {
        std::filesystem::path resIcon = exeDir / "resources" / "sitstandreminder.png";
        if (std::filesystem::exists(resIcon, ec)) {
            iconPath = resIcon.string();
        }
    }

    std::string desktopContent = 
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Version=1.5\n"
        "Name=SitStandReminder\n"
        "Name[zh_CN]=坐立提醒\n"
        "GenericName=Health Reminder\n"
        "GenericName[zh_CN]=工位健康坐立与工间操助手\n"
        "Comment=Scientific desk exercise and sedentary reminder assistant\n"
        "Comment[zh_CN]=科学坐立与颈部视力舒缓工间操健康伴侣\n"
        "Exec=\"" + exe + "\"\n"
        "Icon=" + iconPath + "\n"
        "Terminal=false\n"
        "Categories=Utility;Health;Clock;\n"
        "StartupNotify=true\n"
        "StartupWMClass=SitStandReminder\n";

    bool success = true;

    // 1. 注册到当前用户的应用程序菜单 (~/.local/share/applications)
    if (toMenu) {
        const char* home = std::getenv("HOME");
        if (home && *home) {
            std::filesystem::path menuDir = std::filesystem::path(home) / ".local" / "share" / "applications";
            std::filesystem::create_directories(menuDir, ec);
            std::ofstream f(menuDir / AppConstants::Identity::DESKTOP_ENTRY_NAME);
            if (f.is_open()) {
                f << desktopContent;
            } else {
                success = false;
            }
        }
    }

    // 2. 放置到用户桌面 (~/Desktop 或 ~/桌面)
    if (toDesktop) {
        const char* home = std::getenv("HOME");
        if (home && *home) {
            std::filesystem::path desktopDir = std::filesystem::path(home) / "Desktop";
            if (!std::filesystem::exists(desktopDir, ec)) {
                std::filesystem::path cnDesktop = std::filesystem::path(home) / "桌面";
                if (std::filesystem::exists(cnDesktop, ec)) {
                    desktopDir = cnDesktop;
                }
            }
            if (std::filesystem::exists(desktopDir, ec)) {
                std::filesystem::path deskFile = desktopDir / AppConstants::Identity::DESKTOP_ENTRY_NAME;
                std::ofstream f(deskFile);
                if (f.is_open()) {
                    f << desktopContent;
                    f.close();
                    // 设置 0755 可执行权限，便于文件管理器识别允许双击启动
                    std::filesystem::permissions(deskFile,
                        std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::group_exec | std::filesystem::perms::others_read | std::filesystem::perms::others_exec,
                        std::filesystem::perm_options::replace, ec);
                } else {
                    success = false;
                }
            }
        }
    }

    return success;
#endif
}

void ConfigManager::ApplyPreset(int workMin, int standMin, int restSec) {
    m_config.workMinutes = workMin;
    m_config.standMinutes = standMin;
    m_config.enableStand = (standMin > 0);
    m_config.restSeconds = restSec;
    Save();
}

bool ConfigManager::ClearConfig() {
    SetAutoStartEnabled(false);
#ifdef _WIN32
    // 递归删除软件在注册表中的全部配置键树，并删除根键自身
    LSTATUS status = RegDeleteTreeW(HKEY_CURRENT_USER, m_regKeyPath.c_str());
    RegDeleteKeyW(HKEY_CURRENT_USER, m_regKeyPath.c_str());
    bool ok = (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND);
#else
    std::error_code ec;
    auto configDir = GetLinuxConfigDir();
    std::filesystem::remove_all(configDir, ec);
    bool ok = !ec;
#endif
    if (ok) {
        m_config = ReminderConfig{};
    }
    return ok;
}
