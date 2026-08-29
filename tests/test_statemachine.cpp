#undef NDEBUG
#include <cassert>
#include <iostream>
#include <string>
#include "core/StateMachine.hpp"
#include "core/ConfigManager.hpp"

void TestInitialState() {
    ReminderConfig config;
    config.workMinutes = 45;
    config.restSeconds = 60;
    config.standMinutes = 15;
    config.enableStand = true;

    StateMachine sm(config);
    assert(sm.GetState() == AppState::Idle);
    assert(sm.GetRemainingSeconds() == 0);
    std::cout << "[PASS] TestInitialState" << std::endl;
}

void TestModeMinimumDurations() {
    assert(ReminderConfig::GetMinRestSecondsForMode(ExerciseMode::Comprehensive) == 60);
    assert(ReminderConfig::GetMinRestSecondsForMode(ExerciseMode::NeckOnly) == 32);
    assert(ReminderConfig::GetMinRestSecondsForMode(ExerciseMode::EyeOnly) == 60);
    assert(ReminderConfig::GetMinRestSecondsForMode(ExerciseMode::Simple) == 5);

    assert(ReminderConfig::GetRecommendedRestSecondsForMode(ExerciseMode::Comprehensive) == 90);
    assert(ReminderConfig::GetRecommendedRestSecondsForMode(ExerciseMode::NeckOnly) == 40);
    assert(ReminderConfig::GetRecommendedRestSecondsForMode(ExerciseMode::EyeOnly) == 60);
    assert(ReminderConfig::GetRecommendedRestSecondsForMode(ExerciseMode::Simple) == 20);

    // 验证 StateMachine 启动休息时自动对低于下限的时长执行安全兜底
    ReminderConfig config;
    config.exerciseMode = ExerciseMode::Comprehensive;
    config.restSeconds = 15; // 试图设置过短时长
    StateMachine sm(config);
    sm.StartRest();
    assert(sm.GetTotalSeconds() == 60); // 自动提升到 60 秒保证做完所有动作

    ReminderConfig configEye;
    configEye.exerciseMode = ExerciseMode::EyeOnly;
    configEye.restSeconds = 20;
    StateMachine smEye(configEye);
    smEye.StartRest();
    assert(smEye.GetTotalSeconds() == 60); // 自动提升到 60 秒保证做完 3 阶段护眼

    ReminderConfig configNeck;
    configNeck.exerciseMode = ExerciseMode::NeckOnly;
    configNeck.restSeconds = 10;
    StateMachine smNeck(configNeck);
    smNeck.StartRest();
    assert(smNeck.GetTotalSeconds() == 32); // 自动提升到 32 秒保证做完 4 大颈椎动作

    std::cout << "[PASS] TestModeMinimumDurations" << std::endl;
}

void TestWorkToRestToStandCycle() {
    ReminderConfig config;
    config.workMinutes = 1; // 60s
    config.restSeconds = 60; // 综合模式最少 60s
    config.standMinutes = 1; // 60s
    config.enableStand = true;
    config.exerciseMode = ExerciseMode::Comprehensive;

    StateMachine sm(config);
    
    int stateChangeCount = 0;
    AppState lastState = AppState::Idle;
    sm.SetOnStateChanged([&](AppState /*oldState*/, AppState newState) {
        stateChangeCount++;
        lastState = newState;
    });

    // Start work
    sm.StartWork();
    assert(sm.GetState() == AppState::Working);
    assert(sm.GetRemainingSeconds() == 60);
    assert(sm.GetTotalSeconds() == 60);

    // Fast-forward 59 seconds
    for (int i = 0; i < 59; ++i) {
        sm.Tick();
        assert(sm.GetState() == AppState::Working);
        assert(sm.GetRemainingSeconds() == 59 - i);
    }

    // Tick the 60th second -> Should transition to Resting
    sm.Tick();
    assert(sm.GetState() == AppState::Resting);
    assert(sm.GetRemainingSeconds() == 60);
    assert(sm.GetTotalSeconds() == 60);

    // Fast-forward 60 seconds in Rest
    for (int i = 0; i < 60; ++i) {
        sm.Tick();
    }

    // After Rest completes -> Should transition to Standing (since enableStand is true)
    assert(sm.GetState() == AppState::Standing);
    assert(sm.GetRemainingSeconds() == 60);

    // Fast-forward 60 seconds in Stand
    for (int i = 0; i < 60; ++i) {
        sm.Tick();
    }

    // After Stand completes -> Should transition back to Working (Sit)
    assert(sm.GetState() == AppState::Working);
    assert(sm.GetRemainingSeconds() == 60);

    std::cout << "[PASS] TestWorkToRestToStandCycle" << std::endl;
}

