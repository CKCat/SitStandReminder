#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "EyeExerciseRenderer.hpp"
#include "D2DContext.hpp"
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

    // 巩膜眼眶
    pBrush->SetColor(D2D1::ColorF(0.95f, 0.98f, 1.0f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(eyeX, eyeY), eyeW / 2.0f, eyeH / 2.0f), pBrush);
    pBrush->SetColor(D2D1::ColorF(0.40f, 0.70f, 0.90f));
    pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(eyeX, eyeY), eyeW / 2.0f, eyeH / 2.0f), pBrush, 2.0f * scale);

    // 虹膜与瞳孔（根据彗星注视向量在巩膜眼眶内部平滑转向）
    if (closeAmount < 0.85f) {
        float pupilScale = (1.0f - closeAmount);
        float irisRadius = 9.0f * scale * pupilScale;
        float px = eyeX + std::clamp(pupilOffsetX, -eyeW * 0.26f, eyeW * 0.26f);
        float py = eyeY + std::clamp(pupilOffsetY, -eyeH * 0.22f, eyeH * 0.22f);

        pBrush->SetColor(D2D1::ColorF(0.18f, 0.45f, 0.75f));
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), irisRadius, irisRadius), pBrush);

        pBrush->SetColor(D2D1::ColorF(0.08f, 0.12f, 0.18f));
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), irisRadius * 0.55f, irisRadius * 0.55f), pBrush);

        // 高光
        pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.9f));
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px - 2.5f * scale, py - 2.5f * scale), 2.5f * scale, 2.5f * scale), pBrush);
    }
}

void EyeExerciseRenderer::DrawEyeMonitorWindow(ID2D1RenderTarget* pRT, float cx, float cy, float targetX, float targetY, float scale, ID2D1SolidColorBrush* pBrush) {
    float winW = 150.0f * scale;
    float winH = 42.0f * scale;
    D2D1_ROUNDED_RECT winRect = D2D1::RoundedRect(
        D2D1::RectF(cx - winW / 2.0f, cy - winH / 2.0f, cx + winW / 2.0f, cy + winH / 2.0f),
        8.0f * scale, 8.0f * scale
    );

    pBrush->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.45f));
    pRT->FillRoundedRectangle(winRect, pBrush);
    pBrush->SetColor(D2D1::ColorF(0.3f, 0.8f, 0.6f, 0.4f));
    pRT->DrawRoundedRectangle(winRect, pBrush, 1.0f);

    // 计算真实的彗星注视向量 (Gaze Vector)
    float dx = targetX - cx;
    float dy = targetY - cy;
    float len = std::sqrt(dx * dx + dy * dy);
    float gazeX = (len > 0.001f) ? (dx / len * 7.5f * scale) : 0.0f;
    float gazeY = (len > 0.001f) ? (dy / len * 4.5f * scale) : 0.0f;

    // 左眼与右眼微视窗（眼眶固定在视窗内，瞳孔随彗星平滑转向）
    DrawBlinkingEye(pRT, cx - 28.0f * scale, cy, 0.0f, scale * 0.7f, pBrush, gazeX, gazeY);
    DrawBlinkingEye(pRT, cx + 28.0f * scale, cy, 0.0f, scale * 0.7f, pBrush, gazeX, gazeY);
}

