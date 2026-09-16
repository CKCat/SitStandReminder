#undef NDEBUG
#include "graphics/D2DCompat.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include "core/StateMachine.hpp"
#include "core/ConfigManager.hpp"
#include "graphics/ExerciseLayout.hpp"

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

void TestPostponeInPausedState() {
    std::cout << "[RUN] TestPostponeInPausedState..." << std::endl;
    ReminderConfig config;
    config.workMinutes = 45;
    config.standMinutes = 15;
    config.restSeconds = 60;

    StateMachine sm(config);
    sm.StartWork();
    int remBefore = sm.GetRemainingSeconds(); // 45 * 60

    sm.Pause();
    assert(sm.GetState() == AppState::Paused);

    // 在 Paused 状态下调用 Postpone(5)
    sm.Postpone(5);
    // 关键红灯验证：剩余时间必须为 50 * 60，绝不可被忽略！
    assert(sm.GetRemainingSeconds() == remBefore + 5 * 60);

    sm.Resume();
    assert(sm.GetState() == AppState::Working);
    assert(sm.GetRemainingSeconds() == remBefore + 5 * 60);
    std::cout << "[PASS] TestPostponeInPausedState" << std::endl;
}

void TestConfigManagerCommentParsing() {
    std::cout << "[RUN] TestConfigManagerCommentParsing..." << std::endl;
    std::string iniContent =
        "# 这是井号注释: WorkMinutes = 99\n"
        "; 这是分号注释: WorkMinutes = 88\n"
        "   # 缩进注释: StandMinutes = 77\n"
        "WorkMinutes = 35\n"
        "   ; 缩进分号注释: RestSeconds = 999\n"
        "StandMinutes = 12\n"
        "RestSeconds = 45\n"
        "[General]\n"
        "EnableSound = false\n";

    std::istringstream iss(iniContent);
    ReminderConfig cfg;
    ConfigManager::ParseIniStream(iss, cfg);

    assert(cfg.workMinutes == 35);
    assert(cfg.standMinutes == 12);
    assert(cfg.restSeconds == 45);
    assert(cfg.enableSound == false);
    std::cout << "[PASS] TestConfigManagerCommentParsing" << std::endl;
}

