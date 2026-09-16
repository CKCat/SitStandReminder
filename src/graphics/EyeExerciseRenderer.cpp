#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "EyeExerciseRenderer.hpp"
#include "D2DContext.hpp"
#include "ExerciseLayout.hpp"
#include "../core/AppConstants.hpp"
#include <cmath>
#include <algorithm>
#include <vector>

void EyeExerciseRenderer::SetTotalDuration(float totalSeconds) {
    m_phaseDuration = (std::max)(2.0f, totalSeconds / 3.0f);
    m_animTime = 0.0f;
    m_currentPhase = 0;
}

void EyeExerciseRenderer::Update(float dt) {
    m_animTime += dt;
    m_currentPhase = static_cast<int>(m_animTime / m_phaseDuration) % 3;
}

void EyeExerciseRenderer::Reset() {
    m_animTime = 0.0f;
    m_currentPhase = 0;
}

float EyeExerciseRenderer::GetPhaseProgress() const {
    float phaseTime = std::fmod(m_animTime, m_phaseDuration);
    return phaseTime / m_phaseDuration;
}

void EyeExerciseRenderer::DrawBlinkingEye(
    ID2D1RenderTarget* pRT,
    float eyeX,
    float eyeY,
    float closeAmount,
    float scale,
    ID2D1SolidColorBrush* pBrush,
    float pupilOffsetX,
    float pupilOffsetY
) {
    float eyeW = 44.0f * scale;
    float eyeH = 24.0f * scale * (1.0f - closeAmount * 0.88f);

    if (closeAmount >= 0.85f) {
        // 仿生祥和闭目眼睑 (复用预烘焙基准几何体，消除 60FPS 渲染热路径堆分配)
        auto& d2d = D2DContext::Instance();
        if (!m_baseLidGeom || std::abs(m_lastEyeW - eyeW) > 1e-3f || std::abs(m_lastEyeScale - scale) > 1e-3f) {
            m_lastEyeW = eyeW;
            m_lastEyeScale = scale;
            m_baseLidGeom.Reset();
            d2d.GetD2DFactory()->CreatePathGeometry(m_baseLidGeom.GetAddressOf());
            if (m_baseLidGeom) {
                ComPtr<ID2D1GeometrySink> sink;
                m_baseLidGeom->Open(sink.GetAddressOf());
                D2D1_POINT_2F pLeft = D2D1::Point2F(-eyeW * 0.46f, -2.0f * scale);
                D2D1_POINT_2F pRight = D2D1::Point2F(eyeW * 0.46f, -2.0f * scale);
                sink->BeginFigure(pLeft, D2D1_FIGURE_BEGIN_HOLLOW);
                sink->AddBezier(D2D1::BezierSegment(
                    D2D1::Point2F(-eyeW * 0.20f, 8.5f * scale),
                    D2D1::Point2F(eyeW * 0.20f, 8.5f * scale),
                    pRight
                ));
                sink->EndFigure(D2D1_FIGURE_END_OPEN);
                sink->Close();
            }
        }
        if (m_baseLidGeom) {
            D2D1_MATRIX_3X2_F oldMat;
            pRT->GetTransform(&oldMat);
            pRT->SetTransform(D2D1::Matrix3x2F::Translation(eyeX, eyeY) * oldMat);

            // 柔和眼窝微光阴影
            pBrush->SetColor(D2D1::ColorF(0.20f, 0.65f, 0.88f, 0.35f));
            pRT->DrawGeometry(m_baseLidGeom.Get(), pBrush, 4.0f * scale);

            // 主眼睑闭合轮廓线
            pBrush->SetColor(D2D1::ColorF(0.80f, 0.95f, 1.0f, 0.95f));
            pRT->DrawGeometry(m_baseLidGeom.Get(), pBrush, 2.4f * scale);

            pRT->SetTransform(oldMat);

            // 3 根自然微翘睫毛
            pBrush->SetColor(D2D1::ColorF(0.55f, 0.85f, 1.0f, 0.85f));
            pRT->DrawLine(D2D1::Point2F(eyeX - 8.0f * scale, eyeY + 5.5f * scale), D2D1::Point2F(eyeX - 11.5f * scale, eyeY + 10.0f * scale), pBrush, 1.6f * scale);
            pRT->DrawLine(D2D1::Point2F(eyeX, eyeY + 6.2f * scale), D2D1::Point2F(eyeX, eyeY + 11.2f * scale), pBrush, 1.6f * scale);
            pRT->DrawLine(D2D1::Point2F(eyeX + 8.0f * scale, eyeY + 5.5f * scale), D2D1::Point2F(eyeX + 11.5f * scale, eyeY + 10.0f * scale), pBrush, 1.6f * scale);
        }
        return;
    }

    // 巩膜眼眶
    pBrush->SetColor(D2D1::ColorF(0.95f, 0.98f, 1.0f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(eyeX, eyeY), eyeW / 2.0f, eyeH / 2.0f), pBrush);
    pBrush->SetColor(D2D1::ColorF(0.40f, 0.70f, 0.90f));
    pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(eyeX, eyeY), eyeW / 2.0f, eyeH / 2.0f), pBrush, 2.0f * scale);

    // 虹膜与瞳孔（根据注视向量在巩膜眼眶内部平滑转向）
    float pupilScale = (1.0f - closeAmount);
    float irisRadius = 9.0f * scale * pupilScale;
    float px = eyeX + std::clamp(pupilOffsetX, -eyeW * 0.26f, eyeW * 0.26f);
    float py = eyeY + std::clamp(pupilOffsetY, -eyeH * 0.22f, eyeH * 0.22f);

    pBrush->SetColor(D2D1::ColorF(0.18f, 0.45f, 0.75f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), irisRadius, irisRadius), pBrush);

    pBrush->SetColor(D2D1::ColorF(0.08f, 0.12f, 0.18f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), irisRadius * 0.55f, irisRadius * 0.55f), pBrush);

    // 晶状体高光
    pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.9f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px - 2.5f * scale, py - 2.5f * scale), 2.5f * scale, 2.5f * scale), pBrush);
}

