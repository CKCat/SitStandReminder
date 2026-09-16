#include "SingleInstance.hpp"
#include "../core/AppConstants.hpp"

#ifdef _WIN32
#include <windows.h>

bool SingleInstance::TryAcquire() {
    m_hMutex = CreateMutexW(nullptr, TRUE, AppConstants::Identity::MUTEX_NAME);
    if (!m_hMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (m_hMutex) {
            CloseHandle(m_hMutex);
            m_hMutex = nullptr;
        }
        return false;
    }
    return true;
}

void SingleInstance::Release() {
    if (m_hMutex) {
        ReleaseMutex(m_hMutex);
        CloseHandle(m_hMutex);
        m_hMutex = nullptr;
    }
}

void SingleInstance::WakeExistingInstance() {
    HWND hTray = FindWindowW(AppConstants::Identity::CLASS_TRAY, nullptr);
    if (hTray) {
        DWORD targetPid = 0;
        GetWindowThreadProcessId(hTray, &targetPid);
        if (targetPid != 0) {
            AllowSetForegroundWindow(targetPid);
        } else {
            AllowSetForegroundWindow(ASFW_ANY);
        }
        PostMessageW(hTray, WM_COMMAND, 2020 /* IDM_TRAY_SETTINGS */, 0);
    }
}

bool SingleInstance::CheckAndResetWakeRequested() {
    return false;
}

#else
// Linux 平台基于 flock 文件排他锁与 SIGUSR1 单实例唤醒
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <atomic>

static std::atomic<bool> s_wakeRequested{false};

static void SignalHandler(int signum) {
    if (signum == SIGUSR1) {
        s_wakeRequested.store(true);
    }
}

static std::filesystem::path GetStandardLockPath() {
    const char* customLockPath = std::getenv("SITSTAND_TEST_LOCK_PATH");
    if (customLockPath && *customLockPath) {
        return customLockPath;
    }
    const char* xdgRuntime = std::getenv("XDG_RUNTIME_DIR");
    std::filesystem::path lockDir;
    if (xdgRuntime && *xdgRuntime) {
        lockDir = xdgRuntime;
        std::error_code ec;
        std::filesystem::create_directories(lockDir, ec);
        return lockDir / AppConstants::Identity::LOCK_FILE_NAME;
    }

    const char* home = std::getenv("HOME");
    if (home && *home) {
        lockDir = std::filesystem::path(home) / ".cache";
        std::error_code ec;
        std::filesystem::create_directories(lockDir, ec);
        return lockDir / AppConstants::Identity::LOCK_FILE_NAME;
    }

    // 全局共享 /tmp 目录必须附带 UID 隔离后缀，防止多用户冲突与越权劫持
    std::string uidLockName = std::string("SitStandReminder_") + std::to_string(getuid()) + ".lock";
    return std::filesystem::path("/tmp") / uidLockName;
}

bool SingleInstance::CheckAndResetWakeRequested() {
    return s_wakeRequested.exchange(false);
}

bool SingleInstance::TryAcquire() {
    m_lockPath = GetStandardLockPath().string();

    // 锁文件使用 0600 (S_IRUSR | S_IWUSR) 权限，确保仅当前用户可读写
    m_lockFd = open(m_lockPath.c_str(), O_RDWR | O_CREAT, 0600);
    if (m_lockFd < 0) {
        // 若标准目录无法写入，降级到带 UID 隔离的 /tmp 路径
        std::string uidLockName = std::string("SitStandReminder_") + std::to_string(getuid()) + ".lock";
        m_lockPath = (std::filesystem::path("/tmp") / uidLockName).string();
        m_lockFd = open(m_lockPath.c_str(), O_RDWR | O_CREAT, 0600);
        if (m_lockFd < 0) {
            return false;
        }
    }

    if (flock(m_lockFd, LOCK_EX | LOCK_NB) != 0) {
        // 另一个实例已持有排他锁正在运行
        close(m_lockFd);
        m_lockFd = -1;
        return false;
    }

    // 将当前进程 PID 写入锁文件，便于后发实例定向唤醒
    [[maybe_unused]] int truncRes = ftruncate(m_lockFd, 0);
    lseek(m_lockFd, 0, SEEK_SET);
    std::string pidStr = std::to_string(getpid()) + "\n";
    [[maybe_unused]] auto res = write(m_lockFd, pidStr.c_str(), pidStr.size());

    // 捕获 SIGUSR1 信号唤醒设置中心
    struct sigaction sa;
    sa.sa_handler = SignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, nullptr);

    return true;
}

void SingleInstance::Release() {
    if (m_lockFd >= 0) {
        flock(m_lockFd, LOCK_UN);
        close(m_lockFd);
        m_lockFd = -1;
        // 严禁在此处调用 unlink(m_lockPath.c_str())！
        // 保留文件保证所有并发竞争进程始终 flock 同一个 Inode，杜绝多实例竞争窗口。
    }
}

void SingleInstance::WakeExistingInstance() {
    if (m_lockPath.empty()) {
        m_lockPath = GetStandardLockPath().string();
    }
    std::ifstream f(m_lockPath);
    if (!f.is_open()) {
        std::filesystem::path tmpPath = std::filesystem::path("/tmp") / AppConstants::Identity::LOCK_FILE_NAME;
        f.open(tmpPath);
    }
    if (f.is_open()) {
        pid_t pid = 0;
        if (f >> pid && pid > 1) {
            kill(pid, SIGUSR1);
        }
    }
}

#endif
