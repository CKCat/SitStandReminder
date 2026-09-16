#include "StateMachine.hpp"
#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
static inline uint64_t GetPlatformBootTickMs() {
    return GetTickCount64();
}
#else
#include <time.h>
static inline uint64_t GetPlatformBootTickMs() {
    struct timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1000000ULL;
}
#endif

StateMachine::StateMachine(const ReminderConfig& config)
    : m_config(config) {
}

void StateMachine::SetConfig(const ReminderConfig& config) {
    m_config = config;

    if (m_state == AppState::Working) {
        int baseTotal = (std::max)(1, m_config.workMinutes * 60);
        int newTotal = baseTotal + m_postponedSeconds;
        if (m_totalSeconds != newTotal) {
            int elapsed = (std::max)(0, m_totalSeconds - m_remainingSeconds);
            m_totalSeconds = newTotal;
            m_remainingSeconds = (std::max)(0, newTotal - elapsed);
            m_stateStartTime = std::chrono::steady_clock::now() - std::chrono::seconds(elapsed);
        }
    } else if (m_state == AppState::Standing) {
        int baseTotal = (std::max)(1, m_config.standMinutes * 60);
        int newTotal = baseTotal + m_postponedSeconds;
        if (m_totalSeconds != newTotal) {
            int elapsed = (std::max)(0, m_totalSeconds - m_remainingSeconds);
            m_totalSeconds = newTotal;
            m_remainingSeconds = (std::max)(0, newTotal - elapsed);
            m_stateStartTime = std::chrono::steady_clock::now() - std::chrono::seconds(elapsed);
        }
    } else if (m_state == AppState::Resting) {
        int newTotal = (std::max)(m_config.GetMinRestSeconds(), m_config.restSeconds);
        if (m_totalSeconds != newTotal) {
            int elapsed = (std::max)(0, m_totalSeconds - m_remainingSeconds);
            m_totalSeconds = newTotal;
            m_remainingSeconds = (std::max)(0, newTotal - elapsed);
            m_stateStartTime = std::chrono::steady_clock::now() - std::chrono::seconds(elapsed);
        }
        UpdateRestStage();
    }

    if (m_onTick) {
        m_onTick(m_remainingSeconds, m_totalSeconds);
    }
}

void StateMachine::ChangeState(AppState newState) {
    if (m_state == newState) return;
    AppState oldState = m_state;
    m_previousState = oldState;
    m_state = newState;

    if (m_onStateChanged) {
        m_onStateChanged(oldState, newState);
    }
    if (m_onTick) {
        m_onTick(m_remainingSeconds, m_totalSeconds);
    }
}

void StateMachine::StartWork() {
    m_postponedSeconds = 0;
    m_totalSeconds = (std::max)(1, m_config.workMinutes * 60);
    m_remainingSeconds = m_totalSeconds;
    m_stateStartTime = std::chrono::steady_clock::now();
    ChangeState(AppState::Working);
}

void StateMachine::StartStand() {
    m_postponedSeconds = 0;
    m_totalSeconds = (std::max)(1, m_config.standMinutes * 60);
    m_remainingSeconds = m_totalSeconds;
    m_stateStartTime = std::chrono::steady_clock::now();
    ChangeState(AppState::Standing);
}

void StateMachine::StartRest() {
    m_postponedSeconds = 0;
    m_totalSeconds = (std::max)(m_config.GetMinRestSeconds(), m_config.restSeconds);
    m_remainingSeconds = m_totalSeconds;
    m_stateStartTime = std::chrono::steady_clock::now();
    m_currentRestStage = 0;
    UpdateRestStage();
    ChangeState(AppState::Resting);
}

void StateMachine::Pause() {
    if (m_state != AppState::Idle && m_state != AppState::Paused) {
        m_pausedFromState = m_state;
        m_pauseStartTime = std::chrono::steady_clock::now();
        ChangeState(AppState::Paused);
    }
}

void StateMachine::Resume() {
    if (m_state == AppState::Paused) {
        auto pauseDuration = std::chrono::steady_clock::now() - m_pauseStartTime;
        m_stateStartTime += pauseDuration;
        ChangeState(m_pausedFromState);
    }
}

void StateMachine::Stop() {
    m_postponedSeconds = 0;
    m_remainingSeconds = 0;
    m_totalSeconds = 0;
    ChangeState(AppState::Idle);
}

void StateMachine::ExitRestEarly() {
    if (m_state == AppState::Resting) {
        if (m_previousState == AppState::Working && m_config.enableStand && m_config.standMinutes > 0) {
            StartStand();
        } else {
            StartWork();
        }
    }
}

