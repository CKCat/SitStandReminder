#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include "Version.hpp"

namespace AppConstants {

// 1. 软件标识与系统级对象常量 (单一真相源 SSOT)
namespace Identity {
    inline constexpr const wchar_t* NAME               = L"SitStandReminder";
    inline constexpr const wchar_t* DISPLAY_NAME       = L"坐立提醒";
    inline constexpr const wchar_t* SLOGAN             = L"科学坐立与工位健康伴侣";
    inline constexpr const wchar_t* MUTEX_NAME         = L"SitStandReminderSingleInstanceMutex";
    inline constexpr const wchar_t* REGISTRY_KEY_PATH  = L"Software\\SitStandReminder";
    inline constexpr const wchar_t* RUN_AUTORUN_NAME   = L"SitStandReminder";

    // 窗口类名与标题
    inline constexpr const wchar_t* CLASS_TRAY         = L"SitStandReminderTrayClass";
    inline constexpr const wchar_t* TITLE_TRAY         = L"SitStandReminderTray";
    inline constexpr const wchar_t* TRAY_DEFAULT_TIP   = L"坐立提醒 · 科学坐立与工位健康伴侣";

    inline constexpr const wchar_t* CLASS_FLOATING     = L"SitStandReminderFloatingClass";
    inline constexpr const wchar_t* TITLE_FLOATING     = L"SitStandReminderFloating";

    inline constexpr const wchar_t* CLASS_SETTINGS     = L"SitStandReminderSettingsClass";
    inline constexpr const wchar_t* TITLE_SETTINGS     = L"坐立提醒 · 设置中心";

    inline constexpr const wchar_t* CLASS_MASK         = L"SitStandReminderFullscreenClass";
    inline constexpr const wchar_t* TITLE_MASK         = L"SitStandReminderFullscreen";
}

// 悬浮窗设计尺寸常量 (96 DPI 逻辑基准)
namespace FloatingWindowDimensions {
    inline constexpr int BASE_WIDTH  = 148;
    inline constexpr int BASE_HEIGHT = 68;
    inline constexpr int DOCK_TAB_WIDTH = 32;
}

// 全局数学与物理常量
namespace Math {
    inline constexpr float PI = 3.14159265358979323846f;
}

// 2. 周期预设元数据模型与常量表
struct PresetInfo {
    int id;                      // 1, 2, 3, 4
    WORD menuCmdId;              // 托盘菜单命令 ID
    WORD btnControlId;           // 设置窗口按钮控件 ID
    const wchar_t* buttonLabel;  // 按钮标题 (如 "45m 坐 / 15m 站")
    const wchar_t* menuLabel;    // 菜单文本 (如 "预设：45m 坐 / 15m 站")
    int workMinutes;             // 坐姿工作时长 (分)
    int standMinutes;            // 站立办公时长 (分)
    int restSeconds;             // 工间休息时长 (秒)
};

// 全局 4 大科学周期预设常量表
inline constexpr PresetInfo PRESETS[] = {
    { 1, 2011, 3001, L"45m 坐 / 15m 站",   L"预设：45m 坐 / 15m 站",   45, 15, 90 },
    { 2, 2012, 3002, L"50m 坐 / 10m 站",   L"预设：50m 坐 / 10m 站",   50, 10, 90 },
    { 3, 2013, 3003, L"25m 番茄工作法",    L"预设：25m 番茄工作法",    25,  5, 60 },
    { 4, 2014, 3004, L"60m 深度攻坚",      L"预设：60m 深度攻坚",      60, 20, 90 }
};

inline constexpr size_t PRESET_COUNT = sizeof(PRESETS) / sizeof(PRESETS[0]);

// 根据当前时长快速匹配预设 ID (1~4)，若无匹配则返回 -1
inline int FindMatchingPresetId(int workMin, int standMin, int restSec) {
    for (const auto& p : PRESETS) {
        if (p.workMinutes == workMin && p.standMinutes == standMin && p.restSeconds == restSec) {
            return p.id;
        }
    }
    return -1;
}

// 根据预设 ID (1~4) 查找预设定义
inline const PresetInfo* GetPresetById(int id) {
    for (const auto& p : PRESETS) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

// 根据命令 ID (menuCmdId) 查找预设定义
inline const PresetInfo* GetPresetByMenuCmd(WORD menuCmdId) {
    for (const auto& p : PRESETS) {
        if (p.menuCmdId == menuCmdId) return &p;
    }
    return nullptr;
}

} // namespace AppConstants