void EyeExerciseRenderer::Render(ID2D1RenderTarget* pRT, const D2D1_RECT_F& bounds, float /*dpiScale*/) {
    if (!pRT) return;

    float w = bounds.right - bounds.left;
    float h = bounds.bottom - bounds.top;
    if (w <= 20.0f || h <= 20.0f) return;

    float scale = std::clamp((std::min)(w / 800.0f, h / 540.0f), 0.85f, 1.35f);

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

    float t = GetPhaseProgress();

    auto& d2d = D2DContext::Instance();
    auto fmtBadge = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 11.0f * scale, DWRITE_FONT_WEIGHT_BOLD);
    auto fmtTitle = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 15.0f * scale, DWRITE_FONT_WEIGHT_BOLD);
    auto fmtTip = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 11.5f * scale, DWRITE_FONT_WEIGHT_BOLD);
    auto fmtSubTip = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 9.5f * scale, DWRITE_FONT_WEIGHT_REGULAR);
    auto fmtMetric = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 11.0f * scale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
    auto fmtBreath = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 13.0f * scale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
    auto fmtPacer = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 9.0f * scale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // 顶部标签
    float badgeW = 180.0f * scale;
    float badgeH = 32.0f * scale;
    D2D1_ROUNDED_RECT badgeRect = D2D1::RoundedRect(
        D2D1::RectF(bounds.left + 24.0f * scale, bounds.top + 18.0f * scale, bounds.left + 24.0f * scale + badgeW, bounds.top + 18.0f * scale + badgeH),
        8.0f * scale, 8.0f * scale
    );
    if (pBrush) {
        pBrush->SetColor(D2D1::ColorF(0.0f, 0.62f, 0.86f, 0.35f));
        pRT->FillRoundedRectangle(badgeRect, pBrush.Get());

        float dotSize = 8.0f * scale;
        float dotX = badgeRect.rect.left + 12.0f * scale;
        float dotY = badgeRect.rect.top + (badgeH - dotSize) / 2.0f;

        pBrush->SetColor(D2D1::ColorF(0.31f, 0.78f, 1.0f, 0.35f));
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX + dotSize / 2.0f, dotY + dotSize / 2.0f), dotSize, dotSize), pBrush.Get());

        pBrush->SetColor(D2D1::ColorF(0.47f, 0.88f, 1.0f, 1.0f));
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX + dotSize / 2.0f, dotY + dotSize / 2.0f), dotSize / 2.0f, dotSize / 2.0f), pBrush.Get());

        pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f));
        if (fmtBadge) {
            D2D1_RECT_F textRect = D2D1::RectF(dotX + dotSize + 8.0f * scale, badgeRect.rect.top + 6.0f * scale, badgeRect.rect.right, badgeRect.rect.bottom);
            pRT->DrawTextW(L"20-20-20 科学护眼操", 13, fmtBadge.Get(), textRect, pBrush.Get());
        }
    }

    // 右上角最佳推荐时长
    float pacerW = 170.0f * scale;
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

        pBrush->SetColor(D2D1::ColorF(0.51f, 0.94f, 1.0f));
        std::wstring optText = L"★ 3阶段完整护眼 · 60秒";
        if (fmtPacer) {
            pRT->DrawTextW(optText.c_str(), static_cast<UINT32>(optText.length()), fmtPacer.Get(), pacerRect.rect, pBrush.Get());
        }
    }

    // 动作标题与说明 (纯净中文专业文案)
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

    if (pBrush && fmtTitle) {
        pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f));
        D2D1_RECT_F titleRect = D2D1::RectF(bounds.left + 24.0f * scale, bounds.top + 56.0f * scale, bounds.right - 24.0f * scale, bounds.top + 90.0f * scale);
        pRT->DrawTextW(actionTitle.c_str(), static_cast<UINT32>(actionTitle.length()), fmtTitle.Get(), titleRect, pBrush.Get());
    }

    // 绘制核心图形动画 (限制在独立中部视窗，绝对不与底部文字重叠)
    float cx = bounds.left + w / 2.0f;
    float cy = bounds.top + h * 0.44f;

    if (m_currentPhase == 0) {
        // 法则一：深空透视光轴隧道 + 舒张波纹 + 景深光斑
        float rotAngle = t * AppConstants::Math::PI * 0.4f;

        // 12 束深空光锥 (使用复用 Geometry + 矩阵旋转，彻底消除每帧 12 次 COM 实例化开销)
        if (pBrush) {
            pBrush->SetColor(D2D1::ColorF(0.0f, 0.94f, 0.63f, 0.08f));
            float rayLen = (std::max)(w, h) * 0.70f;

            if (!m_baseRayGeom) {
                d2d.GetD2DFactory()->CreatePathGeometry(m_baseRayGeom.GetAddressOf());
                if (m_baseRayGeom) {
                    ComPtr<ID2D1GeometrySink> sink;
                    m_baseRayGeom->Open(sink.GetAddressOf());
                    sink->BeginFigure(D2D1::Point2F(0.0f, 0.0f), D2D1_FIGURE_BEGIN_FILLED);
                    sink->AddLine(D2D1::Point2F(std::cos(-0.08f) * 1000.0f, std::sin(-0.08f) * 1000.0f));
                    sink->AddLine(D2D1::Point2F(std::cos(0.08f) * 1000.0f, std::sin(0.08f) * 1000.0f));
                    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    sink->Close();
                }
            }

            if (m_baseRayGeom) {
                float scaleFactor = rayLen / 1000.0f;
                D2D1_MATRIX_3X2_F oldTransform;
                pRT->GetTransform(&oldTransform);

                for (int i = 0; i < 12; ++i) {
                    float a = rotAngle + i * (AppConstants::Math::PI * 2.0f / 12.0f);
                    float aDeg = a * 180.0f / AppConstants::Math::PI;

                    D2D1_MATRIX_3X2_F mat = D2D1::Matrix3x2F::Scale(scaleFactor, scaleFactor)
                        * D2D1::Matrix3x2F::Rotation(aDeg)
                        * D2D1::Matrix3x2F::Translation(cx, cy - 8.0f * scale);

                    pRT->SetTransform(mat * oldTransform);
                    pRT->FillGeometry(m_baseRayGeom.Get(), pBrush.Get());
                }
                pRT->SetTransform(oldTransform);
            }

            // 4 级舒张黄金波纹
            for (int i = 0; i < 4; ++i) {
                float ringProgress = std::fmod(t + i * 0.25f, 1.0f);
                float ringRadius = (15.0f + ringProgress * 135.0f) * scale;
                float alpha = (1.0f - ringProgress) * 0.8f;
                pBrush->SetColor(D2D1::ColorF(0.31f, 1.0f, 0.67f, alpha));
                pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy - 8.0f * scale), ringRadius, ringRadius), pBrush.Get(), (2.0f + (1.0f - ringProgress) * 2.0f) * scale);
            }

            // 景深中心聚焦光核
            pBrush->SetColor(D2D1::ColorF(0.96f, 1.0f, 0.98f, 1.0f));
            pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy - 8.0f * scale), 8.0f * scale, 8.0f * scale), pBrush.Get());
        }

        // 提示文字
        if (pBrush && fmtMetric) {
            pBrush->SetColor(D2D1::ColorF(0.70f, 1.0f, 0.82f));
            std::wstring metricText = L"● 视线穿透屏幕 · 凝视 6 米外窗外远景 · 保持 20 秒以上";
            D2D1_RECT_F mRect = D2D1::RectF(bounds.left, cy + 62.0f * scale, bounds.right, cy + 90.0f * scale);
            pRT->DrawTextW(metricText.c_str(), static_cast<UINT32>(metricText.length()), fmtMetric.Get(), mRect, pBrush.Get());
        }
    } else if (m_currentPhase == 1) {
        // 法则二：深度闭目与 4-4-4 呼吸环
        float closeAmount = (t < 0.20f) ? (t / 0.20f) : (t < 0.80f ? 1.0f : (1.0f - (t - 0.80f) / 0.20f));
        float breathProgress = std::fmod(m_animTime, 10.0f) / 10.0f;
        float breathExp = 0.0f;
        std::wstring breathText;
        D2D1_COLOR_F breathColor;

        if (breathProgress < 0.40f) {
            float sub = breathProgress / 0.40f;
            breathExp = 0.5f + 0.5f * static_cast<float>(std::sin(sub * AppConstants::Math::PI / 2.0f));
            breathText = L"● 缓缓深深吸气 · 吸气 4 秒 · 充盈氧气";
            breathColor = D2D1::ColorF(0.51f, 0.86f, 1.0f);
        } else if (breathProgress < 0.60f) {
            breathExp = 1.0f;
            breathText = L"✨ 屏息静气 · 保持 2 秒 · 眼肌深度放松";
            breathColor = D2D1::ColorF(1.0f, 0.90f, 0.43f);
        } else {
            float sub = (breathProgress - 0.60f) / 0.40f;
            breathExp = 1.0f - 0.5f * static_cast<float>(std::sin(sub * AppConstants::Math::PI / 2.0f));
            breathText = L"○ 慢慢缓缓呼出 · 呼气 4 秒 · 释放眼压";
            breathColor = D2D1::ColorF(0.63f, 1.0f, 0.78f);
        }

        // 呼吸光环
        if (pBrush) {
            float breathRadius = (68.0f + breathExp * 28.0f) * scale;
            pBrush->SetColor(D2D1::ColorF(0.10f, 0.47f, 0.78f, (0.24f + breathExp * 0.25f)));
            pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy - 8.0f * scale), breathRadius, breathRadius), pBrush.Get());
            pBrush->SetColor(D2D1::ColorF(0.35f, 0.78f, 1.0f, 0.6f));
            pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy - 8.0f * scale), breathRadius, breathRadius), pBrush.Get(), 2.0f * scale);

            // 闭眼动画
            DrawBlinkingEye(pRT, cx - 72.0f * scale, cy - 8.0f * scale, closeAmount, scale, pBrush.Get());
            DrawBlinkingEye(pRT, cx + 72.0f * scale, cy - 8.0f * scale, closeAmount, scale, pBrush.Get());
        }

        // 呼吸提示文字
        if (pBrush && fmtBreath) {
            pBrush->SetColor(breathColor);
            D2D1_RECT_F bRect = D2D1::RectF(bounds.left, cy + 56.0f * scale, bounds.right, cy + 86.0f * scale);
            pRT->DrawTextW(breathText.c_str(), static_cast<UINT32>(breathText.length()), fmtBreath.Get(), bRect, pBrush.Get());
        }
    } else {
        // 法则三：全屏大视野 ∞ 轨道视线追踪
        float trackWidth = (std::min)(w * 0.32f, 240.0f * scale);
        float trackHeight = (std::min)(h * 0.16f, 85.0f * scale);

        // 绘制发光 ∞ 轨道
        if (pBrush) {
            pBrush->SetColor(D2D1::ColorF(0.24f, 0.90f, 0.63f, 0.35f));
            ComPtr<ID2D1PathGeometry> pathGeom;
            d2d.GetD2DFactory()->CreatePathGeometry(pathGeom.GetAddressOf());
            if (pathGeom) {
                ComPtr<ID2D1GeometrySink> sink;
                pathGeom->Open(sink.GetAddressOf());

                const int numPoints = 120;
                for (int i = 0; i < numPoints; ++i) {
                    float angle = (static_cast<float>(i) / numPoints) * AppConstants::Math::PI * 2.0f;
                    float denom = 1.0f + static_cast<float>(std::sin(angle) * std::sin(angle));
                    float px = cx + (trackWidth * static_cast<float>(std::cos(angle))) / denom;
                    float py = cy + (trackHeight * static_cast<float>(std::sin(angle) * std::cos(angle))) / denom;
                    if (i == 0) sink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
                    else sink->AddLine(D2D1::Point2F(px, py));
                }
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();
                pRT->DrawGeometry(pathGeom.Get(), pBrush.Get(), 2.5f * scale);
            }
        }

        // 彗星头部与 12 阶拖尾
        float moveAngle = t * AppConstants::Math::PI * 2.0f;
        float denomHead = 1.0f + static_cast<float>(std::sin(moveAngle) * std::sin(moveAngle));
        float targetX = cx + (trackWidth * static_cast<float>(std::cos(moveAngle))) / denomHead;
        float targetY = cy + (trackHeight * static_cast<float>(std::sin(moveAngle) * std::cos(moveAngle))) / denomHead;

        if (pBrush) {
            // 12 阶粒子拖尾
            for (int trail = 12; trail >= 1; --trail) {
                float trailAngle = moveAngle - trail * 0.045f;
                float denomTrail = 1.0f + static_cast<float>(std::sin(trailAngle) * std::sin(trailAngle));
                float tx = cx + (trackWidth * static_cast<float>(std::cos(trailAngle))) / denomTrail;
                float ty = cy + (trackHeight * static_cast<float>(std::sin(trailAngle) * std::cos(trailAngle))) / denomTrail;
                float tRadius = (20.0f - trail * 1.3f) * scale;
                float alpha = (1.0f - trail / 13.0f) * 0.55f;

                pBrush->SetColor(D2D1::ColorF(0.16f, 0.94f, 0.55f, alpha));
                pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(tx, ty), tRadius / 2.0f, tRadius / 2.0f), pBrush.Get());
            }

            // 彗星头部与瞄准环
            pBrush->SetColor(D2D1::ColorF(0.18f, 1.0f, 0.55f, 0.7f));
            pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(targetX, targetY), 20.0f * scale, 20.0f * scale), pBrush.Get());

            pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 0.78f, 0.85f));
            pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(targetX, targetY), 13.0f * scale, 13.0f * scale), pBrush.Get(), 2.0f * scale);

            pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 0.86f));
            pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(targetX, targetY), 8.0f * scale, 8.0f * scale), pBrush.Get());

            // 顶部 3D 拟真眼球视窗
            DrawEyeMonitorWindow(pRT, cx, bounds.top + 76.0f * scale, targetX, targetY, scale, pBrush.Get());
        }
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

