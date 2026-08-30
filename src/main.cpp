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

    // 1. 单实例互斥量保护与智能前置唤醒 (基于单一事实源)
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, AppConstants::Identity::MUTEX_NAME);
    if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        HWND hTray = FindWindowW(AppConstants::Identity::CLASS_TRAY, nullptr);
        if (hTray) {
            DWORD targetPid = 0;
            GetWindowThreadProcessId(hTray, &targetPid);
            if (targetPid != 0) {
                AllowSetForegroundWindow(targetPid);
            } else {
                AllowSetForegroundWindow(ASFW_ANY);
            }
            PostMessageW(hTray, WM_COMMAND, IDM_TRAY_SETTINGS, 0);
        }
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

        // 构建托盘 Tooltip 并一次性提交更新 (合并 NIF_ICON 与 NIF_TIP，削减 50% 跨进程 IPC 调用)
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

    // 9. 主消息循环（完整支持非模态对话框键盘 Tab/Enter/Esc 导航）
    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessageW(&msg, nullptr, 0, 0)) != 0) {
        if (bRet == -1) break; // GetMessageW 错误时安全退出，防止处理垃圾消息的无限死循环
        HWND hSettings = SettingsWindow::Instance().GetHwnd();
        if (hSettings && IsWindow(hSettings) && IsWindowVisible(hSettings) && IsDialogMessageW(hSettings, &msg)) {
            continue; // 键盘无障碍焦点事件已由 Win32 内置 Dialog 引擎消费
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

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return 0;
}
