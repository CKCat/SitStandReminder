#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "NeckExerciseRenderer.hpp"
#include "D2DContext.hpp"
#include "../core/AppConstants.hpp"
#include <cmath>
#include <algorithm>

void NeckExerciseRenderer::SetTotalDuration(float totalSeconds) {
    m_phaseDuration = (std::max)(1.5f, totalSeconds / 4.0f);
    m_animTime = 0.0f;
    m_currentPhase = 0;
}

void NeckExerciseRenderer::Update(float dt) {
    m_animTime += dt;
    m_currentPhase = static_cast<int>(m_animTime / m_phaseDuration) % 4;
}

void NeckExerciseRenderer::Reset() {
    m_animTime = 0.0f;
    m_currentPhase = 0;
}

float NeckExerciseRenderer::GetPhaseProgress() const {
    float phaseTime = std::fmod(m_animTime, m_phaseDuration);
    return phaseTime / m_phaseDuration;
}

float NeckExerciseRenderer::EaseInOutCubic(float x) {
    return x < 0.5f ? 4.0f * x * x * x : 1.0f - std::pow(-2.0f * x + 2.0f, 3.0f) / 2.0f;
}

void NeckExerciseRenderer::CalculateStretchPacing(float tNorm, float& ease, int& segment, float& segmentProgress) {
    const float inEnd = 0.30f;
    const float holdEnd = 0.72f;

    if (tNorm < inEnd) {
        segment = 0; // Extending
        float subT = tNorm / inEnd;
        ease = EaseInOutCubic(subT);
        segmentProgress = subT;
    } else if (tNorm < holdEnd) {
        segment = 1; // Holding
        float breathPulse = static_cast<float>(std::sin(tNorm * AppConstants::Math::PI * 6.0f)) * 0.015f;
        ease = std::clamp(1.0f + breathPulse, 0.98f, 1.02f);
        segmentProgress = (tNorm - inEnd) / (holdEnd - inEnd);
    } else {
        segment = 2; // Returning
        float subT = (tNorm - holdEnd) / (1.0f - holdEnd);
        ease = 1.0f - EaseInOutCubic(subT);
        segmentProgress = subT;
    }
}

