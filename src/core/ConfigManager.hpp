#pragma once

#include "AppConstants.hpp"

enum class ExerciseMode : int {
    Comprehensive = 0, // 综合工间操（颈椎 + 护眼）
    NeckOnly = 1,      // 科学颈椎操
    EyeOnly = 2,       // 20-20-20 护眼操
    Simple = 3         // 极简放空休息
};

enum class ThemeMode : int {
    System = 0, // 跟随系统
    Light = 1,  // 浅色模式
    Dark = 2    // 深色模式
};

enum class MascotTheme : int {
    Minimalist = 0, // 极简商务 (纯净能量环)
    Capybara = 1,   // 佛系水豚 (治愈松弛)
    PixelCat = 2,   // 灵动像素猫 (萌宠陪伴)
    CyberBot = 3    // 赛博小助手 (科技极客)
};

enum class TrayDisplayMode : int {
    DefaultIcon = 0,      // 经典静态图标
    DynamicCountdown = 1, // 动态数字倒计时 (显示剩余分钟数 + 微型进度环)
    RunCatHealth = 2,     // 状态感应 RunCat (根据办公状态与紧迫度变速)
    RunCatCpu = 3         // CPU 负载 RunCat (根据系统实时 CPU 占用率变速)
};

enum class BorderWidth : int {
    Thin = 0,       // 细线 (1.5px)
    Medium = 1,     // 标准 (2.5px)
    Thick = 2,      // 加粗 (3.5px)
    ExtraThick = 3  // 醒目极粗 (4.5px)
};

inline float GetStrokeWidthForBorder(BorderWidth bw) {
    switch (bw) {
        case BorderWidth::Thin:       return 1.5f;
        case BorderWidth::Medium:     return 2.5f;
        case BorderWidth::Thick:      return 3.5f;
        case BorderWidth::ExtraThick: return 4.5f;
        default:                      return 2.5f;
    }
}

inline float GetTabStrokeWidthForBorder(BorderWidth bw) {
    switch (bw) {
        case BorderWidth::Thin:       return 1.2f;
        case BorderWidth::Medium:     return 2.0f;
        case BorderWidth::Thick:      return 2.8f;
        case BorderWidth::ExtraThick: return 3.6f;
        default:                      return 2.0f;
    }
}

struct ReminderConfig {
    int workMinutes = 45;
    int restSeconds = 90;
    int standMinutes = 15;
    bool enableStand = true;
    bool blockInput = true;
    bool strongReminder = true;
    bool alwaysTopMost = true;
    bool multiScreen = true;
    bool enableSound = true;
    bool autoStart = false;
    bool enableEdgeDock = true;
    ThemeMode themeMode = ThemeMode::System;
    ExerciseMode exerciseMode = ExerciseMode::Comprehensive;
    MascotTheme mascotTheme = MascotTheme::Minimalist;
    TrayDisplayMode trayDisplayMode = TrayDisplayMode::DynamicCountdown;
    BorderWidth borderWidth = BorderWidth::Medium;

    // 获取特定模式下的最小安全休息秒数（确保所有动作/法则都能完整做完）
    static int GetMinRestSecondsForMode(ExerciseMode mode) {
        switch (mode) {
            case ExerciseMode::Comprehensive: return 60; // 颈椎4动作(30s) + 护眼3法则(30s)，紧凑全量做完需60s
            case ExerciseMode::NeckOnly: return 32;      // 颈椎4动作，每个动作生理有效拉伸至少8s
            case ExerciseMode::EyeOnly: return 60;       // 护眼3阶段(远眺20s+闭目20s+追踪20s)，标准做完需60s
            case ExerciseMode::Simple: return 5;         // 极简放空模式，无动作序列，不限制最小时长
            default: return 5;
        }
    }

    int GetMinRestSeconds() const {
        return GetMinRestSecondsForMode(exerciseMode);
    }

    // 获取特定模式下的推荐休息秒数
    static int GetRecommendedRestSecondsForMode(ExerciseMode mode) {
        switch (mode) {
            case ExerciseMode::Comprehensive: return 90; // 满血推荐：颈椎40s + 护眼50s
            case ExerciseMode::NeckOnly: return 40;      // 满血推荐：4大动作各10s
            case ExerciseMode::EyeOnly: return 60;       // 满血推荐：3大阶段各20s
            case ExerciseMode::Simple: return 20;
            default: return 20;
        }
    }

    int GetRecommendedRestSeconds() const {
        return GetRecommendedRestSecondsForMode(exerciseMode);
    }
};

class ConfigManager {
public:
    static ConfigManager& Instance() {
        static ConfigManager instance;
        return instance;
    }

    void Load();
    void Save();

    const ReminderConfig& GetConfig() const { return m_config; }
    ReminderConfig& GetConfig() { return m_config; }
    void SetConfig(const ReminderConfig& config) { m_config = config; }

    bool IsAutoStartEnabled() const;
    void SetAutoStartEnabled(bool enable);

    void ApplyPreset(int workMin, int standMin, int restSec);

private:
    ConfigManager();
    ~ConfigManager() = default;

    ReminderConfig m_config;
    const std::wstring m_regKeyPath = AppConstants::Identity::REGISTRY_KEY_PATH;
};