void EyeExerciseRenderer::DrawEyeMonitorWindow(ID2D1RenderTarget* pRT, float cx, float cy, float targetX, float targetY, float scale, ID2D1SolidColorBrush* pBrush) {
    float winW = 160.0f * scale;
    float winH = 46.0f * scale;
    D2D1_ROUNDED_RECT winRect = D2D1::RoundedRect(
        D2D1::RectF(cx - winW / 2.0f, cy - winH / 2.0f, cx + winW / 2.0f, cy + winH / 2.0f),
        8.0f * scale, 8.0f * scale
    );

    pBrush->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.45f));
    pRT->FillRoundedRectangle(winRect, pBrush);
    pBrush->SetColor(D2D1::ColorF(0.3f, 0.8f, 0.6f, 0.4f));
    pRT->DrawRoundedRectangle(winRect, pBrush, 1.0f);

    // 计算彗星注视向量 (Gaze Vector)
    float dx = targetX - cx;
    float dy = targetY - cy;
    float len = std::sqrt(dx * dx + dy * dy);
    float gazeX = (len > 0.001f) ? (dx / len * 7.5f * scale) : 0.0f;
    float gazeY = (len > 0.001f) ? (dy / len * 4.5f * scale) : 0.0f;

    // 左眼与右眼微视窗（瞳孔随彗星平滑转向）
    DrawBlinkingEye(pRT, cx - 28.0f * scale, cy, 0.0f, scale * 0.7f, pBrush, gazeX, gazeY);
    DrawBlinkingEye(pRT, cx + 28.0f * scale, cy, 0.0f, scale * 0.7f, pBrush, gazeX, gazeY);
}

void EyeExerciseRenderer::DrawCosmicExpansion(
    ID2D1RenderTarget* pRT,
    ID2D1SolidColorBrush* pBrush,
    float cx,
    float cy,
    float cw,
    float ch,
    float animScale,
    float t
) {
    if (!pRT || !pBrush) return;

    // 1. 中心深空漫射星云核心 (极其柔和低对比度，彻底杜绝刺眼白点，不给睫状肌任何近焦锚点)
    float maxDist = (std::min)(cw, ch) * 0.48f;
    float coreRadius = 45.0f * animScale;
    
    // 漫射星云多层发散光晕
    pBrush->SetColor(D2D1::ColorF(0.04f, 0.35f, 0.50f, 0.22f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), coreRadius * 1.8f, coreRadius * 1.8f), pBrush);
    pBrush->SetColor(D2D1::ColorF(0.08f, 0.55f, 0.65f, 0.35f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), coreRadius, coreRadius), pBrush);
    pBrush->SetColor(D2D1::ColorF(0.20f, 0.85f, 0.75f, 0.25f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), coreRadius * 0.5f, coreRadius * 0.5f), pBrush);

    // 2. 向外无限扩散的景深舒张光环 (Outward Expanding Infinite Waves)
    // 4 级黄金舒展波纹，由中心诞生持续向边界扩散并平滑渐隐至完全透明
    for (int i = 0; i < 4; ++i) {
        float waveProgress = std::fmod(t * 0.6f + i * 0.25f, 1.0f);
        float waveRadius = coreRadius * 0.6f + waveProgress * (maxDist - coreRadius * 0.6f);
        float alpha = (1.0f - waveProgress) * 0.55f;
        float strokeW = (1.5f + (1.0f - waveProgress) * 2.0f) * animScale;

        pBrush->SetColor(D2D1::ColorF(0.20f, 0.88f, 0.68f, alpha));
        pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), waveRadius, waveRadius), pBrush, strokeW);
    }

    // 3. 12 束向外无限延展的柔光深空透视射线 (平滑淡出，非截断齿轮)
    float rotAngle = t * AppConstants::Math::PI * 0.25f;
    for (int i = 0; i < 12; ++i) {
        float angle = rotAngle + i * (AppConstants::Math::PI * 2.0f / 12.0f);
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);
        D2D1_POINT_2F startPt = D2D1::Point2F(cx + cosA * (coreRadius * 0.8f), cy + sinA * (coreRadius * 0.8f));
        D2D1_POINT_2F endPt = D2D1::Point2F(cx + cosA * maxDist, cy + sinA * maxDist);

        pBrush->SetColor(D2D1::ColorF(0.08f, 0.65f, 0.55f, 0.18f));
        pRT->DrawLine(startPt, endPt, pBrush, 1.8f * animScale);
    }

    // 4. 视差深空微星漫射点 (诱导睫状肌舒张放空)
    for (int s = 0; s < 8; ++s) {
        float starAngle = s * 0.785f + t * 0.15f;
        float starDist = coreRadius * 1.2f + s * (maxDist - coreRadius * 1.2f) / 8.0f;
        float sx = cx + std::cos(starAngle) * starDist;
        float sy = cy + std::sin(starAngle) * starDist;
        float starAlpha = 0.25f + 0.35f * static_cast<float>(std::sin(t * 4.0f + s));

        pBrush->SetColor(D2D1::ColorF(0.60f, 1.0f, 0.85f, starAlpha));
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(sx, sy), 2.2f * animScale, 2.2f * animScale), pBrush);
    }
}

