#pragma once

#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>
#include "../core/ConfigManager.hpp"
#include "../core/StateMachine.hpp"

using Microsoft::WRL::ComPtr;

class MascotRenderer {
public:
    static MascotRenderer& Instance() {
        static MascotRenderer instance;
        return instance;
    }

    // 在指定中心点与包围盒绘制伴侣形象与进度
    void DrawMascotFloating(
        ID2D1RenderTarget* pTarget,
        D2D1_POINT_2F center,
        float radius,
        MascotTheme theme,
        AppState state,
        int remainingSec,
        int totalSec,
        int animTick,
        D2D1_COLOR_F accentColor,
        float scale,
        bool isDark = true
    );

    // 在外露贴边小拉手（Dock Tab）上绘制微型伴侣头像与状态指示
    void DrawMascotDockTab(
        ID2D1RenderTarget* pTarget,
        D2D1_RECT_F tabRect,
        MascotTheme theme,
        AppState state,
        int remainingSec,
        int totalSec,
        int animTick,
        D2D1_COLOR_F accentColor,
        bool isLeftEdge,
        float scale,
        bool isDark = true
    );

    // 绘制外边框圆角矩形流光进度条 (从顶部 12 点钟方向顺时针流转)
    void DrawRoundedRectProgress(
        ID2D1RenderTarget* pTarget,
        D2D1_RECT_F rect,
        float radius,
        float strokeW,
        float progress,
        D2D1_COLOR_F accentColor
    );

private:
    MascotRenderer() = default;

    void DrawCapybara(ID2D1RenderTarget* pTarget, ID2D1SolidColorBrush* pBrush, D2D1_POINT_2F center, float radius, AppState state, int animTick, float scale, bool isDark = true);
    void DrawPixelCat(ID2D1RenderTarget* pTarget, ID2D1SolidColorBrush* pBrush, D2D1_POINT_2F center, float radius, AppState state, int animTick, float scale, bool isDark = true);
    void DrawCyberBot(ID2D1RenderTarget* pTarget, ID2D1SolidColorBrush* pBrush, D2D1_POINT_2F center, float radius, AppState state, int animTick, float scale, bool isDark = true);
    void DrawProgressRing(ID2D1RenderTarget* pTarget, D2D1_POINT_2F center, float radius, float strokeW, float progress, D2D1_COLOR_F accentColor);
};