void StateMachine::SkipCurrent() {
    if (m_state == AppState::Working) {
        StartRest();
    } else if (m_state == AppState::Standing) {
        StartWork();
    } else if (m_state == AppState::Resting) {
        ExitRestEarly();
    }
}

void StateMachine::Postpone(int addMinutes) {
    if (m_state == AppState::Working || m_state == AppState::Standing || m_state == AppState::Paused) {
        int addSec = addMinutes * 60;
        m_postponedSeconds += addSec;
        m_remainingSeconds += addSec;
        m_totalSeconds += addSec;
        if (m_useWallClock) {
            // 物理绝对时钟模式下，平移基准开始时间点，使已流逝时间保持连续性
            m_stateStartTime = std::chrono::steady_clock::now() - std::chrono::seconds(m_totalSeconds - m_remainingSeconds);
        }
        if (m_onTick) {
            m_onTick(m_remainingSeconds, m_totalSeconds);
        }
    }
}

void StateMachine::OnSystemSuspendOrLock() {
    if (m_state != AppState::Idle && m_state != AppState::Paused) {
        if (!m_isSuspendedOrLocked) {
            m_isSuspendedOrLocked = true;
            m_suspendStartTick = GetPlatformBootTickMs();
        }
    }
}

void StateMachine::OnSystemResumeOrUnlock() {
    if (m_isSuspendedOrLocked) {
        m_isSuspendedOrLocked = false;
        // 在系统休眠期间自动补偿真实物理流逝时间 (Win32: GetTickCount64, Linux: CLOCK_BOOTTIME)
        uint64_t awayMs = GetPlatformBootTickMs() - m_suspendStartTick;
        int awaySec = static_cast<int>(awayMs / 1000);

        // 离座超过 5 分钟 (300 秒)，说明用户已离开工位活动，人性化自动开启全新工作周期！
        if (awaySec >= 300) {
            StartWork();
        } else {
            // 离座不到 5 分钟 (如短暂离开或锁屏)，精准锚定开始时间点，保持当前阶段剩余倒计时严格不变
            if (m_useWallClock) {
                m_stateStartTime = std::chrono::steady_clock::now() - std::chrono::seconds(m_totalSeconds - m_remainingSeconds);
            }
        }

        if (m_onTick) {
            m_onTick(m_remainingSeconds, m_totalSeconds);
        }
    }
}

void StateMachine::UpdateRestStage() {
    if (m_state != AppState::Resting && m_remainingSeconds == 0) return;

    int elapsed = m_totalSeconds - m_remainingSeconds;
    int oldStage = m_currentRestStage;

    switch (m_config.exerciseMode) {
        case ExerciseMode::Comprehensive: {
            int neckDuration = (std::max)(30, m_totalSeconds / 2);
            if (elapsed < neckDuration) {
                m_currentRestStage = 0;
                m_currentRestStageName = L"阶段 1/2 · 科学颈椎保养操";
            } else {
                m_currentRestStage = 1;
                m_currentRestStageName = L"阶段 2/2 · 20-20-20 护眼与极目远眺";
            }
            break;
        }
        case ExerciseMode::NeckOnly:
            m_currentRestStage = 0;
            m_currentRestStageName = L"科学颈椎保养操";
            break;
        case ExerciseMode::EyeOnly:
            m_currentRestStage = 0;
            m_currentRestStageName = L"20-20-20 科学护眼操";
            break;
        case ExerciseMode::Simple:
            m_currentRestStage = 0;
            m_currentRestStageName = L"工间静心放空休息";
            break;
    }

    if (m_onRestStageChanged && (oldStage != m_currentRestStage || elapsed == 0)) {
        m_onRestStageChanged(m_currentRestStage, m_currentRestStageName, m_remainingSeconds, m_totalSeconds);
    }
}

void StateMachine::Tick() {
    if (m_state == AppState::Idle || m_state == AppState::Paused) {
        return;
    }

    if (m_useWallClock) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_stateStartTime).count();
        m_remainingSeconds = (std::max)(0, m_totalSeconds - static_cast<int>(elapsed));
    } else {
        if (m_remainingSeconds > 0) {
            m_remainingSeconds--;
        }
    }

    if (m_state == AppState::Resting) {
        UpdateRestStage();
    }

    if (m_onTick) {
        m_onTick(m_remainingSeconds, m_totalSeconds);
    }

    if (m_remainingSeconds <= 0) {
        if (m_state == AppState::Working) {
            StartRest();
        } else if (m_state == AppState::Resting) {
            if (m_previousState == AppState::Working && m_config.enableStand && m_config.standMinutes > 0) {
                StartStand();
            } else {
                StartWork();
            }
        } else if (m_state == AppState::Standing) {
            StartWork();
        }
    }
}