void EyeExerciseRenderer::DrawBreathingHalo(
    ID2D1RenderTarget* pRT,
    ID2D1SolidColorBrush* pBrush,
    float cx,
    float cy,
    float animScale,
    float breathExp,
    int breathState,
    [[maybe_unused]] float stateProgress
) {
    if (!pRT || !pBrush) return;

    // 基础半径与呼吸振幅 (有机舒缩)
    float baseRadius = 88.0f * animScale;
    float currentRadius = baseRadius * (0.82f + breathExp * 0.38f);

    // 状态色彩演化：吸气=冰湖天蓝，屏息=静谧琥珀温金，呼气=舒缓翡翠青
    D2D1_COLOR_F glowColor;
    if (breathState == 0) {
        // 吸气：澄澈天蓝 (Inhale: Azure Blue)
        glowColor = D2D1::ColorF(0.20f, 0.70f, 1.0f);
    } else if (breathState == 1) {
        // 屏息：静谧温金 (Hold: Calm Amber Gold)
        glowColor = D2D1::ColorF(1.0f, 0.85f, 0.35f);
    } else {
        // 呼气：放松翡翠绿 (Exhale: Soothing Emerald)
        glowColor = D2D1::ColorF(0.25f, 0.95f, 0.65f);
    }

    // 1. 使用 Direct2D RadialGradientBrush 打造连续指数羽化水母呼吸微光场 (消除硬同心圆阶跃)
    D2D1_GRADIENT_STOP stops[4];
    stops[0].position = 0.0f;
    stops[0].color = D2D1::ColorF(glowColor.r, glowColor.g, glowColor.b, 0.40f + breathExp * 0.20f);
    stops[1].position = 0.42f;
    stops[1].color = D2D1::ColorF(glowColor.r * 0.8f, glowColor.g * 0.8f, glowColor.b * 0.8f, 0.22f + breathExp * 0.15f);
    stops[2].position = 0.76f;
    stops[2].color = D2D1::ColorF(glowColor.r * 0.4f, glowColor.g * 0.4f, glowColor.b * 0.4f, 0.06f + breathExp * 0.06f);
    stops[3].position = 1.0f;
    stops[3].color = D2D1::ColorF(glowColor.r * 0.1f, glowColor.g * 0.1f, glowColor.b * 0.1f, 0.0f);

    ComPtr<ID2D1GradientStopCollection> pStops;
    pRT->CreateGradientStopCollection(stops, 4, pStops.GetAddressOf());
    if (pStops) {
        ComPtr<ID2D1RadialGradientBrush> pRadial;
        float outerRadius = currentRadius * 1.55f;
        pRT->CreateRadialGradientBrush(
            D2D1::RadialGradientBrushProperties(D2D1::Point2F(cx, cy), D2D1::Point2F(0, 0), outerRadius, outerRadius),
            pStops.Get(),
            pRadial.GetAddressOf()
        );
        if (pRadial) {
            pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), outerRadius, outerRadius), pRadial.Get());
        }
    }

    // 2. 水母有机呼吸微光环 (两道高透明度极细微光波纹，随呼吸延展)
    pBrush->SetColor(D2D1::ColorF(glowColor.r, glowColor.g, glowColor.b, 0.18f + breathExp * 0.15f));
    pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), currentRadius * 1.22f, currentRadius * 1.22f), pBrush, 1.2f * animScale);

    pBrush->SetColor(D2D1::ColorF(glowColor.r, glowColor.g, glowColor.b, 0.35f + breathExp * 0.22f));
    pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), currentRadius * 0.95f, currentRadius * 0.95f), pBrush, 1.8f * animScale);

    // 3. 静心内核与安详闭目月牙眼
    pBrush->SetColor(D2D1::ColorF(0.02f, 0.06f, 0.10f, 0.85f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), currentRadius * 0.62f, currentRadius * 0.62f), pBrush);

    // 绘制月牙眼弧与睫毛
    float eyeSpan = 38.0f * animScale;
    DrawBlinkingEye(pRT, cx - eyeSpan, cy, 1.0f, animScale * 1.20f, pBrush);
    DrawBlinkingEye(pRT, cx + eyeSpan, cy, 1.0f, animScale * 1.20f, pBrush);
}

