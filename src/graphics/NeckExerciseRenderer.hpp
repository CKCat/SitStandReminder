#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

class NeckExerciseRenderer {
public:
    NeckExerciseRenderer() = default;
    ~NeckExerciseRenderer() = default;

    void SetTotalDuration(float totalSeconds);
    void Update(float dt);
    void Reset();

    void Render(ID2D1RenderTarget* pRT, const D2D1_RECT_F& bounds, float dpiScale = 1.0f);

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

    ComPtr<ID2D1PathGeometry> m_baseHeadGeom;
};

