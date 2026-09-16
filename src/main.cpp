#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "core/ConfigManager.hpp"
#include "core/StateMachine.hpp"
#include "graphics/D2DContext.hpp"
#include "platform/ThemeManager.hpp"
#include "platform/SingleInstance.hpp"
#include "ui/TrayWindow.hpp"
#include "ui/FloatingWindow.hpp"
#include "ui/FullscreenMask.hpp"
#include "ui/SettingsWindow.hpp"
#include "core/AppConstants.hpp"
#include <memory>
#include <cwchar>

StateMachine* g_pStateMachine = nullptr;

// 开启 Windows 11 效率模式 (EcoQoS) 与低功耗智能调度
#ifndef PROCESS_POWER_THROTTLING_CURRENT_VERSION
#define PROCESS_POWER_THROTTLING_CURRENT_VERSION 1
#define PROCESS_POWER_THROTTLING_EXECUTION_SPEED 0x1
#define PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION 0x4
typedef struct _PROCESS_POWER_THROTTLING_STATE {
    ULONG Version;
    ULONG ControlMask;
    ULONG StateMask;
} PROCESS_POWER_THROTTLING_STATE, *PPROCESS_POWER_THROTTLING_STATE;
#endif

#ifndef ProcessPowerThrottling
#define ProcessPowerThrottling (PROCESS_INFORMATION_CLASS)40
#endif