void EyeExerciseRenderer::DrawEyeCircularPacer(
    ID2D1RenderTarget* pRT,
    ID2D1SolidColorBrush* pBrush,
    const D2D1_POINT_2F& center,
    float radius,
    float dpiScale,
    int phase,
    float tNorm,
    int breathState,
    float breathRemain
) {
    if (!pRT || !pBrush || radius < 10.0f) return;

    auto& d2d = D2DContext::Instance();

    // 1. 仪表盘磨砂底盘与外发光环
    pBrush->SetColor(D2D1::ColorF(0.02f, 0.05f, 0.09f, 0.85f));
    pRT->FillEllipse(D2D1::Ellipse(center, radius, radius), pBrush);
    pBrush->SetColor(D2D1::ColorF(0.18f, 0.42f, 0.55f, 0.35f));
    pRT->DrawEllipse(D2D1::Ellipse(center, radius, radius), pBrush, 1.2f * dpiScale);

    // 外圈 12 精密秒数刻度线 (Clock/Meter Tick Marks)
    float tickOuterR = radius * 0.94f;
    for (int k = 0; k < 12; ++k) {
        float tickAngle = k * (AppConstants::Math::PI / 6.0f) - AppConstants::Math::PI * 0.5f;
        float tickLen = (k % 3 == 0) ? (7.0f * dpiScale) : (4.0f * dpiScale);
        float tickInnerR = tickOuterR - tickLen;
        float cosA = std::cos(tickAngle);
        float sinA = std::sin(tickAngle);
        D2D1_POINT_2F p1 = D2D1::Point2F(center.x + cosA * tickInnerR, center.y + sinA * tickInnerR);
        D2D1_POINT_2F p2 = D2D1::Point2F(center.x + cosA * tickOuterR, center.y + sinA * tickOuterR);
        pBrush->SetColor((k % 3 == 0) ? D2D1::ColorF(0.55f, 0.85f, 1.0f, 0.65f) : D2D1::ColorF(0.40f, 0.65f, 0.85f, 0.30f));
        pRT->DrawLine(p1, p2, pBrush, (k % 3 == 0 ? 1.8f : 1.0f) * dpiScale);
    }

    // 2. 底层刻度导轨
    float trackRadius = radius * 0.76f;
    pBrush->SetColor(D2D1::ColorF(0.12f, 0.22f, 0.32f, 0.45f));
    pRT->DrawEllipse(D2D1::Ellipse(center, trackRadius, trackRadius), pBrush, 3.5f * dpiScale);

    // 3. 动态进度扫掠弧线与巡航发光微珠
    float sweepFraction = std::clamp(tNorm, 0.0f, 1.0f);
    if (sweepFraction > 0.005f) {
            const int arcSegments = static_cast<int>(sweepFraction * 72.0f) + 1;
            D2D1_COLOR_F sweepColor;
            if (phase == 0) sweepColor = D2D1::ColorF(0.20f, 0.88f, 0.65f, 0.95f);
            else if (phase == 1) {
                if (breathState == 0) sweepColor = D2D1::ColorF(0.20f, 0.70f, 1.0f, 0.95f);
                else if (breathState == 1) sweepColor = D2D1::ColorF(1.0f, 0.85f, 0.35f, 1.0f);
                else sweepColor = D2D1::ColorF(0.35f, 1.0f, 0.70f, 0.95f);
            } else {
                sweepColor = D2D1::ColorF(0.25f, 0.95f, 0.65f, 0.95f);
            }
            pBrush->SetColor(sweepColor);

            D2D1_POINT_2F prevPt = D2D1::Point2F(
                center.x + trackRadius * std::cos(-AppConstants::Math::PI * 0.5f),
                center.y + trackRadius * std::sin(-AppConstants::Math::PI * 0.5f)
            );
            D2D1_POINT_2F headPt = prevPt;

            for (int i = 1; i <= arcSegments; ++i) {
                float frac = (static_cast<float>(i) / arcSegments) * sweepFraction;
                float angle = -AppConstants::Math::PI * 0.5f + frac * AppConstants::Math::PI * 2.0f;
                D2D1_POINT_2F pt = D2D1::Point2F(
                    center.x + trackRadius * std::cos(angle),
                    center.y + trackRadius * std::sin(angle)
                );
                pRT->DrawLine(prevPt, pt, pBrush, 4.0f * dpiScale);
                prevPt = pt;
                if (i == arcSegments) headPt = pt;
            }

            // 头部发光微珠
            pBrush->SetColor(D2D1::ColorF(sweepColor.r, sweepColor.g, sweepColor.b, 0.35f));
            pRT->FillEllipse(D2D1::Ellipse(headPt, 6.5f * dpiScale, 6.5f * dpiScale), pBrush);
            pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
            pRT->FillEllipse(D2D1::Ellipse(headPt, 3.0f * dpiScale, 3.0f * dpiScale), pBrush);
        }

    // 4. 文字与大字号实时倒计时
    if (phase == 0) {
        float remain = (std::max)(0.1f, (1.0f - tNorm) * m_phaseDuration);
        wchar_t buf[16];
        swprintf_s(buf, L"%.0fs", remain);

        auto fmtNum = d2d.GetCachedTextFormat(L"Segoe UI", 24.0f * dpiScale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtNum) {
            pBrush->SetColor(D2D1::ColorF(0.40f, 1.0f, 0.75f));
            D2D1_RECT_F rNum = D2D1::RectF(center.x - radius, center.y - 18.0f * dpiScale, center.x + radius, center.y + 6.0f * dpiScale);
            pRT->DrawTextW(buf, static_cast<UINT32>(wcslen(buf)), fmtNum.Get(), rNum, pBrush);
        }
        auto fmtLbl = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 11.5f * dpiScale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtLbl) {
            pBrush->SetColor(D2D1::ColorF(0.80f, 0.95f, 0.90f, 0.88f));
            D2D1_RECT_F rLbl = D2D1::RectF(center.x - radius, center.y + 7.0f * dpiScale, center.x + radius, center.y + 24.0f * dpiScale);
            pRT->DrawTextW(L"极目远眺倒计时", 7, fmtLbl.Get(), rLbl, pBrush);
        }
    } else if (phase == 1) {
        wchar_t buf[16];
        swprintf_s(buf, L"%.1fs", breathRemain);

        auto fmtNum = d2d.GetCachedTextFormat(L"Segoe UI", 22.0f * dpiScale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtNum) {
            D2D1_COLOR_F c = (breathState == 0) ? D2D1::ColorF(0.35f, 0.85f, 1.0f) : (breathState == 1 ? D2D1::ColorF(1.0f, 0.88f, 0.40f) : D2D1::ColorF(0.45f, 1.0f, 0.72f));
            pBrush->SetColor(c);
            D2D1_RECT_F rNum = D2D1::RectF(center.x - radius, center.y - 18.0f * dpiScale, center.x + radius, center.y + 6.0f * dpiScale);
            pRT->DrawTextW(buf, static_cast<UINT32>(wcslen(buf)), fmtNum.Get(), rNum, pBrush);
        }
        std::wstring bLbl = (breathState == 0) ? L"深长吸气" : (breathState == 1 ? L"屏息舒压" : L"缓慢呼气");
        auto fmtLbl = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 14.0f * dpiScale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtLbl) {
            pBrush->SetColor(D2D1::ColorF(0.85f, 0.95f, 0.90f, 0.88f));
            D2D1_RECT_F rLbl = D2D1::RectF(center.x - radius, center.y + 6.0f * dpiScale, center.x + radius, center.y + 26.0f * dpiScale);
            pRT->DrawTextW(bLbl.c_str(), static_cast<UINT32>(bLbl.length()), fmtLbl.Get(), rLbl, pBrush);
        }
    } else {
        float remainTrack = (std::max)(0.1f, (1.0f - tNorm) * m_phaseDuration);
        wchar_t buf[16];
        swprintf_s(buf, L"%.0fs", remainTrack);

        auto fmtNum = d2d.GetCachedTextFormat(L"Segoe UI", 22.0f * dpiScale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtNum) {
            pBrush->SetColor(D2D1::ColorF(0.35f, 1.0f, 0.75f));
            D2D1_RECT_F rNum = D2D1::RectF(center.x - radius, center.y - 18.0f * dpiScale, center.x + radius, center.y + 6.0f * dpiScale);
            pRT->DrawTextW(buf, static_cast<UINT32>(wcslen(buf)), fmtNum.Get(), rNum, pBrush);
        }
        auto fmtLbl = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 11.5f * dpiScale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtLbl) {
            pBrush->SetColor(D2D1::ColorF(0.70f, 0.90f, 0.85f, 0.85f));
            D2D1_RECT_F rLbl = D2D1::RectF(center.x - radius, center.y + 7.0f * dpiScale, center.x + radius, center.y + 24.0f * dpiScale);
            pRT->DrawTextW(L"全景八字追踪", 6, fmtLbl.Get(), rLbl, pBrush);
        }
    }
}

