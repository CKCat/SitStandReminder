#pragma once

#include "D2DCompat.hpp"
#include <string>

class NeckExerciseRenderer {
public:
    NeckExerciseRenderer() = default;
    ~NeckExerciseRenderer() = default;

    void SetTotalDuration(float totalSeconds);
    void Update(float dt);
    void Reset();

    void Render(ID2D1RenderTarget* pRT, const D2D1_RECT_F& bounds, float dpiScale = 1.0f);
    void RenderStatic(ID2D1RenderTarget* pRT, const D2D1_RECT_F& bounds, float dpiScale = 1.0f);
    void RenderDynamic(ID2D1RenderTarget* pRT, const D2D1_RECT_F& bounds, float dpiScale = 1.0f);

    int GetCurrentPhase() const { return m_currentPhase; }
    float GetPhaseProgress() const;

private:
    float m_animTime = 0.0f;
    float m_phaseDuration = 5.0f;
    int m_currentPhase = 0; // 0: 缩下巴, 1: 缓慢后仰, 2: 左侧拉伸, 3: 右侧拉伸

    static float EaseInOutCubic(float x);
    static void CalculateStretchPacing(float tNorm, float& ease, int& segment, float& segmentProgress);

    void DrawProfileHead(
        ID2D1RenderTarget* pRT,
        ID2D1SolidColorBrush* pBrush,
        ID2D1Factory* pFactory,
        float hx,
        float hy,
        float headRadius,
        float scale,
        bool isTucking,
        float tuckEase,
        bool isExtending
    );

    void DrawAnatomicalTorso(
        ID2D1RenderTarget* pRT,
        ID2D1SolidColorBrush* pBrush,
        float cx,
        float cy,
        float animScale,
        float ease,
        int stretchSide,
        float canvasBottom = 0.0f
    );

    void DrawCircularPacer(
        ID2D1RenderTarget* pRT,
        ID2D1SolidColorBrush* pBrush,
        const D2D1_POINT_2F& center,
        float radius,
        float dpiScale,
        int segment,
        float segProgress,
        float tNorm
    );

    ComPtr<ID2D1PathGeometry> m_baseHeadGeom;
    ComPtr<ID2D1PathGeometry> m_baseTorsoGeom;
    ComPtr<ID2D1PathGeometry> m_baseLeftClavGeom;
    ComPtr<ID2D1PathGeometry> m_baseRightClavGeom;
    ComPtr<ID2D1PathGeometry> m_baseLeftRibGeom;
    ComPtr<ID2D1PathGeometry> m_baseRightRibGeom;
    ComPtr<ID2D1PathGeometry> m_baseTrapGeom;
    int m_lastTrapStretchSide = 0;

    float m_lastAnimScale = 0.0f;
    float m_lastCanvasBottom = 0.0f;
    float m_lastCx = 0.0f;
    float m_lastCy = 0.0f;
};

