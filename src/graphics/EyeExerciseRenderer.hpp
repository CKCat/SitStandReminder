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

class EyeExerciseRenderer {
public:
    EyeExerciseRenderer() = default;
    ~EyeExerciseRenderer() = default;

    void SetTotalDuration(float totalSeconds);
    void Update(float dt);
    void Reset();

    void Render(ID2D1RenderTarget* pRT, const D2D1_RECT_F& bounds, float dpiScale = 1.0f);

    int GetCurrentPhase() const { return m_currentPhase; }
    float GetPhaseProgress() const;

private:
    float m_animTime = 0.0f;
    float m_phaseDuration = 6.6f;
    int m_currentPhase = 0; // 0: 极目远眺, 1: 深度闭目, 2: 视线追踪

    void DrawBlinkingEye(ID2D1RenderTarget* pRT, float eyeX, float eyeY, float closeAmount, float scale, ID2D1SolidColorBrush* pBrush, float pupilOffsetX = 0.0f, float pupilOffsetY = 0.0f);
    void DrawEyeMonitorWindow(ID2D1RenderTarget* pRT, float cx, float cy, float targetX, float targetY, float scale, ID2D1SolidColorBrush* pBrush);

    void DrawCosmicExpansion(
        ID2D1RenderTarget* pRT,
        ID2D1SolidColorBrush* pBrush,
        float cx,
        float cy,
        float cw,
        float ch,
        float animScale,
        float t
    );

    void DrawBreathingHalo(
        ID2D1RenderTarget* pRT,
        ID2D1SolidColorBrush* pBrush,
        float cx,
        float cy,
        float animScale,
        float breathExp,
        int breathState,
        float stateProgress
    );

    void DrawEyeCircularPacer(
        ID2D1RenderTarget* pRT,
        ID2D1SolidColorBrush* pBrush,
        const D2D1_POINT_2F& center,
        float radius,
        float dpiScale,
        int phase,
        float tNorm,
        int breathState = 0,
        float breathRemain = 0.0f
    );

    ComPtr<ID2D1PathGeometry> m_baseRayGeom;
};