void EyeExerciseRenderer::Render(ID2D1RenderTarget* pRT, const D2D1_RECT_F& bounds, float dpiScale) {
    RenderStatic(pRT, bounds, dpiScale);
    RenderDynamic(pRT, bounds, dpiScale);
}

void EyeExerciseRenderer::RenderStatic(ID2D1RenderTarget* pRT, const D2D1_RECT_F& bounds, float dpiScale) {
    if (!pRT) return;

    float w = bounds.right - bounds.left;
    float h = bounds.bottom - bounds.top;
    if (w <= 20.0f || h <= 20.0f) return;

    // 1. 基于全屏响应式安全视口计算几何分区与动画缩放
    auto layout = ExerciseLayout::Calculate(bounds, dpiScale, 520.0f, 280.0f);
    float uiScale = layout.dpiScale;

    // 2. 全屏柔和微光半透明卡片背景
    D2D1_ROUNDED_RECT cardRect = D2D1::RoundedRect(
        D2D1::RectF(bounds.left + 4.0f, bounds.top + 4.0f, bounds.right - 4.0f, bounds.bottom - 4.0f),
        16.0f * uiScale, 16.0f * uiScale
    );

    ComPtr<ID2D1SolidColorBrush> pBrush;
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.04f, 0.06f, 0.09f, 0.65f), pBrush.GetAddressOf());
    if (pBrush) {
        pRT->FillRoundedRectangle(cardRect, pBrush.Get());
        pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.15f));
        pRT->DrawRoundedRectangle(cardRect, pBrush.Get(), 1.0f * uiScale);
    }

    auto& d2d = D2DContext::Instance();

    // 3. Zone A: 顶部栏 Header (大字号 Badge 与步骤概览)
    float badgeW = 205.0f * uiScale;
    float badgeH = 34.0f * uiScale;
    D2D1_ROUNDED_RECT badgeRect = D2D1::RoundedRect(
        D2D1::RectF(layout.headerRect.left, layout.headerRect.top, layout.headerRect.left + badgeW, layout.headerRect.top + badgeH),
        8.0f * uiScale, 8.0f * uiScale
    );
    if (pBrush) {
        pBrush->SetColor(D2D1::ColorF(0.0f, 0.62f, 0.86f, 0.35f));
        pRT->FillRoundedRectangle(badgeRect, pBrush.Get());
        pBrush->SetColor(D2D1::ColorF(0.31f, 0.78f, 1.0f, 0.55f));
        pRT->DrawRoundedRectangle(badgeRect, pBrush.Get(), 1.2f * uiScale);

        float dotSize = 9.0f * uiScale;
        float dotX = badgeRect.rect.left + 12.0f * uiScale;
        float dotY = badgeRect.rect.top + (badgeH - dotSize) / 2.0f;

        pBrush->SetColor(D2D1::ColorF(0.31f, 0.78f, 1.0f, 0.35f));
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX + dotSize / 2.0f, dotY + dotSize / 2.0f), dotSize, dotSize), pBrush.Get());

        pBrush->SetColor(D2D1::ColorF(0.47f, 0.88f, 1.0f, 1.0f));
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX + dotSize / 2.0f, dotY + dotSize / 2.0f), dotSize / 2.0f, dotSize / 2.0f), pBrush.Get());

        pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f));
        auto fmtBadge = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 13.0f * uiScale, DWRITE_FONT_WEIGHT_BOLD);
        if (fmtBadge) {
            D2D1_RECT_F textRect = D2D1::RectF(dotX + dotSize + 8.0f * uiScale, badgeRect.rect.top + 6.0f * uiScale, badgeRect.rect.right, badgeRect.rect.bottom);
            pRT->DrawTextW(L"视网膜与眼外肌舒缓", 9, fmtBadge.Get(), textRect, pBrush.Get());
        }
    }

    // 顶部右侧法则步骤标签
    float pacerW = 160.0f * uiScale;
    float pacerH = 34.0f * uiScale;
    D2D1_ROUNDED_RECT pacerRect = D2D1::RoundedRect(
        D2D1::RectF(layout.headerRect.right - pacerW, layout.headerRect.top, layout.headerRect.right, layout.headerRect.top + pacerH),
        8.0f * uiScale, 8.0f * uiScale
    );
    if (pBrush) {
        pBrush->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.45f));
        pRT->FillRoundedRectangle(pacerRect, pBrush.Get());
        pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.25f));
        pRT->DrawRoundedRectangle(pacerRect, pBrush.Get(), 1.0f * uiScale);

        wchar_t phaseBuf[32];
        swprintf_s(phaseBuf, L"★ 3阶段调理 · 法则 %d/3", m_currentPhase + 1);

        pBrush->SetColor(D2D1::ColorF(0.51f, 0.94f, 1.0f));
        auto fmtPacer = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 12.0f * uiScale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtPacer) {
            pRT->DrawTextW(phaseBuf, static_cast<UINT32>(wcslen(phaseBuf)), fmtPacer.Get(), pacerRect.rect, pBrush.Get());
        }
    }

    // 4. Zone B: 底部信息卡片 Guidance Dock (黄金分割双栏排版：左侧要领 + 右侧仪表盘)
    std::wstring actionTitle, actionTip, subTip;
    switch (m_currentPhase) {
        case 0:
            actionTitle = L"法则一 · 20-20-20 极目远眺";
            actionTip = L"将视线完全移开屏幕，极目凝视窗外 6 米以外的无限远景物至少 20 秒";
            subTip = L"近距离聚焦使睫状肌持续痉挛收缩，远眺 20 秒以上令睫状肌彻底恢复自然松弛";
            break;
        case 1:
            actionTitle = L"法则二 · 深度闭目与 4-4-4 呼吸引导";
            actionTip = L"完全闭合双眼，跟随呼吸环节奏：深吸气 4 秒 ➔ 屏息 2 秒 ➔ 慢呼气 4 秒 深度舒压";
            subTip = L"主动完全眨眼与闭目可重新均匀涂布角膜脂质泪膜，深呼吸降低交感神经张力与眼压";
            break;
        case 2:
            actionTitle = L"法则三 · 全屏大视野视线八字追踪";
            actionTip = L"保持头部端正不动，双眼视线大范围平滑跟随全屏翡翠彗星沿 ∞ 轨道移动";
            subTip = L"大范围平缓活动 6 条眼外肌群并促进眼眶血液微循环，切忌剧烈用力甩动眼球";
            break;
    }

    if (pBrush) {
        D2D1_ROUNDED_RECT dockPlate = D2D1::RoundedRect(layout.dockRect, 14.0f * uiScale, 14.0f * uiScale);
        pBrush->SetColor(D2D1::ColorF(0.04f, 0.08f, 0.12f, 0.85f));
        pRT->FillRoundedRectangle(dockPlate, pBrush.Get());
        pBrush->SetColor(D2D1::ColorF(0.18f, 0.60f, 0.85f, 0.45f));
        pRT->DrawRoundedRectangle(dockPlate, pBrush.Get(), 1.2f * uiScale);

        // 左栏：要领文案
        float dockInnerLeft = layout.dockLeftRect.left + 24.0f * uiScale;
        float dockInnerRight = layout.dockLeftRect.right - 12.0f * uiScale;
        float currentY = layout.dockLeftRect.top + 18.0f * uiScale;

        // 行 1：动作标题 (大字号 25.0f * uiScale，纯白加粗)
        auto fmtTitle = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 25.0f * uiScale, DWRITE_FONT_WEIGHT_BOLD);
        if (fmtTitle) {
            pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f));
            D2D1_RECT_F titleRect = D2D1::RectF(dockInnerLeft, currentY, dockInnerRight, currentY + 34.0f * uiScale);
            pRT->DrawTextW(actionTitle.c_str(), static_cast<UINT32>(actionTitle.length()), fmtTitle.Get(), titleRect, pBrush.Get());
        }

        currentY += 38.0f * uiScale;

        // 行 2：动作要领 (大字号 18.5f * uiScale，翡翠青绿加粗)
        auto fmtTip = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 18.5f * uiScale, DWRITE_FONT_WEIGHT_BOLD);
        if (fmtTip) {
            pBrush->SetColor(D2D1::ColorF(0.55f, 1.0f, 0.72f));
            std::wstring fullTip = L"● 动作要领：" + actionTip;
            D2D1_RECT_F tipRect = D2D1::RectF(dockInnerLeft, currentY, dockInnerRight, currentY + 28.0f * uiScale);
            pRT->DrawTextW(fullTip.c_str(), static_cast<UINT32>(fullTip.length()), fmtTip.Get(), tipRect, pBrush.Get());
        }

        currentY += 32.0f * uiScale;

        // 行 3：医学依据 (大字号 15.5f * uiScale，淡青灰呼吸色)
        auto fmtSubTip = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 15.5f * uiScale, DWRITE_FONT_WEIGHT_REGULAR);
        if (fmtSubTip) {
            pBrush->SetColor(D2D1::ColorF(0.80f, 0.90f, 0.86f, 0.95f));
            std::wstring fullSubTip = L"● 医学依据：" + subTip;
            D2D1_RECT_F subTipRect = D2D1::RectF(dockInnerLeft, currentY, dockInnerRight, layout.dockLeftRect.bottom - 12.0f * uiScale);
            pRT->DrawTextW(fullSubTip.c_str(), static_cast<UINT32>(fullSubTip.length()), fmtSubTip.Get(), subTipRect, pBrush.Get());
        }

        // 黄金分割分界线 (垂直微光细线)
        float divX = (layout.dockLeftRect.right + layout.dockRightRect.left) * 0.5f;
        pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f));
        pRT->DrawLine(
            D2D1::Point2F(divX, layout.dockRect.top + 16.0f * uiScale),
            D2D1::Point2F(divX, layout.dockRect.bottom - 16.0f * uiScale),
            pBrush.Get(),
            1.0f * uiScale
        );

        // 节拍器底座外发光环与刻度导轨
        pBrush->SetColor(D2D1::ColorF(0.02f, 0.05f, 0.09f, 0.85f));
        pRT->FillEllipse(D2D1::Ellipse(layout.dockMeterCenter, layout.dockMeterRadius, layout.dockMeterRadius), pBrush.Get());
        pBrush->SetColor(D2D1::ColorF(0.18f, 0.42f, 0.55f, 0.35f));
        pRT->DrawEllipse(D2D1::Ellipse(layout.dockMeterCenter, layout.dockMeterRadius, layout.dockMeterRadius), pBrush.Get(), 1.2f * uiScale);

        float trackRadius = layout.dockMeterRadius * 0.76f;
        pBrush->SetColor(D2D1::ColorF(0.12f, 0.22f, 0.32f, 0.45f));
        pRT->DrawEllipse(D2D1::Ellipse(layout.dockMeterCenter, trackRadius, trackRadius), pBrush.Get(), 3.5f * uiScale);
    }

    // Zone C: 法则一的静态指引文字 (大屏升级至 17.5px 粗体翡翠呼吸光)
    if (m_currentPhase == 0 && pBrush) {
        auto fmtMetric = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 17.5f * uiScale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        if (fmtMetric) {
            pBrush->SetColor(D2D1::ColorF(0.65f, 1.0f, 0.85f));
            std::wstring metricText = L"● 视线完全移开屏幕 · 极目凝视窗外无限远景物 · 彻底放空睫状肌";
            D2D1_RECT_F mRect = D2D1::RectF(layout.canvasRect.left, layout.canvasRect.bottom - 34.0f * uiScale, layout.canvasRect.right, layout.canvasRect.bottom);
            pRT->DrawTextW(metricText.c_str(), static_cast<UINT32>(metricText.length()), fmtMetric.Get(), mRect, pBrush.Get());
        }
    }
}