void NeckExerciseRenderer::DrawProfileHead(
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
) {
    if (!pRT || !pBrush || !pFactory) return;

    // 1. 绘制医学解剖侧面头部轮廓 (含额头、前凸鼻尖、唇峰、下巴与下颌角)
    // 采用复用基准几何路径 + 矩阵旋转缩放，消除 60FPS 渲染热路径中的每秒 60 次 COM 堆分配
    if (!m_baseHeadGeom) {
        pFactory->CreatePathGeometry(m_baseHeadGeom.GetAddressOf());
        if (m_baseHeadGeom) {
            ComPtr<ID2D1GeometrySink> sink;
            m_baseHeadGeom->Open(sink.GetAddressOf());

            // 颅骨后脑顶点 (以 (0,0) 为基准原点，radius=1.0)
            sink->BeginFigure(D2D1::Point2F(-0.45f, -0.95f), D2D1_FIGURE_BEGIN_FILLED);
            // 颅顶弧线
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(0.0f, -1.02f),
                D2D1::Point2F(0.45f, -0.95f),
                D2D1::Point2F(0.82f, -0.55f)
            ));
            // 额头与眉骨
            sink->AddLine(D2D1::Point2F(0.95f, -0.22f));
            // 鼻根凹陷
            sink->AddLine(D2D1::Point2F(0.90f, -0.10f));
            // 鼻尖 (显著向前凸起，清晰指示朝向)
            sink->AddLine(D2D1::Point2F(1.34f, 0.08f));
            // 鼻底小柱
            sink->AddLine(D2D1::Point2F(0.92f, 0.20f));
            // 上唇峰
            sink->AddLine(D2D1::Point2F(1.00f, 0.32f));
            // 口裂
            sink->AddLine(D2D1::Point2F(0.88f, 0.38f));
            // 下唇
            sink->AddLine(D2D1::Point2F(0.98f, 0.46f));
            // 颏唇沟
            sink->AddLine(D2D1::Point2F(0.88f, 0.54f));
            // 颏部 (下巴)
            sink->AddLine(D2D1::Point2F(1.05f, 0.68f));
            // 下颌底线
            sink->AddLine(D2D1::Point2F(0.55f, 0.84f));
            // 下颌角
            sink->AddLine(D2D1::Point2F(0.12f, 0.78f));
            // 枕下颈后线
            sink->AddLine(D2D1::Point2F(-0.65f, 0.42f));
            // 枕骨后脑大圆弧
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(-1.05f, 0.15f),
                D2D1::Point2F(-1.00f, -0.55f),
                D2D1::Point2F(-0.45f, -0.95f)
            ));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
        }
    }

    if (m_baseHeadGeom) {
        D2D1_MATRIX_3X2_F oldMat;
        pRT->GetTransform(&oldMat);
        D2D1_MATRIX_3X2_F headMat = D2D1::Matrix3x2F::Scale(headRadius, headRadius) * D2D1::Matrix3x2F::Translation(hx, hy);
        pRT->SetTransform(headMat * oldMat);

        // 填充头部
        pBrush->SetColor(D2D1::ColorF(0.92f, 0.96f, 1.0f, 0.95f));
        pRT->FillGeometry(m_baseHeadGeom.Get(), pBrush);
        // 勾勒高精轮廓线
        pBrush->SetColor(D2D1::ColorF(0.62f, 0.82f, 0.98f, 1.0f));
        pRT->DrawGeometry(m_baseHeadGeom.Get(), pBrush, (2.6f * scale) / headRadius);

        pRT->SetTransform(oldMat);
    }

    // 2. 绘制耳廓
    float earX = hx - headRadius * 0.06f;
    float earY = hy + headRadius * 0.16f;
    pBrush->SetColor(D2D1::ColorF(0.85f, 0.92f, 0.98f, 1.0f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(earX, earY), 6.5f * scale, 11.5f * scale), pBrush);
    pBrush->SetColor(D2D1::ColorF(0.55f, 0.76f, 0.94f, 1.0f));
    pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(earX, earY), 6.5f * scale, 11.5f * scale), pBrush, 2.0f * scale);
    // 耳甲腔微结构
    pRT->DrawLine(D2D1::Point2F(earX, earY - 5.0f * scale), D2D1::Point2F(earX + 2.0f * scale, earY + 3.0f * scale), pBrush, 1.8f * scale);

    // 3. 绘制眼睛与睫毛
    float eyeX = hx + headRadius * 0.52f;
    float eyeY = hy - headRadius * 0.12f;
    // 眉毛
    pBrush->SetColor(D2D1::ColorF(0.40f, 0.65f, 0.88f, 1.0f));
    pRT->DrawLine(D2D1::Point2F(eyeX - 8.0f * scale, eyeY - 11.0f * scale), D2D1::Point2F(eyeX + 16.0f * scale, eyeY - 7.0f * scale), pBrush, 2.5f * scale);
    // 眼裂轮廓 (侧面杏仁眼)
    pBrush->SetColor(D2D1::ColorF(0.20f, 0.45f, 0.70f, 1.0f));
    pRT->DrawLine(D2D1::Point2F(eyeX - 4.0f * scale, eyeY - 5.0f * scale), D2D1::Point2F(eyeX + 14.0f * scale, eyeY), pBrush, 2.2f * scale);
    pRT->DrawLine(D2D1::Point2F(eyeX - 2.0f * scale, eyeY + 4.0f * scale), D2D1::Point2F(eyeX + 14.0f * scale, eyeY), pBrush, 2.0f * scale);
    // 晶状体瞳孔 (发光翡翠青)
    pBrush->SetColor(D2D1::ColorF(0.0f, 0.85f, 0.65f, 1.0f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(eyeX + 7.0f * scale, eyeY), 3.8f * scale, 3.8f * scale), pBrush);
    pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(eyeX + 8.5f * scale, eyeY - 1.2f * scale), 1.4f * scale, 1.4f * scale), pBrush);

    // 4. 视线朝向指引光束 (Gaze Vector Ray)
    float rayStartX = eyeX + 16.0f * scale;
    float rayStartY = eyeY;
    float rayLength = 85.0f * scale;
    
    // 视线微光粒子与渐变射束
    pBrush->SetColor(D2D1::ColorF(0.0f, 0.95f, 0.75f, 0.45f));
    pRT->DrawLine(D2D1::Point2F(rayStartX, rayStartY), D2D1::Point2F(rayStartX + rayLength, rayStartY), pBrush, 2.2f * scale);
    
    // 视线焦点指示圆环
    pBrush->SetColor(D2D1::ColorF(0.35f, 1.0f, 0.85f, 0.85f));
    pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(rayStartX + rayLength, rayStartY), 5.5f * scale, 5.5f * scale), pBrush, 1.8f * scale);
    pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(rayStartX + rayLength, rayStartY), 2.5f * scale, 2.5f * scale), pBrush);

    // 视线说明文字
    auto fmtGaze = D2DContext::Instance().GetCachedTextFormat(L"Microsoft YaHei UI", 9.5f * scale, DWRITE_FONT_WEIGHT_BOLD);
    if (fmtGaze) {
        pBrush->SetColor(D2D1::ColorF(0.65f, 1.0f, 0.85f, 0.9f));
        std::wstring gazeLabel = isExtending ? L"仰视 25° 向上舒展" : L"视线平视正前方";
        D2D1_RECT_F gazeRect = D2D1::RectF(rayStartX + 8.0f * scale, rayStartY - 18.0f * scale, rayStartX + rayLength + 120.0f * scale, rayStartY);
        pRT->DrawTextW(gazeLabel.c_str(), static_cast<UINT32>(gazeLabel.length()), fmtGaze.Get(), gazeRect, pBrush);
    }

    // 5. 动作一专用：后缩挤下巴时的下颌肌群激活发光提示
    if (isTucking && tuckEase > 0.15f) {
        pBrush->SetColor(D2D1::ColorF(0.20f, 0.85f, 1.0f, tuckEase * 0.65f));
        pRT->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(hx + headRadius * 0.40f, hy + headRadius * 0.82f), 14.0f * scale * tuckEase, 8.0f * scale * tuckEase),
            pBrush
        );
    }
}

