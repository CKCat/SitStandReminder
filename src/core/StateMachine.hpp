#pragma once

#include "ConfigManager.hpp"
#include <chrono>
#include <functional>
#include <string>

enum class AppState {
    Idle,
    Working,   // 坐姿办公倒计时
    Standing,  // 站立办公倒计时
    Resting,   // 全屏工间操休息
    Paused     // 暂停状态
};

class StateMachine {
public:
    using StateChangedCallback = std::function<void(AppState oldState, AppState newState)>;
    using TickCallback = std::function<void(int remainingSec, int totalSec)>;
    using RestStageCallback = std::function<void(int stageIndex, const std::wstring& stageName, int remainingSec, int totalSec)>;

    explicit StateMachine(const ReminderConfig& config);
    ~StateMachine() = default;

    void SetConfig(const ReminderConfig& config);
    const ReminderConfig& GetConfig() const { return m_config; }

    void StartWork();
    void StartStand();
    void StartRest();
    void Pause();
    void Resume();
    void Stop();
    void ExitRestEarly();
    void SkipCurrent();
    void Postpone(int addMinutes = 5);

    // 系统电源/锁屏感知
    void OnSystemSuspendOrLock();
    void OnSystemResumeOrUnlock();

    void Tick(); // 每秒或定时器触发调用
    void SetUseWallClock(bool enable) { m_useWallClock = enable; }

    AppState GetState() const { return m_state; }
    AppState GetPreviousState() const { return m_previousState; }
    int GetRemainingSeconds() const { return m_remainingSeconds; }
    int GetTotalSeconds() const { return m_totalSeconds; }
    int GetElapsedSeconds() const { return m_totalSeconds - m_remainingSeconds; }
    int GetCurrentRestStage() const { return m_currentRestStage; }
    std::wstring GetCurrentRestStageName() const { return m_currentRestStageName; }
    bool IsSuspendedOrLocked() const { return m_isSuspendedOrLocked; }
    ULONGLONG GetSuspendStartTick() const { return m_suspendStartTick; }

    void SetOnStateChanged(StateChangedCallback cb) { m_onStateChanged = std::move(cb); }
    void SetOnTick(TickCallback cb) { m_onTick = std::move(cb); }
    void SetOnRestStageChanged(RestStageCallback cb) { m_onRestStageChanged = std::move(cb); }

private:
    void ChangeState(AppState newState);
    void UpdateRestStage();

    ReminderConfig m_config;
    AppState m_state = AppState::Idle;
    AppState m_previousState = AppState::Idle;
    AppState m_pausedFromState = AppState::Idle;

    int m_remainingSeconds = 0;
    int m_totalSeconds = 0;
    int m_currentRestStage = 0;
    std::wstring m_currentRestStageName = L"";

    bool m_useWallClock = false;
    bool m_isSuspendedOrLocked = false;
    std::chrono::steady_clock::time_point m_stateStartTime;
    std::chrono::steady_clock::time_point m_pauseStartTime;
    ULONGLONG m_suspendStartTick = 0;

    StateChangedCallback m_onStateChanged;
    TickCallback m_onTick;
    RestStageCallback m_onRestStageChanged;
};