void TestRestWithoutStandCycle() {
    ReminderConfig config;
    config.workMinutes = 1;
    config.restSeconds = 40;
    config.standMinutes = 0;
    config.enableStand = false;
    config.exerciseMode = ExerciseMode::NeckOnly; // Min is 32s, 40s is recommended

    StateMachine sm(config);
    sm.StartWork();
    
    // Fast-forward 60 seconds
    for (int i = 0; i < 60; ++i) sm.Tick();
    assert(sm.GetState() == AppState::Resting);

    // Fast-forward 40 seconds
    for (int i = 0; i < 40; ++i) sm.Tick();
    // Since enableStand is false / standMinutes == 0, should go directly to Working
    assert(sm.GetState() == AppState::Working);

    std::cout << "[PASS] TestRestWithoutStandCycle" << std::endl;
}

void TestPauseResumeAndSkip() {
    ReminderConfig config;
    config.workMinutes = 10;
    config.restSeconds = 60;

    StateMachine sm(config);
    sm.StartWork();
    assert(sm.GetState() == AppState::Working);

    sm.Tick();
    int rem = sm.GetRemainingSeconds();

    sm.Pause();
    assert(sm.GetState() == AppState::Paused);
    sm.Tick(); // Should not decrease remaining seconds when paused
    assert(sm.GetRemainingSeconds() == rem);

    sm.Resume();
    assert(sm.GetState() == AppState::Working);
    sm.Tick();
    assert(sm.GetRemainingSeconds() == rem - 1);

    // Early trigger Rest
    sm.StartRest();
    assert(sm.GetState() == AppState::Resting);
    assert(sm.GetRemainingSeconds() == 60);

    // Early exit Rest (e.g. Esc pressed)
    sm.ExitRestEarly();
    assert(sm.GetState() == AppState::Standing || sm.GetState() == AppState::Working);

    std::cout << "[PASS] TestPauseResumeAndSkip" << std::endl;
}

void TestComprehensiveRestStages() {
    ReminderConfig config;
    config.workMinutes = 1;
    config.restSeconds = 60;
    config.exerciseMode = ExerciseMode::Comprehensive;

    StateMachine sm(config);
    sm.StartRest();
    assert(sm.GetState() == AppState::Resting);

    int currentStage = -1;
    sm.SetOnRestStageChanged([&](int stage, const std::wstring&, int, int) {
        currentStage = stage;
    });

    // Stage 1: Neck (first half >= 20s, e.g. 30s)
    sm.Tick();
    assert(sm.GetCurrentRestStage() == 0);

    // Fast forward to second half
    for (int i = 0; i < 35; ++i) {
        sm.Tick();
    }
    // Stage 2: Eye exercise
    assert(sm.GetCurrentRestStage() == 1);

    std::cout << "[PASS] TestComprehensiveRestStages" << std::endl;
}

void TestPostpone() {
    ReminderConfig config;
    config.workMinutes = 45;
    config.standMinutes = 15;
    config.restSeconds = 60;

    StateMachine sm(config);
    sm.StartWork();
    assert(sm.GetRemainingSeconds() == 45 * 60);

    // 延期 5 分钟
    sm.Postpone(5);
    assert(sm.GetRemainingSeconds() == 50 * 60);
    assert(sm.GetTotalSeconds() == 50 * 60);

    std::cout << "[PASS] TestPostpone" << std::endl;
}

void TestTrayDisplayConfig() {
    ReminderConfig config;
    assert(config.trayDisplayMode == TrayDisplayMode::DynamicCountdown);

    config.trayDisplayMode = TrayDisplayMode::RunCatHealth;
    assert(config.trayDisplayMode == TrayDisplayMode::RunCatHealth);

    config.trayDisplayMode = TrayDisplayMode::RunCatCpu;
    assert(config.trayDisplayMode == TrayDisplayMode::RunCatCpu);

    std::cout << "[PASS] TestTrayDisplayConfig" << std::endl;
}

void TestSystemSuspendAndResume() {
    ReminderConfig config;
    config.workMinutes = 45;
    config.standMinutes = 15;
    config.restSeconds = 60;

    StateMachine sm(config);
    sm.StartWork();
    assert(sm.GetState() == AppState::Working);
    assert(sm.GetRemainingSeconds() == 45 * 60);

    // 1. 模拟工作单步进行了 10 秒
    for (int i = 0; i < 10; ++i) {
        sm.Tick();
    }
    int remBeforeSuspend = sm.GetRemainingSeconds();
    assert(remBeforeSuspend == 45 * 60 - 10);

    // 2. 短暂锁屏/休眠（模拟离座 1 分钟）
    sm.OnSystemSuspendOrLock();
    assert(sm.IsSuspendedOrLocked() == true);

    // 恢复，离座不到 5 分钟，状态保持 Working，剩余倒计时精确锁定
    sm.OnSystemResumeOrUnlock();
    assert(sm.IsSuspendedOrLocked() == false);
    assert(sm.GetState() == AppState::Working);
    assert(sm.GetRemainingSeconds() == remBeforeSuspend);

    // 3. 唤醒后继续 Tick，时间顺畅单调递减
    sm.Tick();
    assert(sm.GetRemainingSeconds() == remBeforeSuspend - 1);

    std::cout << "[PASS] TestSystemSuspendAndResume" << std::endl;
}