static void DrawFrontHead(
    ID2D1RenderTarget* pRT,
    ID2D1SolidColorBrush* pBrush,
    float hx,
    float hy,
    float headRadius,
    float scale
) {
    if (!pRT || !pBrush) return;

    // 1. 正面脸部轮廓
    pBrush->SetColor(D2D1::ColorF(0.92f, 0.96f, 1.0f, 0.95f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx, hy), headRadius * 0.92f, headRadius * 1.10f), pBrush);
    pBrush->SetColor(D2D1::ColorF(0.62f, 0.82f, 0.98f, 1.0f));
    pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(hx, hy), headRadius * 0.92f, headRadius * 1.10f), pBrush, 2.6f * scale);

    // 2. 双耳
    float earDist = headRadius * 0.92f;
    pBrush->SetColor(D2D1::ColorF(0.85f, 0.92f, 0.98f, 1.0f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx - earDist, hy + 2.0f * scale), 6.5f * scale, 12.0f * scale), pBrush);
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx + earDist, hy + 2.0f * scale), 6.5f * scale, 12.0f * scale), pBrush);
    pBrush->SetColor(D2D1::ColorF(0.55f, 0.76f, 0.94f, 1.0f));
    pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(hx - earDist, hy + 2.0f * scale), 6.5f * scale, 12.0f * scale), pBrush, 2.0f * scale);
    pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(hx + earDist, hy + 2.0f * scale), 6.5f * scale, 12.0f * scale), pBrush, 2.0f * scale);

    // 3. 正面双眼与眉毛
    float eyeSpan = 16.0f * scale;
    float eyeY = hy - 6.0f * scale;
    // 眉毛
    pBrush->SetColor(D2D1::ColorF(0.40f, 0.65f, 0.88f, 1.0f));
    pRT->DrawLine(D2D1::Point2F(hx - eyeSpan - 9.0f * scale, eyeY - 10.0f * scale), D2D1::Point2F(hx - eyeSpan + 9.0f * scale, eyeY - 8.0f * scale), pBrush, 2.2f * scale);
    pRT->DrawLine(D2D1::Point2F(hx + eyeSpan - 9.0f * scale, eyeY - 8.0f * scale), D2D1::Point2F(hx + eyeSpan + 9.0f * scale, eyeY - 10.0f * scale), pBrush, 2.2f * scale);
    // 眼球
    pBrush->SetColor(D2D1::ColorF(0.0f, 0.85f, 0.65f, 1.0f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx - eyeSpan, eyeY), 4.5f * scale, 4.5f * scale), pBrush);
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx + eyeSpan, eyeY), 4.5f * scale, 4.5f * scale), pBrush);
    pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx - eyeSpan + 1.0f * scale, eyeY - 1.0f * scale), 1.6f * scale, 1.6f * scale), pBrush);
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx + eyeSpan + 1.0f * scale, eyeY - 1.0f * scale), 1.6f * scale, 1.6f * scale), pBrush);

    // 4. 正面鼻子 (极简立体鼻梁与鼻翼)
    pBrush->SetColor(D2D1::ColorF(0.55f, 0.75f, 0.92f, 1.0f));
    pRT->DrawLine(D2D1::Point2F(hx, eyeY + 2.0f * scale), D2D1::Point2F(hx, hy + 12.0f * scale), pBrush, 2.0f * scale);
    pRT->DrawLine(D2D1::Point2F(hx - 5.0f * scale, hy + 12.0f * scale), D2D1::Point2F(hx + 5.0f * scale, hy + 12.0f * scale), pBrush, 2.0f * scale);

    // 5. 嘴唇
    pBrush->SetColor(D2D1::ColorF(0.60f, 0.78f, 0.94f, 1.0f));
    pRT->DrawLine(D2D1::Point2F(hx - 8.0f * scale, hy + 24.0f * scale), D2D1::Point2F(hx + 8.0f * scale, hy + 24.0f * scale), pBrush, 2.2f * scale);
}

