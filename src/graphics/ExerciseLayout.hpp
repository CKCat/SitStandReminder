#pragma once

#include "D2DCompat.hpp"
#include <algorithm>

/**
 * @brief 工间操全屏展示安全分区响应式布局计算器
 * 
 * 核心职责：
 * 1. 严格将全屏视口解耦为三大互不相交的安全区域：Header、Central Canvas、Guidance Dock；
 * 2. 彻底消灭硬编码 scale clamp 上限，基于 Canvas 可用高宽进行自适应动态大尺度放大；
 * 3. 保证数学层面上 canvasRect.bottom < dockRect.top，杜绝动画对文字的穿透覆盖。
 */
struct ExerciseLayout {
    D2D1_RECT_F totalBounds{ 0, 0, 0, 0 };
    D2D1_RECT_F headerRect{ 0, 0, 0, 0 };
    D2D1_RECT_F canvasRect{ 0, 0, 0, 0 };
    D2D1_RECT_F dockRect{ 0, 0, 0, 0 };
    D2D1_RECT_F dockLeftRect{ 0, 0, 0, 0 };
    D2D1_RECT_F dockRightRect{ 0, 0, 0, 0 };
    D2D1_POINT_2F dockMeterCenter{ 0, 0 };
    float dockMeterRadius = 0.0f;

    float canvasCenterX = 0.0f;
    float canvasCenterY = 0.0f;
    float animScale = 1.0f;
    float dpiScale = 1.0f;

    static ExerciseLayout Calculate(
        const D2D1_RECT_F& bounds,
        float dpiScale,
        float baseDesignWidth = 480.0f,
        float baseDesignHeight = 280.0f
    ) {
        ExerciseLayout layout;
        layout.totalBounds = bounds;
        layout.dpiScale = (dpiScale > 0.1f) ? dpiScale : 1.0f;

        float w = bounds.right - bounds.left;
        float h = bounds.bottom - bounds.top;
        if (w <= 20.0f || h <= 20.0f) {
            layout.animScale = layout.dpiScale;
            return layout;
        }

        float padX = 24.0f * layout.dpiScale;
        float padY = 16.0f * layout.dpiScale;
        float gapY = 14.0f * layout.dpiScale;

        // 1. Zone A: 顶部栏 Header (高度自适应，保证徽章与节拍器大字号展示)
        float headerH = (std::clamp)(h * 0.075f, 44.0f * layout.dpiScale, 64.0f * layout.dpiScale);
        layout.headerRect = D2D1::RectF(
            bounds.left + padX,
            bounds.top + padY,
            bounds.right - padX,
            bounds.top + padY + headerH
        );

        // 2. Zone B: 底部信息卡片 Guidance Dock (黄金分割双栏架构，充裕呼吸留白与放大排版)
        float dockH = (std::clamp)(h * 0.22f, 160.0f * layout.dpiScale, 240.0f * layout.dpiScale);
        float dockBottom = bounds.bottom - padY;
        float dockTop = dockBottom - dockH;
        layout.dockRect = D2D1::RectF(
            bounds.left + padX,
            dockTop,
            bounds.right - padX,
            dockBottom
        );

        // 黄金分割双栏划分 (左侧 65% 要领文案，右侧 35% 动态仪表盘)
        float dockW = layout.dockRect.right - layout.dockRect.left;
        float dividerGap = 16.0f * layout.dpiScale;
        float splitX = layout.dockRect.left + dockW * 0.65f;

        layout.dockLeftRect = D2D1::RectF(
            layout.dockRect.left,
            layout.dockRect.top,
            splitX - dividerGap * 0.5f,
            layout.dockRect.bottom
        );

        layout.dockRightRect = D2D1::RectF(
            splitX + dividerGap * 0.5f,
            layout.dockRect.top,
            layout.dockRect.right,
            layout.dockRect.bottom
        );

        layout.dockMeterCenter = D2D1::Point2F(
            (layout.dockRightRect.left + layout.dockRightRect.right) * 0.5f,
            (layout.dockRightRect.top + layout.dockRightRect.bottom) * 0.5f
        );

        float rightBoxW = layout.dockRightRect.right - layout.dockRightRect.left;
        float rightBoxH = layout.dockRightRect.bottom - layout.dockRightRect.top;
        layout.dockMeterRadius = (std::min)(rightBoxW * 0.30f, rightBoxH * 0.42f);

        // 3. Zone C: 中央动画视口 Central Canvas (严格介于 Header 与 Dock 之间)
        float canvasTop = layout.headerRect.bottom + gapY;
        float canvasBottom = layout.dockRect.top - gapY;
        if (canvasBottom < canvasTop) {
            canvasBottom = canvasTop;
        }

        layout.canvasRect = D2D1::RectF(
            bounds.left + padX,
            canvasTop,
            bounds.right - padX,
            canvasBottom
        );

        layout.canvasCenterX = layout.canvasRect.left + (layout.canvasRect.right - layout.canvasRect.left) / 2.0f;
        layout.canvasCenterY = layout.canvasRect.top + (layout.canvasRect.bottom - layout.canvasRect.top) / 2.0f;

        // 4. 自适应动画缩放比例计算 (严格以画布可用净高宽为硬上限，杜绝矮屏穿透)
        float cw = layout.canvasRect.right - layout.canvasRect.left;
        float ch = layout.canvasRect.bottom - layout.canvasRect.top;
        if (cw > 10.0f && ch > 10.0f && baseDesignWidth > 10.0f && baseDesignHeight > 10.0f) {
            float scaleW = cw / baseDesignWidth;
            float scaleH = ch / baseDesignHeight;
            float rawScale = (std::min)(scaleW, scaleH);

            // 自适应缩放比例：严格以画布可用净高宽为硬上限 (确保 animScale <= rawScale)，
            // 杜绝在矮屏或小窗口下因硬编码 minScale 穿透 Header 与 Guidance Dock。
            // 在高分辨率大屏下允许放大至 4.5x DPI 基准。
            float maxScale = 4.5f * layout.dpiScale;
            float targetScale = (std::min)(rawScale, maxScale);
            layout.animScale = (std::max)(0.1f, targetScale);
        } else {
            layout.animScale = layout.dpiScale;
        }

        return layout;
    }
};