void TestWallClockResumeAnchor() {
    ReminderConfig config;
    config.workMinutes = 45;
    config.standMinutes = 15;
    config.restSeconds = 60;

    StateMachine sm(config);
    sm.SetUseWallClock(true);
    sm.StartWork();

    // 模拟挂起
    sm.OnSystemSuspendOrLock();
    // 恢复：验证绝对物理锚点计算精准锁定
    sm.OnSystemResumeOrUnlock();
    assert(sm.GetRemainingSeconds() == 45 * 60);

    std::cout << "[PASS] TestWallClockResumeAnchor" << std::endl;
}

void TestSystemSuspendLockDebounce() {
    ReminderConfig config;
    config.workMinutes = 45;
    config.standMinutes = 15;
    config.restSeconds = 60;

    StateMachine sm(config);
    sm.StartWork();

    // 模拟先收到锁屏通知
    sm.OnSystemSuspendOrLock();
    ULONGLONG firstTick = sm.GetSuspendStartTick();
    assert(sm.IsSuspendedOrLocked() == true);

    // 模拟短时间后（如休眠前夕）再次收到电源挂起通知
    Sleep(15);
    sm.OnSystemSuspendOrLock();
    ULONGLONG secondTick = sm.GetSuspendStartTick();

    // 关键断言: 第二次通知不应覆盖第一次锁屏的时间戳
    assert(firstTick == secondTick);

    sm.OnSystemResumeOrUnlock();
    assert(sm.IsSuspendedOrLocked() == false);

    std::cout << "[PASS] TestSystemSuspendLockDebounce" << std::endl;
}

void TestPresetsAndConstants() {
    // 1. 验证数学常量
    assert(AppConstants::Math::PI > 3.14159f && AppConstants::Math::PI < 3.14160f);

    // 2. 验证 4 大预设匹配与反查
    assert(AppConstants::PRESET_COUNT == 4);
    for (const auto& p : AppConstants::PRESETS) {
        int matchId = AppConstants::FindMatchingPresetId(p.workMinutes, p.standMinutes, p.restSeconds);
        assert(matchId == p.id);
        const auto* found = AppConstants::GetPresetById(p.id);
        assert(found != nullptr);
        assert(found->menuCmdId == p.menuCmdId);
        assert(found->btnControlId == p.btnControlId);
    }

    // 3. 验证非预设时长返回 -1
    assert(AppConstants::FindMatchingPresetId(37, 12, 45) == -1);

    std::cout << "[PASS] TestPresetsAndConstants" << std::endl;
}

void TestSoundConfigAndTransition() {
    ReminderConfig config;
    config.workMinutes = 1;
    config.restSeconds = 60;
    config.enableSound = true;

    StateMachine sm(config);
    bool soundShouldPlay = false;

    sm.SetOnStateChanged([&](AppState oldState, AppState newState) {
        if (config.enableSound) {
            if (newState == AppState::Resting || (oldState == AppState::Resting && (newState == AppState::Working || newState == AppState::Standing))) {
                soundShouldPlay = true;
            }
        }
    });

    // 1. 进入休息时应触发提示音
    sm.StartRest();
    assert(soundShouldPlay == true);

    // 2. 退出休息时应再次触发提示音
    soundShouldPlay = false;
    sm.ExitRestEarly();
    assert(soundShouldPlay == true);

    std::cout << "[PASS] TestSoundConfigAndTransition" << std::endl;
}

int main() {
    std::cout << "Running StateMachine & Config Unit Tests..." << std::endl;
    TestInitialState();
    TestModeMinimumDurations();
    TestWorkToRestToStandCycle();
    TestRestWithoutStandCycle();
    TestPauseResumeAndSkip();
    TestComprehensiveRestStages();
    TestPostpone();
    TestTrayDisplayConfig();
    TestSystemSuspendAndResume();
    TestWallClockResumeAnchor();
    TestSystemSuspendLockDebounce();
    TestPresetsAndConstants();
    TestSoundConfigAndTransition();
    std::cout << "All 13 Test Suites PASSED successfully!" << std::endl;
    return 0;
}
