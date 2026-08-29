#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "StateMachine.hpp"
#include <algorithm>

StateMachine::StateMachine(const ReminderConfig& config)
    : m_config(config) {
}

void StateMachine::SetConfig(const ReminderConfig& config) {
    m_config = config;

    if (m_state == AppState::Working) {
        int newTotal = (std::max)(1, m_config.workMinutes * 60);
        if (m_totalSeconds != newTotal) {
            if (m_remainingSeconds > newTotal) {
                m_remainingSeconds = newTotal;
            }
            m_totalSeconds = newTotal;
            m_stateStartTime = std::chrono::steady_clock::now() - std::chrono::seconds(m_totalSeconds - m_remainingSeconds);
        }
    } else if (m_state == AppState::Standing) {
        int newTotal = (std::max)(1, m_config.standMinutes * 60);
        if (m_totalSeconds != newTotal) {
            if (m_remainingSeconds > newTotal) {
                m_remainingSeconds = newTotal;
            }
            m_totalSeconds = newTotal;
            m_stateStartTime = std::chrono::steady_clock::now() - std::chrono::seconds(m_totalSeconds - m_remainingSeconds);
        }
    } else if (m_state == AppState::Resting) {
        int newTotal = (std::max)(m_config.GetMinRestSeconds(), m_config.restSeconds);
        m_totalSeconds = newTotal;
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
}

void StateMachine::StartWork() {
    m_totalSeconds = (std::max)(1, m_config.workMinutes * 60);
    m_remainingSeconds = m_totalSeconds;
    m_stateStartTime = std::chrono::steady_clock::now();
    ChangeState(AppState::Working);
}

void StateMachine::StartStand() {
    m_totalSeconds = (std::max)(1, m_config.standMinutes * 60);
    m_remainingSeconds = m_totalSeconds;
    m_stateStartTime = std::chrono::steady_clock::now();
    ChangeState(AppState::Standing);
}

void StateMachine::StartRest() {
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
    if (m_state == AppState::Working || m_state == AppState::Standing) {
        int addSec = addMinutes * 60;
        m_remainingSeconds += addSec;
        m_totalSeconds += addSec;
        if (m_onTick) {
            m_onTick(m_remainingSeconds, m_totalSeconds);
        }
    }
}

void StateMachine::OnSystemSuspendOrLock() {
    if (m_state != AppState::Idle && m_state != AppState::Paused) {
        if (!m_isSuspendedOrLocked) {
            m_isSuspendedOrLocked = true;
            m_suspendStartTick = GetTickCount64();
        }
    }
}

void StateMachine::OnSystemResumeOrUnlock() {
    if (m_isSuspendedOrLocked) {
        m_isSuspendedOrLocked = false;
        // GetTickCount64 在系统休眠期间由 Windows 内核自动补偿，包含真实物理流逝时间
        ULONGLONG awayMs = GetTickCount64() - m_suspendStartTick;
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
                m_currentRestStageName = L"阶段 1/2 · 🧘 科学颈椎保养操";
            } else {
                m_currentRestStage = 1;
                m_currentRestStageName = L"阶段 2/2 · 👁️ 20-20-20 护眼与极目远眺";
            }
            break;
        }
        case ExerciseMode::NeckOnly:
            m_currentRestStage = 0;
            m_currentRestStageName = L"🧘 科学颈椎保养操";
            break;
        case ExerciseMode::EyeOnly:
            m_currentRestStage = 0;
            m_currentRestStageName = L"👁️ 20-20-20 科学护眼操";
            break;
        case ExerciseMode::Simple:
            m_currentRestStage = 0;
            m_currentRestStageName = L"🍃 极简工间放空休息";
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