void EyeExerciseRenderer::RenderDynamic(ID2D1RenderTarget* pRT, const D2D1_RECT_F& bounds, float dpiScale) {
    if (!pRT) return;

    float w = bounds.right - bounds.left;
    float h = bounds.bottom - bounds.top;
    if (w <= 20.0f || h <= 20.0f) return;

    auto layout = ExerciseLayout::Calculate(bounds, dpiScale, 520.0f, 280.0f);
    float animScale = layout.animScale;
    float uiScale = layout.dpiScale;
    float t = GetPhaseProgress();

    auto& d2d = D2DContext::Instance();
    ComPtr<ID2D1SolidColorBrush> pBrush;
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), pBrush.GetAddressOf());

    // 计算呼吸状态参数 (供呼吸环与节拍仪表盘同频调用)
    float breathProgress = std::fmod(m_animTime, 10.0f) / 10.0f;
    float breathExp = 0.0f;
    int breathState = 0; // 0: 吸气, 1: 屏息, 2: 呼气
    float breathRemain = 0.0f;

    if (breathProgress < 0.40f) {
        breathState = 0;
        float sub = breathProgress / 0.40f;
        breathExp = 0.5f + 0.5f * static_cast<float>(std::sin(sub * AppConstants::Math::PI / 2.0f));
        breathRemain = (0.40f - breathProgress) * 10.0f;
    } else if (breathProgress < 0.60f) {
        breathState = 1;
        breathExp = 1.0f;
        breathRemain = (0.60f - breathProgress) * 10.0f;
    } else {
        breathState = 2;
        float sub = (breathProgress - 0.60f) / 0.40f;
        breathExp = 1.0f - 0.5f * static_cast<float>(std::sin(sub * AppConstants::Math::PI / 2.0f));
        breathRemain = (1.0f - breathProgress) * 10.0f;
    }

    // 右栏：动态环形节拍仪表盘
    if (pBrush) {
        DrawEyeCircularPacer(pRT, pBrush.Get(), layout.dockMeterCenter, layout.dockMeterRadius, uiScale, m_currentPhase, t, breathState, breathRemain);
    }

    // 5. Zone C: 中央动画独立视口 Central Canvas
    float cx = layout.canvasCenterX;
    float cy = layout.canvasCenterY;
    float cw = layout.canvasRect.right - layout.canvasRect.left;
    float ch = layout.canvasRect.bottom - layout.canvasRect.top;

    if (m_currentPhase == 0) {
        // 法则一：向外无限扩散的深空景深漫射场
        DrawCosmicExpansion(pRT, pBrush.Get(), cx, cy, cw, ch, animScale, t);
    } else if (m_currentPhase == 1) {
        // 法则二：仿生闭目眼睑与多层水母呼吸光晕
        DrawBreathingHalo(pRT, pBrush.Get(), cx, cy - 8.0f * animScale, animScale, breathExp, breathState, breathProgress);
    } else {
        // 法则三：全屏大视野 ∞ 轨道视线追踪
        float orbitCenterY = cy + 10.0f * animScale;
        float trackWidth = (std::min)(cw * 0.44f, 480.0f * animScale);
        float trackHeight = (std::min)(ch * 0.40f, 260.0f * animScale);

        if (!m_baseInfinityGeom || std::abs(m_lastInfinityWidth - trackWidth) > 1e-3f || std::abs(m_lastInfinityHeight - trackHeight) > 1e-3f) {
            m_lastInfinityWidth = trackWidth;
            m_lastInfinityHeight = trackHeight;
            m_baseInfinityGeom.Reset();
            d2d.GetD2DFactory()->CreatePathGeometry(m_baseInfinityGeom.GetAddressOf());
            if (m_baseInfinityGeom) {
                ComPtr<ID2D1GeometrySink> sink;
                m_baseInfinityGeom->Open(sink.GetAddressOf());

                const int numPoints = 160;
                for (int i = 0; i < numPoints; ++i) {
                    float angle = (static_cast<float>(i) / numPoints) * AppConstants::Math::PI * 2.0f;
                    float denom = 1.0f + static_cast<float>(std::sin(angle) * std::sin(angle));
                    float px = (static_cast<float>(std::cos(angle)) / denom) * trackWidth;
                    float py = (static_cast<float>(std::sin(angle) * static_cast<float>(std::cos(angle))) / denom) * trackHeight;
                    if (i == 0) sink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
                    else sink->AddLine(D2D1::Point2F(px, py));
                }
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();
            }
        }

        if (m_baseInfinityGeom && pBrush) {
            D2D1_MATRIX_3X2_F oldMat;
            pRT->GetTransform(&oldMat);
            D2D1_MATRIX_3X2_F infMat = D2D1::Matrix3x2F::Translation(cx, orbitCenterY);
            pRT->SetTransform(infMat * oldMat);

            // 外层漫射柔光轨 (消除非等比拉伸线宽失真，等比保真 6.0f * animScale)
            pBrush->SetColor(D2D1::ColorF(0.18f, 0.85f, 0.60f, 0.12f));
            pRT->DrawGeometry(m_baseInfinityGeom.Get(), pBrush.Get(), 6.0f * animScale);

            // 内层清晰导引轨 (等比保真 2.2f * animScale)
            pBrush->SetColor(D2D1::ColorF(0.24f, 0.90f, 0.65f, 0.42f));
            pRT->DrawGeometry(m_baseInfinityGeom.Get(), pBrush.Get(), 2.2f * animScale);

            pRT->SetTransform(oldMat);
        }

        // 翡翠彗星头部与 18 阶粒子柔光拖尾
        float moveAngle = t * AppConstants::Math::PI * 2.0f;
        float denomHead = 1.0f + static_cast<float>(std::sin(moveAngle) * std::sin(moveAngle));
        float targetX = cx + (trackWidth * static_cast<float>(std::cos(moveAngle))) / denomHead;
        float targetY = orbitCenterY + (trackHeight * static_cast<float>(std::sin(moveAngle) * std::cos(moveAngle))) / denomHead;

        if (pBrush) {
            for (int trail = 18; trail >= 1; --trail) {
                float trailAngle = moveAngle - trail * 0.034f;
                float denomTrail = 1.0f + static_cast<float>(std::sin(trailAngle) * std::sin(trailAngle));
                float tx = cx + (trackWidth * static_cast<float>(std::cos(trailAngle))) / denomTrail;
                float ty = orbitCenterY + (trackHeight * static_cast<float>(std::sin(trailAngle) * std::cos(trailAngle))) / denomTrail;
                float tRadius = (24.0f - trail * 1.05f) * animScale;
                float alpha = (1.0f - trail / 19.0f) * 0.52f;

                pBrush->SetColor(D2D1::ColorF(0.16f, 0.95f, 0.60f, alpha));
                pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(tx, ty), tRadius / 2.0f, tRadius / 2.0f), pBrush.Get());
            }

            pBrush->SetColor(D2D1::ColorF(0.18f, 1.0f, 0.60f, 0.25f));
            pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(targetX, targetY), 28.0f * animScale, 28.0f * animScale), pBrush.Get());

            pBrush->SetColor(D2D1::ColorF(0.35f, 1.0f, 0.70f, 0.85f));
            pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(targetX, targetY), 16.0f * animScale, 16.0f * animScale), pBrush.Get());

            pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 0.88f, 0.95f));
            pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(targetX, targetY), 12.0f * animScale, 12.0f * animScale), pBrush.Get(), 2.0f * animScale);

            pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
            pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(targetX, targetY), 7.5f * animScale, 7.5f * animScale), pBrush.Get());
        }
    }
}