void NeckExerciseRenderer::Render(ID2D1RenderTarget* pRT, const D2D1_RECT_F& bounds, float /*dpiScale*/) {
    if (!pRT) return;

    float w = bounds.right - bounds.left;
    float h = bounds.bottom - bounds.top;
    if (w <= 20.0f || h <= 20.0f) return;

    // 大画幅高占比自适应缩放：让骨骼与五官清晰可见
    float scale = std::clamp((std::min)(w / 640.0f, h / 420.0f), 1.0f, 1.65f);

    // 1. 半透明卡片背景
    D2D1_ROUNDED_RECT cardRect = D2D1::RoundedRect(
        D2D1::RectF(bounds.left + 4.0f, bounds.top + 4.0f, bounds.right - 4.0f, bounds.bottom - 4.0f),
        18.0f * scale, 18.0f * scale
    );

    ComPtr<ID2D1SolidColorBrush> pBrush;
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.04f, 0.06f, 0.09f, 0.55f), pBrush.GetAddressOf());
    if (pBrush) {
        pRT->FillRoundedRectangle(cardRect, pBrush.Get());
        pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.25f));
        pRT->DrawRoundedRectangle(cardRect, pBrush.Get(), 1.5f * scale);
    }

    // 2. 计算节律进度
    float phaseTime = std::fmod(m_animTime, m_phaseDuration);
    float tNorm = phaseTime / m_phaseDuration;
    float ease = 0.0f;
    int segment = 0;
    float segProgress = 0.0f;
    CalculateStretchPacing(tNorm, ease, segment, segProgress);

    auto& d2d = D2DContext::Instance();
    auto fmtBadge = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 11.0f * scale, DWRITE_FONT_WEIGHT_BOLD);
    auto fmtTitle = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 15.0f * scale, DWRITE_FONT_WEIGHT_BOLD);
    auto fmtTip = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 11.5f * scale, DWRITE_FONT_WEIGHT_BOLD);
    auto fmtSubTip = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 9.5f * scale, DWRITE_FONT_WEIGHT_REGULAR);
    auto fmtPacer = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 9.0f * scale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // 顶部标签 Badge
    float badgeW = 165.0f * scale;
    float badgeH = 32.0f * scale;
    D2D1_ROUNDED_RECT badgeRect = D2D1::RoundedRect(
        D2D1::RectF(bounds.left + 24.0f * scale, bounds.top + 18.0f * scale, bounds.left + 24.0f * scale + badgeW, bounds.top + 18.0f * scale + badgeH),
        8.0f * scale, 8.0f * scale
    );
    if (pBrush) {
        pBrush->SetColor(D2D1::ColorF(0.0f, 0.75f, 0.43f, 0.35f));
        pRT->FillRoundedRectangle(badgeRect, pBrush.Get());

        // 发光微标
        float dotSize = 8.0f * scale;
        float dotX = badgeRect.rect.left + 12.0f * scale;
        float dotY = badgeRect.rect.top + (badgeH - dotSize) / 2.0f;

        pBrush->SetColor(D2D1::ColorF(0.47f, 1.0f, 0.70f, 0.35f));
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX + dotSize / 2.0f, dotY + dotSize / 2.0f), dotSize, dotSize), pBrush.Get());

        pBrush->SetColor(D2D1::ColorF(0.55f, 1.0f, 0.78f, 1.0f));
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX + dotSize / 2.0f, dotY + dotSize / 2.0f), dotSize / 2.0f, dotSize / 2.0f), pBrush.Get());

        pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
        if (fmtBadge) {
            D2D1_RECT_F textRect = D2D1::RectF(dotX + dotSize + 8.0f * scale, badgeRect.rect.top + 6.0f * scale, badgeRect.rect.right, badgeRect.rect.bottom);
            pRT->DrawTextW(L"颈椎科学保养操", 7, fmtBadge.Get(), textRect, pBrush.Get());
        }
    }

    // 右上角节拍器 Pacer
    float pacerW = 140.0f * scale;
    float pacerH = 30.0f * scale;
    D2D1_ROUNDED_RECT pacerRect = D2D1::RoundedRect(
        D2D1::RectF(bounds.right - pacerW - 24.0f * scale, bounds.top + 18.0f * scale, bounds.right - 24.0f * scale, bounds.top + 18.0f * scale + pacerH),
        6.0f * scale, 6.0f * scale
    );
    if (pBrush) {
        pBrush->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.3f));
        pRT->FillRoundedRectangle(pacerRect, pBrush.Get());
        pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.3f));
        pRT->DrawRoundedRectangle(pacerRect, pBrush.Get(), 1.0f);

        std::wstring pacerText;
        D2D1_COLOR_F pacerColor;
        if (segment == 0) {
            pacerText = L"● 柔和延展中";
            pacerColor = D2D1::ColorF(0.51f, 0.86f, 1.0f);
        } else if (segment == 1) {
            float remainHold = (std::max)(0.1f, (0.72f - tNorm) * m_phaseDuration);
            wchar_t buf[32];
            swprintf_s(buf, L"✨ 保持中 %.1fs", remainHold);
            pacerText = buf;
            pacerColor = D2D1::ColorF(1.0f, 0.88f, 0.35f);
        } else {
            pacerText = L"○ 缓慢回正";
            pacerColor = D2D1::ColorF(0.63f, 1.0f, 0.75f);
        }

        pBrush->SetColor(pacerColor);
        if (fmtPacer) {
            pRT->DrawTextW(pacerText.c_str(), static_cast<UINT32>(pacerText.length()), fmtPacer.Get(), pacerRect.rect, pBrush.Get());
        }
    }

    // 动作标题与说明 (纯净中文专业文案)
    std::wstring actionTitle, actionTip, subTip;
    switch (m_currentPhase) {
        case 0:
            actionTitle = L"动作 1/4 · 水平后缩下巴";
            actionTip = L"平视前方，头顶百会穴向上拔高延展，下巴水平向后平移收紧，轻微挤出双下巴";
            subTip = L"强效激活深层颈长肌与头长肌，恢复自然生理曲度，根治头前倾体态";
            break;
        case 1:
            actionTitle = L"动作 2/4 · 受控仰角复位";
            actionTip = L"在后缩基础上由头顶引领向上后仰 25°，目光仰视斜上方，维持舒展保持 3 秒";
            subTip = L"安全恢复低头变直的颈椎生理前凸，温和拉伸颈前肌群，消除僵硬紧绷";
            break;
        case 2:
            actionTitle = L"动作 3/4 · 缓慢向左侧拉伸";
            actionTip = L"左耳向左肩靠近，右肩主动下沉固定，感受右侧斜方肌与颈侧深度延展";
            subTip = L"深度消除单侧斜方肌与肩胛提肌持续痉挛，促进椎动脉供血，保持无痛舒展";
            break;
        case 3:
            actionTitle = L"动作 4/4 · 缓慢向右侧拉伸";
            actionTip = L"右耳向右肩靠近，左肩主动下沉固定，感受左侧斜方肌与颈侧深度延展";
            subTip = L"对称平衡双侧颈肩肌群张力，松解肌筋膜粘连，消除长期伏案酸痛";
            break;
    }

    if (pBrush && fmtTitle) {
        pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f));
        D2D1_RECT_F titleRect = D2D1::RectF(bounds.left + 24.0f * scale, bounds.top + 56.0f * scale, bounds.right - 24.0f * scale, bounds.top + 90.0f * scale);
        pRT->DrawTextW(actionTitle.c_str(), static_cast<UINT32>(actionTitle.length()), fmtTitle.Get(), titleRect, pBrush.Get());
    }

    // 3. 绘制人体解剖骨骼与动画 (大幅面高清视觉展示)
    float cx = bounds.left + w / 2.0f;
    float cy = bounds.top + h * 0.42f;
    float headRadius = 34.0f * scale; // 大幅放大的头部比例

    if (m_currentPhase == 0) {
        // 动作一：水平后缩下巴 (平视前方，下巴水平向后平移)
        float shiftX = -ease * (34.0f * scale);
        float liftY = -ease * (6.0f * scale);

        // 躯干与胸椎轮廓 (更宽阔立体的肩胸比例)
        if (pBrush) {
            pBrush->SetColor(D2D1::ColorF(0.75f, 0.86f, 0.94f));
            pRT->DrawLine(D2D1::Point2F(cx - 36.0f * scale, cy + 44.0f * scale), D2D1::Point2F(cx - 44.0f * scale, cy + 96.0f * scale), pBrush.Get(), 5.0f * scale);
            pRT->DrawLine(D2D1::Point2F(cx + 34.0f * scale, cy + 52.0f * scale), D2D1::Point2F(cx + 28.0f * scale, cy + 96.0f * scale), pBrush.Get(), 5.0f * scale);
            pBrush->SetColor(D2D1::ColorF(0.85f, 0.85f, 0.85f));
            pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 1.0f * scale, cy + 48.0f * scale), 10.0f * scale, 10.0f * scale), pBrush.Get());
        }

        // 颈椎 C1-C7 节段 (加粗高亮发光，清晰可见 C 弯曲度)
        D2D1_POINT_2F neckBase = D2D1::Point2F(cx - 10.0f * scale, cy + 42.0f * scale);
        D2D1_POINT_2F headJoint = D2D1::Point2F(cx - 8.0f * scale + shiftX, cy - 6.0f * scale + liftY);

        if (pBrush) {
            pBrush->SetColor(D2D1::ColorF(0.30f, 0.80f, 1.0f, 0.9f));
            pRT->DrawLine(neckBase, headJoint, pBrush.Get(), 6.5f * scale);

            pBrush->SetColor(D2D1::ColorF(0.70f, 0.98f, 1.0f));
            for (int i = 0; i < 7; ++i) {
                float f = i / 6.0f;
                float px = neckBase.x + (headJoint.x - neckBase.x) * f - static_cast<float>(std::sin(f * AppConstants::Math::PI)) * (9.0f + ease * 5.0f) * scale;
                float py = neckBase.y + (headJoint.y - neckBase.y) * f;
                pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), 4.5f * scale, 4.5f * scale), pBrush.Get());
            }
        }

        // 绘制带五官轮廓与视线光束的侧面头部
        float hx = cx + shiftX;
        float hy = cy - 42.0f * scale + liftY;
        DrawProfileHead(pRT, pBrush.Get(), d2d.GetD2DFactory(), hx, hy, headRadius, scale, true, ease, false);

    } else if (m_currentPhase == 1) {
        // 动作二：受控仰角复位 25° (五官清晰向上抬起 25°，视线射向斜上方)
        float angle = -ease * 25.0f;

        // 躯干轮廓
        if (pBrush) {
            pBrush->SetColor(D2D1::ColorF(0.75f, 0.86f, 0.94f));
            pRT->DrawLine(D2D1::Point2F(cx - 36.0f * scale, cy + 44.0f * scale), D2D1::Point2F(cx - 44.0f * scale, cy + 96.0f * scale), pBrush.Get(), 5.0f * scale);
            pRT->DrawLine(D2D1::Point2F(cx + 34.0f * scale, cy + 52.0f * scale), D2D1::Point2F(cx + 28.0f * scale, cy + 96.0f * scale), pBrush.Get(), 5.0f * scale);
        }

        // 旋转坐标系：绕胸锁关节枢纽点仰角旋转
        D2D1_MATRIX_3X2_F oldTransform;
        pRT->GetTransform(&oldTransform);
        D2D1_POINT_2F pivot = D2D1::Point2F(cx, cy + 38.0f * scale);
        pRT->SetTransform(D2D1::Matrix3x2F::Rotation(angle, pivot) * oldTransform);

        if (pBrush) {
            // 颈椎 C1-C7
            pBrush->SetColor(D2D1::ColorF(0.30f, 0.80f, 1.0f, 0.9f));
            pRT->DrawLine(pivot, D2D1::Point2F(cx, cy - 10.0f * scale), pBrush.Get(), 6.5f * scale);

            pBrush->SetColor(D2D1::ColorF(0.70f, 0.98f, 1.0f));
            for (int i = 0; i < 6; ++i) {
                float f = i / 5.0f;
                float py = pivot.y - 48.0f * scale * f;
                pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, py), 4.5f * scale, 4.5f * scale), pBrush.Get());
            }

            // 绘制向上仰起 25° 的五官侧面头部 (鼻尖、眼睛、下巴同步仰起)
            float hx = cx;
            float hy = cy - 10.0f * scale - headRadius * 0.90f;
            DrawProfileHead(pRT, pBrush.Get(), d2d.GetD2DFactory(), hx, hy, headRadius, scale, false, 0.0f, true);
        }

        pRT->SetTransform(oldTransform);

    } else {
        // 动作三/四：左右侧拉伸 (正面视角：五官轴线向肩部舒展倾斜)
        float sideAngle = (m_currentPhase == 2 ? -1.0f : 1.0f) * ease * 25.0f;
        int stretchSide = (m_currentPhase == 2) ? 1 : -1;

        // 双肩与斜方肌基座 (宽阔舒展)
        if (pBrush) {
            pBrush->SetColor(D2D1::ColorF(0.75f, 0.86f, 0.94f));
            pRT->DrawLine(D2D1::Point2F(cx - 95.0f * scale, cy + 42.0f * scale), D2D1::Point2F(cx + 95.0f * scale, cy + 42.0f * scale), pBrush.Get(), 5.5f * scale);
            pRT->DrawLine(D2D1::Point2F(cx - 78.0f * scale, cy + 42.0f * scale), D2D1::Point2F(cx - 78.0f * scale, cy + 96.0f * scale), pBrush.Get(), 5.5f * scale);
            pRT->DrawLine(D2D1::Point2F(cx + 78.0f * scale, cy + 42.0f * scale), D2D1::Point2F(cx + 78.0f * scale, cy + 96.0f * scale), pBrush.Get(), 5.5f * scale);

            // 斜方肌热力发光带
            if (ease > 0.15f) {
                pBrush->SetColor(D2D1::ColorF(1.0f, 0.27f, 0.27f, ease * 0.75f));
                pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + stretchSide * 50.0f * scale, cy + 30.0f * scale), 24.0f * scale * ease, 14.0f * scale * ease), pBrush.Get());
            }
        }

        // 旋转正面头部
        D2D1_MATRIX_3X2_F oldTransform;
        pRT->GetTransform(&oldTransform);
        D2D1_POINT_2F pivot = D2D1::Point2F(cx, cy + 40.0f * scale);
        pRT->SetTransform(D2D1::Matrix3x2F::Rotation(sideAngle, pivot) * oldTransform);

        if (pBrush) {
            // 正面颈椎
            pBrush->SetColor(D2D1::ColorF(0.30f, 0.80f, 1.0f, 0.9f));
            pRT->DrawLine(pivot, D2D1::Point2F(cx, cy - 6.0f * scale), pBrush.Get(), 6.5f * scale);

            pBrush->SetColor(D2D1::ColorF(0.70f, 0.98f, 1.0f));
            for (int i = 0; i < 6; ++i) {
                float f = i / 5.0f;
                float py = pivot.y - 46.0f * scale * f;
                pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, py), 4.5f * scale, 4.5f * scale), pBrush.Get());
            }

            // 绘制倾斜的正面五官 (双眼、鼻子、嘴巴、双耳)
            float hx = cx;
            float hy = cy - 6.0f * scale - headRadius * 0.90f;
            DrawFrontHead(pRT, pBrush.Get(), hx, hy, headRadius, scale);
        }

        pRT->SetTransform(oldTransform);
    }

    // 4. 底部说明文案 (充足排版高度，严格杜绝重叠与截断)
    if (pBrush) {
        float bottomY = bounds.bottom - (74.0f * scale);
        pBrush->SetColor(D2D1::ColorF(0.55f, 1.0f, 0.70f));
        std::wstring fullTip = L"● 动作要领：" + actionTip;
        D2D1_RECT_F tipRect = D2D1::RectF(bounds.left + 24.0f * scale, bottomY, bounds.right - 24.0f * scale, bottomY + 32.0f * scale);
        if (fmtTip) pRT->DrawTextW(fullTip.c_str(), static_cast<UINT32>(fullTip.length()), fmtTip.Get(), tipRect, pBrush.Get());

        pBrush->SetColor(D2D1::ColorF(0.84f, 0.94f, 0.88f));
        std::wstring fullSubTip = L"● 医学依据：" + subTip;
        D2D1_RECT_F subTipRect = D2D1::RectF(bounds.left + 24.0f * scale, bottomY + 34.0f * scale, bounds.right - 24.0f * scale, bounds.bottom - 10.0f * scale);
        if (fmtSubTip) pRT->DrawTextW(fullSubTip.c_str(), static_cast<UINT32>(fullSubTip.length()), fmtSubTip.Get(), subTipRect, pBrush.Get());
    }
}