void TestPostponeWallClock() {
    ReminderConfig config;
    config.workMinutes = 45;
    config.standMinutes = 15;
    config.restSeconds = 60;

    StateMachine sm(config);
    sm.SetUseWallClock(true);
    sm.StartWork();
    assert(sm.GetRemainingSeconds() == 45 * 60);

    // 延期 5 分钟 (300秒)
    sm.Postpone(5);
    assert(sm.GetRemainingSeconds() == 50 * 60);
    assert(sm.GetTotalSeconds() == 50 * 60);

    // 模拟等待 10ms 并触发 Tick()
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    sm.Tick();
    // 关键断言: 在物理时间流逝仅数毫秒的情况下，剩余时间应当依然保持为 50*60 (3000s)
    assert(sm.GetRemainingSeconds() >= 50 * 60 - 1);

    // 再次调用 SetConfig 重新加载配置 (例如设置中心保存配置)
    sm.SetConfig(config);
    // 关键断言: 重新 SetConfig 后，推迟后的 50 分钟绝不可被粗暴截断或抹平回 45 分钟！
    assert(sm.GetRemainingSeconds() == 50 * 60);
    assert(sm.GetTotalSeconds() == 50 * 60);

    std::cout << "[PASS] TestPostponeWallClock" << std::endl;
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
    uint64_t firstTick = sm.GetSuspendStartTick();
    assert(sm.IsSuspendedOrLocked() == true);

    // 模拟短时间后（如休眠前夕）再次收到电源挂起通知
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    sm.OnSystemSuspendOrLock();
    uint64_t secondTick = sm.GetSuspendStartTick();

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

void TestBorderWidthConfig() {
    ReminderConfig config;
    assert(config.borderWidth == BorderWidth::Medium);

    // 验证悬浮窗线宽映射
    assert(GetStrokeWidthForBorder(BorderWidth::Thin) == 1.5f);
    assert(GetStrokeWidthForBorder(BorderWidth::Medium) == 2.5f);
    assert(GetStrokeWidthForBorder(BorderWidth::Thick) == 3.5f);
    assert(GetStrokeWidthForBorder(BorderWidth::ExtraThick) == 4.5f);
    assert(GetStrokeWidthForBorder(static_cast<BorderWidth>(99)) == 2.5f);

    // 验证折叠拉手微型线宽映射
    assert(GetTabStrokeWidthForBorder(BorderWidth::Thin) == 1.2f);
    assert(GetTabStrokeWidthForBorder(BorderWidth::Medium) == 2.0f);
    assert(GetTabStrokeWidthForBorder(BorderWidth::Thick) == 2.8f);
    assert(GetTabStrokeWidthForBorder(BorderWidth::ExtraThick) == 3.6f);
    assert(GetTabStrokeWidthForBorder(static_cast<BorderWidth>(99)) == 2.0f);

    config.borderWidth = BorderWidth::ExtraThick;
    assert(config.borderWidth == BorderWidth::ExtraThick);

    std::cout << "[PASS] TestBorderWidthConfig" << std::endl;
}

void TestReminderConfigEquality() {
    ReminderConfig c1;
    ReminderConfig c2;
    assert(c1 == c2);
    assert(!(c1 != c2));

    // 测试工作时长差异
    c2.workMinutes = 50;
    assert(c1 != c2);
    assert(!(c1 == c2));
    c2.workMinutes = 45;
    assert(c1 == c2);

    // 测试布尔开关差异
    c2.strongReminder = !c1.strongReminder;
    assert(c1 != c2);
    c2.strongReminder = c1.strongReminder;
    assert(c1 == c2);

    // 测试边框线宽差异
    c2.borderWidth = BorderWidth::ExtraThick;
    assert(c1 != c2);
    c2.borderWidth = c1.borderWidth;
    assert(c1 == c2);

    // 测试托盘风格差异
    c2.trayDisplayMode = TrayDisplayMode::RunCatCpu;
    assert(c1 != c2);
    c2.trayDisplayMode = c1.trayDisplayMode;
    assert(c1 == c2);

    std::cout << "[PASS] TestReminderConfigEquality" << std::endl;
}

void TestExerciseLayout() {
    // 1. Happy Path: 1920x1080 标准 1080P
    D2D1_RECT_F bounds1080 = D2D1::RectF(0, 0, 1920, 1080);
    auto layout = ExerciseLayout::Calculate(bounds1080, 1.0f, 480.0f, 280.0f);

    // 基础三段式互斥性
    assert(layout.headerRect.top >= bounds1080.top);
    assert(layout.headerRect.bottom < layout.canvasRect.top);
    assert(layout.canvasRect.bottom < layout.dockRect.top);
    assert(layout.dockRect.bottom <= bounds1080.bottom);

    // 黄金分割双栏几何断言
    assert(layout.dockLeftRect.left == layout.dockRect.left);
    assert(layout.dockLeftRect.top == layout.dockRect.top);
    assert(layout.dockLeftRect.bottom == layout.dockRect.bottom);
    assert(layout.dockRightRect.right == layout.dockRect.right);
    assert(layout.dockRightRect.top == layout.dockRect.top);
    assert(layout.dockRightRect.bottom == layout.dockRect.bottom);

    // 左右两栏必须有正向间距 (Divider Gap)
    assert(layout.dockLeftRect.right < layout.dockRightRect.left);

    // 左栏宽度约占 60%~70% 黄金比例
    float totalDockW = layout.dockRect.right - layout.dockRect.left;
    float leftW = layout.dockLeftRect.right - layout.dockLeftRect.left;
    float leftRatio = leftW / totalDockW;
    assert(leftRatio >= 0.60f && leftRatio <= 0.70f);

    // 右侧仪表盘中心与有效半径
    assert(layout.dockMeterCenter.x > layout.dockRightRect.left);
    assert(layout.dockMeterCenter.x < layout.dockRightRect.right);
    assert(layout.dockMeterRadius >= 20.0f);
    assert(layout.dockMeterRadius <= (layout.dockRightRect.bottom - layout.dockRightRect.top) * 0.5f);

    // 2. 4K 高分屏 (3840x2160, dpi=2.0)
    D2D1_RECT_F bounds4K = D2D1::RectF(0, 0, 3840, 2160);
    auto layout4K = ExerciseLayout::Calculate(bounds4K, 2.0f, 480.0f, 280.0f);
    assert(layout4K.dockLeftRect.right < layout4K.dockRightRect.left);
    assert(layout4K.animScale >= 2.0f);
    assert(layout4K.dockMeterRadius >= 40.0f);

    // 3. 边界退化情况 (小分辨率)
    D2D1_RECT_F boundsSmall = D2D1::RectF(0, 0, 10, 10);
    auto layoutSmall = ExerciseLayout::Calculate(boundsSmall, 1.0f);
    assert(layoutSmall.animScale > 0.0f);

    std::cout << "[PASS] TestExerciseLayout" << std::endl;
}

void TestStateTransitionTriggersOnTickImmediately() {
    ReminderConfig config;
    config.workMinutes = 30;
    config.standMinutes = 10;
    config.restSeconds = 60;
    StateMachine sm(config);

    int tickCount = 0;
    int lastRemain = -1;
    int lastTotal = -1;
    sm.SetOnTick([&](int remain, int total) {
        tickCount++;
        lastRemain = remain;
        lastTotal = total;
    });

    sm.StartWork();
    assert(tickCount == 1);
    assert(lastRemain == 30 * 60);
    assert(lastTotal == 30 * 60);

    sm.StartStand();
    assert(tickCount == 2);
    assert(lastRemain == 10 * 60);
    assert(lastTotal == 10 * 60);

    sm.StartRest();
    assert(tickCount == 3);
    assert(lastRemain == 60);
    assert(lastTotal == 60);

    std::cout << "[PASS] TestStateTransitionTriggersOnTickImmediately" << std::endl;
}

void TestSetConfigWallClockContinuity() {
    ReminderConfig config;
    config.workMinutes = 45; // 2700s
    config.restSeconds = 60;
    config.standMinutes = 15;
    config.enableStand = true;

    StateMachine sm(config);
    sm.SetUseWallClock(false);
    sm.StartWork();

    // 模拟工作已经流逝了 15 分钟 (900 秒)，剩余 30 分钟 (1800 秒)
    for (int i = 0; i < 900; ++i) {
        sm.Tick();
    }
    assert(sm.GetElapsedSeconds() == 900);
    assert(sm.GetRemainingSeconds() == 1800);

    // 启用 WallClock 并修改配置：用户在设置中心将工作时长改为 50 分钟 (3000 秒)
    sm.SetUseWallClock(true);
    ReminderConfig newConfig = config;
    newConfig.workMinutes = 50;
    sm.SetConfig(newConfig);

    // 核心断言：总时长增加 300 秒，已流逝时间 900 秒保持不变，剩余时间必须正确增加为 2100 秒 (35分钟)
    assert(sm.GetTotalSeconds() == 3000);
    assert(sm.GetRemainingSeconds() == 2100);
    assert(sm.GetElapsedSeconds() == 900);

    std::cout << "[PASS] TestSetConfigWallClockContinuity" << std::endl;
}

void TestClearConfigResetsMemory() {
    auto& cm = ConfigManager::Instance();
    ReminderConfig customConfig;
    customConfig.workMinutes = 99;
    customConfig.standMinutes = 77;
    customConfig.restSeconds = 250;
    cm.SetConfig(customConfig);
    cm.Save();

    assert(cm.GetConfig().workMinutes == 99);

    // 清除配置
    bool cleared = cm.ClearConfig();
    assert(cleared);

    // 核心断言：不仅磁盘文件删除，内存单例中必须完全恢复为出厂默认值 (45m, 15m, 90s)
    assert(cm.GetConfig().workMinutes == 45);
    assert(cm.GetConfig().standMinutes == 15);
    assert(cm.GetConfig().restSeconds == 90);

    std::cout << "[PASS] TestClearConfigResetsMemory" << std::endl;
}

void TestStandingToWorkTransition() {
    ReminderConfig config;
    config.workMinutes = 1;
    config.restSeconds = 60;
    config.standMinutes = 1;
    config.enableStand = true;

    StateMachine sm(config);
    sm.StartStand();
    assert(sm.GetState() == AppState::Standing);

    // 快进 60 秒站立办公
    for (int i = 0; i < 60; ++i) {
        sm.Tick();
    }

    // 坐站健康循环：站立办公结束后，自动开启坐姿办公，剩余时间重置为 60 秒
    assert(sm.GetState() == AppState::Working);
    assert(sm.GetRemainingSeconds() == 60);

    std::cout << "[PASS] TestStandingToWorkTransition" << std::endl;
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
    TestPostponeInPausedState();
    TestPostponeWallClock();
    TestTrayDisplayConfig();
    TestBorderWidthConfig();
    TestReminderConfigEquality();
    TestSystemSuspendAndResume();
    TestWallClockResumeAnchor();
    TestSystemSuspendLockDebounce();
    TestPresetsAndConstants();
    TestSoundConfigAndTransition();
    TestExerciseLayout();
    TestStateTransitionTriggersOnTickImmediately();
    TestSetConfigWallClockContinuity();
    TestClearConfigResetsMemory();
    TestStandingToWorkTransition();
    TestConfigManagerCommentParsing();
    std::cout << "All 22 Test Suites PASSED successfully!" << std::endl;
    return 0;
}