static void EnableEcoQoS() {
    PROCESS_POWER_THROTTLING_STATE powerThrottling = { 0 };
    powerThrottling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    powerThrottling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
    powerThrottling.StateMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &powerThrottling, sizeof(powerThrottling));
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PWSTR pCmdLine, int /*nCmdShow*/) {
    // 0. 支持绿色卸载与清理注册表命令行参数 (--clean / --uninstall)
    if (pCmdLine && (wcsstr(pCmdLine, L"--clean") || wcsstr(pCmdLine, L"--uninstall") || wcsstr(pCmdLine, L"/clean") || wcsstr(pCmdLine, L"/uninstall"))) {
        int choice = MessageBoxW(
            nullptr,
            L"是否彻底清除「坐立提醒」在系统注册表中的所有配置与开机自启项？\n\n（清除后程序将恢复出厂状态并退出）",
            AppConstants::Identity::DISPLAY_NAME,
            MB_YESNO | MB_ICONQUESTION | MB_TOPMOST
        );
        if (choice == IDYES) {
            ConfigManager::Instance().ClearRegistry();
            MessageBoxW(
                nullptr,
                L"已成功清除所有配置数据与开机自启动项！",
                AppConstants::Identity::DISPLAY_NAME,
                MB_OK | MB_ICONINFORMATION | MB_TOPMOST
            );
        }
        return 0;
    }

    // 1. 单实例互斥保护与已有实例唤醒 (统一平台抽象)
    if (!SingleInstance::Instance().TryAcquire()) {
        SingleInstance::Instance().WakeExistingInstance();
        return 0;
    }

    // 2. 启用 Per-Monitor DPI v2 现代高分屏感知与 Windows 11 EcoQoS
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    EnableEcoQoS();

    // 3. 初始化 COM 与 Direct2D
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    D2DContext::Instance().Initialize();
    ConfigManager::Instance().Load();
    ThemeManager::Instance().Refresh();

    // 4. 构建核心状态机（启用绝对物理时间戳对齐，杜绝模态循环下的时钟漂移）
    StateMachine sm(ConfigManager::Instance().GetConfig());
    sm.SetUseWallClock(true);
    g_pStateMachine = &sm;

    // 5. 初始化 UI 模块
    TrayWindow::Instance().Create(hInstance);
    FloatingWindow::Instance().Create(hInstance);
    FullscreenMask::Instance().Initialize(hInstance);

    // 6. 绑定状态机事件
    sm.SetOnStateChanged([](AppState oldState, AppState newState) {
        if (ConfigManager::Instance().GetConfig().enableSound) {
            if (newState == AppState::Resting || (oldState == AppState::Resting && (newState == AppState::Working || newState == AppState::Standing))) {
                MessageBeep(MB_ICONASTERISK);
            }
        }

        if (newState == AppState::Working || newState == AppState::Standing || newState == AppState::Paused) {
            FullscreenMask::Instance().Show(false);
            FloatingWindow::Instance().Show(true);
        } else if (newState == AppState::Resting) {
            FloatingWindow::Instance().Show(false);
            FullscreenMask::Instance().Show(true);
        } else if (newState == AppState::Idle) {
            FloatingWindow::Instance().Show(false);
            FullscreenMask::Instance().Show(false);
        }
    });

    sm.SetOnTick([&sm](int remainingSec, int totalSec) {
        FloatingWindow::Instance().UpdateState(sm.GetState(), remainingSec, totalSec);
        FullscreenMask::Instance().UpdateDisplay(remainingSec, totalSec, sm.GetCurrentRestStage(), sm.GetCurrentRestStageName());

        // 构建托盘 Tooltip 并一次性提交更新
        int minutes = remainingSec / 60;
        int seconds = remainingSec % 60;
        wchar_t buf[128];
        const wchar_t* stateName = L"工作中";
        if (sm.GetState() == AppState::Standing) stateName = L"站立中";
        else if (sm.GetState() == AppState::Resting) stateName = L"工间操休息中";
        else if (sm.GetState() == AppState::Paused) stateName = L"已暂停";

        swprintf_s(buf, L"%s (%s) - 剩余 %02d:%02d", AppConstants::Identity::DISPLAY_NAME, stateName, minutes, seconds);
        TrayWindow::Instance().UpdateDynamicIcon(sm.GetState(), remainingSec, totalSec, buf);
    });

    sm.SetOnRestStageChanged([](int stageIndex, const std::wstring& stageName, int remainingSec, int totalSec) {
        FullscreenMask::Instance().UpdateDisplay(remainingSec, totalSec, stageIndex, stageName);
    });

    // 7. 启动初始坐姿工作周期
    sm.StartWork();

    // 8. 启动系统级 1 秒精准时钟
    UINT_PTR mainTimerId = SetTimer(TrayWindow::Instance().GetHwnd(), 5001, 1000, [](HWND, UINT, UINT_PTR, DWORD) {
        if (g_pStateMachine) {
            g_pStateMachine->Tick();
        }
    });

    // 9. 主消息循环
    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessageW(&msg, nullptr, 0, 0)) != 0) {
        if (bRet == -1) break;
        HWND hSettings = SettingsWindow::Instance().GetHwnd();
        if (hSettings && IsWindow(hSettings) && IsWindowVisible(hSettings) && IsDialogMessageW(hSettings, &msg)) {
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 10. 资源安全清理
    KillTimer(TrayWindow::Instance().GetHwnd(), mainTimerId);
    FullscreenMask::Instance().Show(false);
    FloatingWindow::Instance().Destroy();
    TrayWindow::Instance().Destroy();
    SettingsWindow::Instance().Close();
    D2DContext::Instance().Uninitialize();
    CoUninitialize();

    SingleInstance::Instance().Release();

    return 0;
}

#else

// ---------------- Linux POSIX & X11 入口 ----------------
#include "core/ConfigManager.hpp"
#include "core/StateMachine.hpp"
#include "graphics/D2DContext.hpp"
#include "platform/ThemeManager.hpp"
#include "platform/SingleInstance.hpp"
#include "ui/TrayWindow.hpp"
#include "ui/FloatingWindow.hpp"
#include "ui/FullscreenMask.hpp"
#include "ui/SettingsWindow.hpp"
#include "ui/linux/X11App.hpp"
#include "core/AppConstants.hpp"
#include <iostream>
#include <string>
#include <cwchar>
#include <thread>

StateMachine* g_pStateMachine = nullptr;

int main(int argc, char* argv[]) {
    // 0. 支持绿色卸载与清理配置命令行参数 (--clean / --uninstall)
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--clean" || arg == "--uninstall" || arg == "/clean" || arg == "/uninstall") {
            ConfigManager::Instance().ClearConfig();
            std::cout << "[SitStandReminder] 已成功清除所有配置数据与开机自启动项！\n";
            return 0;
        }
    }

    // 1. 单实例互斥保护与已有实例唤醒
    if (!SingleInstance::Instance().TryAcquire()) {
        SingleInstance::Instance().WakeExistingInstance();
        std::cout << "[SitStandReminder] 应用已在运行，已唤醒设置中心。\n";
        return 0;
    }

    // 2. 初始化核心系统与图形引擎
    D2DContext::Instance().Initialize();
    ConfigManager::Instance().Load();
    // 自动向当前用户的桌面应用菜单注册快捷方式与图标 (无需 root 权限)
    ConfigManager::Instance().InstallDesktopShortcuts(false, true);
    ThemeManager::Instance().Refresh();

    // 3. 构建核心状态机
    StateMachine sm(ConfigManager::Instance().GetConfig());
    sm.SetUseWallClock(true);
    g_pStateMachine = &sm;

    // 4. 初始化 X11 UI 组件
    auto& app = X11App::Instance();
    if (!app.Initialize()) {
        std::cerr << "[SitStandReminder] 错误：无法连接至 X11 显示服务器！请检查 DISPLAY 环境变量。\n";
        return 1;
    }

    TrayWindow::Instance().Create();
    FloatingWindow::Instance().Create();
    FullscreenMask::Instance().Initialize();

    SingleInstance::Instance().SetOnWakeRequested([]() {
        SettingsWindow::Instance().Show();
    });

    app.SetPowerStateCallback([](bool isSuspendOrLock) {
        if (g_pStateMachine) {
            if (isSuspendOrLock) {
                g_pStateMachine->OnSystemSuspendOrLock();
            } else {
                g_pStateMachine->OnSystemResumeOrUnlock();
            }
        }
    });

    // 5. 绑定状态机生命周期事件
    sm.SetOnStateChanged([](AppState oldState, AppState newState) {
        if (ConfigManager::Instance().GetConfig().enableSound) {
            if (newState == AppState::Resting || (oldState == AppState::Resting && (newState == AppState::Working || newState == AppState::Standing))) {
                Display* dpy = X11App::Instance().GetDisplay();
                if (dpy) {
                    XBell(dpy, 0);
                    XFlush(dpy);
                }
                std::cout << '\a' << std::flush;

                // 优先使用现代 Linux 桌面 freedesktop 声音规范 (彻底非阻塞执行，不卡顿 GUI 主循环)
                std::thread([]() {
                    static int s_playerType = -1; // -1: 未探测, 1: canberra, 2: paplay, 0: 无
                    if (s_playerType == -1) {
                        if (std::system("which canberra-gtk-play > /dev/null 2>&1") == 0) {
                            s_playerType = 1;
                        } else if (std::system("which paplay > /dev/null 2>&1") == 0) {
                            s_playerType = 2;
                        } else {
                            s_playerType = 0;
                        }
                    }
                    if (s_playerType == 1) {
                        int ret = std::system("canberra-gtk-play -i complete > /dev/null 2>&1");
                        (void)ret;
                    } else if (s_playerType == 2) {
                        int ret = std::system("paplay /usr/share/sounds/freedesktop/stereo/complete.oga > /dev/null 2>&1");
                        (void)ret;
                    }
                }).detach();
            }
        }

        if (newState == AppState::Working || newState == AppState::Standing || newState == AppState::Paused) {
            FullscreenMask::Instance().Show(false);
            FloatingWindow::Instance().Show(true);
        } else if (newState == AppState::Resting) {
            FloatingWindow::Instance().Show(false);
            FullscreenMask::Instance().Show(true);
        } else if (newState == AppState::Idle) {
            FloatingWindow::Instance().Show(false);
            FullscreenMask::Instance().Show(false);
        }
    });

    sm.SetOnTick([&sm](int remainingSec, int totalSec) {
        FloatingWindow::Instance().UpdateState(sm.GetState(), remainingSec, totalSec);
        FullscreenMask::Instance().UpdateDisplay(remainingSec, totalSec, sm.GetCurrentRestStage(), sm.GetCurrentRestStageName());

        int minutes = remainingSec / 60;
        int seconds = remainingSec % 60;
        wchar_t buf[128];
        const wchar_t* stateName = L"工作中";
        if (sm.GetState() == AppState::Standing) stateName = L"站立中";
        else if (sm.GetState() == AppState::Resting) stateName = L"工间操休息中";
        else if (sm.GetState() == AppState::Paused) stateName = L"已暂停";

        swprintf(buf, 128, L"%ls (%ls) - 剩余 %02d:%02d", AppConstants::Identity::DISPLAY_NAME, stateName, minutes, seconds);
        TrayWindow::Instance().UpdateDynamicIcon(sm.GetState(), remainingSec, totalSec, buf);
    });

    sm.SetOnRestStageChanged([](int stageIndex, const std::wstring& stageName, int remainingSec, int totalSec) {
        FullscreenMask::Instance().UpdateDisplay(remainingSec, totalSec, stageIndex, stageName);
    });

    // 6. 启动初始坐姿工作周期
    sm.StartWork();

    // 7. 进入 X11 60FPS 事件驱动与 1 秒心跳循环
    app.RunEventLoop([]() {
        if (g_pStateMachine) {
            g_pStateMachine->Tick();
        }
    });

    // 8. 资源释放与退出清理 (彻底销毁所有 X11 窗口、GC 与图像缓冲区)
    FullscreenMask::Instance().Destroy();
    FloatingWindow::Instance().Destroy();
    TrayWindow::Instance().Destroy();
    SettingsWindow::Instance().Close();
    D2DContext::Instance().Uninitialize();
    SingleInstance::Instance().Release();

    return 0;
}

#endif
