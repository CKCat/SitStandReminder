#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "NeckExerciseRenderer.hpp"
#include "D2DContext.hpp"
#include "ExerciseLayout.hpp"
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
            // 颅顶优美大弧线
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(0.0f, -1.02f),
                D2D1::Point2F(0.45f, -0.95f),
                D2D1::Point2F(0.82f, -0.55f)
            ));
            // 额头与眉骨 (平滑贝塞尔过渡)
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(0.92f, -0.42f),
                D2D1::Point2F(0.96f, -0.25f),
                D2D1::Point2F(0.92f, -0.10f)
            ));
            // 鼻根与立体鼻梁鼻尖 (光滑流线，指示清晰朝向)
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(0.95f, -0.02f),
                D2D1::Point2F(1.30f, 0.05f),
                D2D1::Point2F(1.28f, 0.12f)
            ));
            // 鼻小柱与人中唇峰
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(1.08f, 0.18f),
                D2D1::Point2F(0.98f, 0.24f),
                D2D1::Point2F(1.04f, 0.32f)
            ));
            // 唇裂与下唇微弧
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(0.92f, 0.36f),
                D2D1::Point2F(0.96f, 0.42f),
                D2D1::Point2F(1.02f, 0.48f)
            ));
            // 颏唇沟与微翘下巴
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(0.92f, 0.54f),
                D2D1::Point2F(0.98f, 0.62f),
                D2D1::Point2F(1.05f, 0.68f)
            ));
            // 优美下颌角曲线
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(0.85f, 0.80f),
                D2D1::Point2F(0.48f, 0.82f),
                D2D1::Point2F(0.12f, 0.78f)
            ));
            // 下颌角过渡至枕下线
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(-0.15f, 0.70f),
                D2D1::Point2F(-0.45f, 0.55f),
                D2D1::Point2F(-0.65f, 0.42f)
            ));
            // 枕骨后脑饱满大圆弧
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(-1.08f, 0.15f),
                D2D1::Point2F(-1.02f, -0.55f),
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

    // 视线说明文字 (在世界坐标系下绝对水平排版，彻底消除汉字倾斜带来的识别障碍)
    auto fmtGaze = D2DContext::Instance().GetCachedTextFormat(L"Microsoft YaHei UI", (std::max)(12.0f * scale, 13.5f), DWRITE_FONT_WEIGHT_BOLD);
    if (fmtGaze) {
        D2D1_MATRIX_3X2_F currentMat;
        pRT->GetTransform(&currentMat);

        // 将靶环坐标变换为世界无旋转坐标
        D2D1_POINT_2F localTargetPt = D2D1::Point2F(rayStartX + rayLength, rayStartY);
        D2D1_POINT_2F worldTargetPt = D2D1::Point2F(
            localTargetPt.x * currentMat._11 + localTargetPt.y * currentMat._21 + currentMat._31,
            localTargetPt.x * currentMat._12 + localTargetPt.y * currentMat._22 + currentMat._32
        );

        // 临时切换至无倾斜的正交坐标系绘制文字与连接微标
        pRT->SetTransform(D2D1::Matrix3x2F::Identity());

        pBrush->SetColor(D2D1::ColorF(0.70f, 1.0f, 0.88f, 0.95f));
        std::wstring gazeLabel = isExtending ? L"仰视 25° 向上舒展" : L"视线平视正前方";

        float textLeft = worldTargetPt.x + 10.0f * scale;
        float textTop = worldTargetPt.y - 12.0f * scale;
        D2D1_RECT_F gazeRect = D2D1::RectF(textLeft, textTop, textLeft + 220.0f * scale, textTop + 24.0f * scale);
        pRT->DrawTextW(gazeLabel.c_str(), static_cast<UINT32>(gazeLabel.length()), fmtGaze.Get(), gazeRect, pBrush);

        pRT->SetTransform(currentMat);
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

    // 1. 正面脸部轮廓 (饱满额头与微收敛下颌)
    pBrush->SetColor(D2D1::ColorF(0.92f, 0.96f, 1.0f, 0.95f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx, hy), headRadius * 0.92f, headRadius * 1.08f), pBrush);
    pBrush->SetColor(D2D1::ColorF(0.62f, 0.82f, 0.98f, 1.0f));
    pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(hx, hy), headRadius * 0.92f, headRadius * 1.08f), pBrush, 2.6f * scale);

    // 2. 双耳 (耳廓解剖微弧)
    float earDist = headRadius * 0.92f;
    pBrush->SetColor(D2D1::ColorF(0.85f, 0.92f, 0.98f, 1.0f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx - earDist, hy + 2.0f * scale), 6.5f * scale, 12.0f * scale), pBrush);
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx + earDist, hy + 2.0f * scale), 6.5f * scale, 12.0f * scale), pBrush);
    pBrush->SetColor(D2D1::ColorF(0.55f, 0.76f, 0.94f, 1.0f));
    pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(hx - earDist, hy + 2.0f * scale), 6.5f * scale, 12.0f * scale), pBrush, 2.0f * scale);
    pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(hx + earDist, hy + 2.0f * scale), 6.5f * scale, 12.0f * scale), pBrush, 2.0f * scale);

    // 3. 正面双眼与眉毛 (杏仁眼眶与清透瞳孔)
    float eyeSpan = 17.0f * scale;
    float eyeY = hy - 6.0f * scale;
    // 柔和微弧眉毛
    pBrush->SetColor(D2D1::ColorF(0.40f, 0.65f, 0.88f, 1.0f));
    pRT->DrawLine(D2D1::Point2F(hx - eyeSpan - 9.0f * scale, eyeY - 10.0f * scale), D2D1::Point2F(hx - eyeSpan + 9.0f * scale, eyeY - 8.0f * scale), pBrush, 2.2f * scale);
    pRT->DrawLine(D2D1::Point2F(hx + eyeSpan - 9.0f * scale, eyeY - 8.0f * scale), D2D1::Point2F(hx + eyeSpan + 9.0f * scale, eyeY - 10.0f * scale), pBrush, 2.2f * scale);
    // 巩膜底色
    pBrush->SetColor(D2D1::ColorF(0.88f, 0.94f, 0.98f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx - eyeSpan, eyeY), 7.5f * scale, 4.8f * scale), pBrush);
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx + eyeSpan, eyeY), 7.5f * scale, 4.8f * scale), pBrush);
    pBrush->SetColor(D2D1::ColorF(0.40f, 0.68f, 0.90f));
    pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(hx - eyeSpan, eyeY), 7.5f * scale, 4.8f * scale), pBrush, 1.5f * scale);
    pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(hx + eyeSpan, eyeY), 7.5f * scale, 4.8f * scale), pBrush, 1.5f * scale);
    // 晶状体瞳孔与聚焦点
    pBrush->SetColor(D2D1::ColorF(0.0f, 0.85f, 0.65f, 1.0f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx - eyeSpan, eyeY), 4.2f * scale, 4.2f * scale), pBrush);
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx + eyeSpan, eyeY), 4.2f * scale, 4.2f * scale), pBrush);
    pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx - eyeSpan + 1.2f * scale, eyeY - 1.2f * scale), 1.6f * scale, 1.6f * scale), pBrush);
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hx + eyeSpan + 1.2f * scale, eyeY - 1.2f * scale), 1.6f * scale, 1.6f * scale), pBrush);

    // 4. 正面鼻子 (立体鼻梁与微弧鼻翼)
    pBrush->SetColor(D2D1::ColorF(0.55f, 0.75f, 0.92f, 0.85f));
    pRT->DrawLine(D2D1::Point2F(hx, eyeY + 4.0f * scale), D2D1::Point2F(hx, hy + 10.0f * scale), pBrush, 1.8f * scale);
    pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(hx, hy + 11.0f * scale), 4.5f * scale, 2.5f * scale), pBrush, 1.6f * scale);

    // 5. 嘴唇 (温和放松弧线)
    pBrush->SetColor(D2D1::ColorF(0.55f, 0.75f, 0.92f, 0.90f));
    pRT->DrawLine(D2D1::Point2F(hx - 8.0f * scale, hy + 22.0f * scale), D2D1::Point2F(hx + 8.0f * scale, hy + 22.0f * scale), pBrush, 2.0f * scale);
}

void NeckExerciseRenderer::DrawAnatomicalTorso(
    ID2D1RenderTarget* pRT,
    ID2D1SolidColorBrush* pBrush,
    float cx,
    float cy,
    float animScale,
    float ease,
    int stretchSide,
    float canvasBottom
) {
    if (!pRT || !pBrush) return;

    auto& d2d = D2DContext::Instance();

    // 1. 颈窝中心点与双肩峰基准 (拉伸侧肩膀主动下沉固定，深化解剖学真实感)
    float sinkOffset = (stretchSide != 0) ? (ease * 7.0f * animScale) : 0.0f;
    D2D1_POINT_2F notch = D2D1::Point2F(cx, cy + 42.0f * animScale);
    D2D1_POINT_2F leftShoulder = D2D1::Point2F(cx - 106.0f * animScale, cy + 38.0f * animScale + (stretchSide == -1 ? sinkOffset : 0.0f));
    D2D1_POINT_2F rightShoulder = D2D1::Point2F(cx + 106.0f * animScale, cy + 38.0f * animScale + (stretchSide == 1 ? sinkOffset : 0.0f));

    // 2. 胸廓躯干微光剪影闭合底座 (Torso Silhouette - 彻底消灭“悬空铁丝衣架”失重怪相)
    float torsoBottomY = (canvasBottom > cy + 120.0f * animScale)
        ? (std::min)(canvasBottom - 8.0f * animScale, cy + 160.0f * animScale)
        : (cy + 138.0f * animScale);

    ComPtr<ID2D1PathGeometry> torsoBaseGeom;
    d2d.GetD2DFactory()->CreatePathGeometry(torsoBaseGeom.GetAddressOf());
    if (torsoBaseGeom) {
        ComPtr<ID2D1GeometrySink> sink;
        torsoBaseGeom->Open(sink.GetAddressOf());

        // 始于胸骨柄颈窝
        sink->BeginFigure(notch, D2D1_FIGURE_BEGIN_FILLED);

        // 沿左锁骨至左肩峰
        sink->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(cx - 38.0f * animScale, cy + 44.0f * animScale),
            D2D1::Point2F(cx - 72.0f * animScale, cy + 40.0f * animScale),
            leftShoulder
        ));

        // 沿左侧三角肌与肋廓流线下滑至底座
        sink->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(cx - 102.0f * animScale, cy + 64.0f * animScale),
            D2D1::Point2F(cx - 88.0f * animScale, cy + 98.0f * animScale),
            D2D1::Point2F(cx - 72.0f * animScale, torsoBottomY)
        ));

        // 底座水平微弧过渡至右侧
        sink->AddLine(D2D1::Point2F(cx + 72.0f * animScale, torsoBottomY));

        // 沿右侧肋廓流线上滑至右肩峰
        sink->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(cx + 88.0f * animScale, cy + 98.0f * animScale),
            D2D1::Point2F(cx + 102.0f * animScale, cy + 64.0f * animScale),
            rightShoulder
        ));

        // 沿右锁骨回归胸骨柄
        sink->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(cx + 72.0f * animScale, cy + 40.0f * animScale),
            D2D1::Point2F(cx + 38.0f * animScale, cy + 44.0f * animScale),
            notch
        ));

        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->Close();

        // 自上而下柔和半透明微光渐变 (冷海蓝微光向底座自然渐隐至透明)
        D2D1_GRADIENT_STOP baseStops[3];
        baseStops[0].position = 0.0f;
        baseStops[0].color = D2D1::ColorF(0.08f, 0.16f, 0.25f, 0.45f);
        baseStops[1].position = 0.45f;
        baseStops[1].color = D2D1::ColorF(0.04f, 0.10f, 0.18f, 0.25f);
        baseStops[2].position = 1.0f;
        baseStops[2].color = D2D1::ColorF(0.02f, 0.05f, 0.08f, 0.0f);

        ComPtr<ID2D1GradientStopCollection> pBaseStops;
        pRT->CreateGradientStopCollection(baseStops, 3, pBaseStops.GetAddressOf());
        if (pBaseStops) {
            ComPtr<ID2D1LinearGradientBrush> pBaseBrush;
            pRT->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(notch, D2D1::Point2F(cx, torsoBottomY)),
                pBaseStops.Get(),
                pBaseBrush.GetAddressOf()
            );
            if (pBaseBrush) {
                pRT->FillGeometry(torsoBaseGeom.Get(), pBaseBrush.Get());
            }
        }
    }

    // 3. 斜方肌热力张力渐变肌筋膜带 (在拉伸侧呈现自然的肌束受力走向)
    if (ease > 0.08f && stretchSide != 0) {
        float shoulderX = (stretchSide == 1) ? rightShoulder.x : leftShoulder.x;
        float shoulderY = (stretchSide == 1) ? rightShoulder.y : leftShoulder.y;

        ComPtr<ID2D1PathGeometry> trapGeom;
        d2d.GetD2DFactory()->CreatePathGeometry(trapGeom.GetAddressOf());
        if (trapGeom) {
            ComPtr<ID2D1GeometrySink> sink;
            trapGeom->Open(sink.GetAddressOf());
            D2D1_POINT_2F neckOrigin = D2D1::Point2F(cx + stretchSide * 6.0f * animScale, cy + 8.0f * animScale);
            sink->BeginFigure(neckOrigin, D2D1_FIGURE_BEGIN_FILLED);
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(cx + stretchSide * 45.0f * animScale, cy + 18.0f * animScale),
                D2D1::Point2F(cx + stretchSide * 78.0f * animScale, cy + 26.0f * animScale),
                D2D1::Point2F(shoulderX, shoulderY)
            ));
            sink->AddLine(D2D1::Point2F(cx + stretchSide * 28.0f * animScale, cy + 42.0f * animScale));
            sink->AddLine(neckOrigin);
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();

            // 线性渐变热力笔刷 (浅青蓝 -> 温暖琥珀橙红)
            D2D1_GRADIENT_STOP stops[3];
            stops[0].position = 0.0f;
            stops[0].color = D2D1::ColorF(0.15f, 0.85f, 0.95f, 0.25f * ease);
            stops[1].position = 0.55f;
            stops[1].color = D2D1::ColorF(1.0f, 0.65f, 0.15f, 0.55f * ease);
            stops[2].position = 1.0f;
            stops[2].color = D2D1::ColorF(1.0f, 0.25f, 0.25f, 0.80f * ease);

            ComPtr<ID2D1GradientStopCollection> pStops;
            pRT->CreateGradientStopCollection(stops, 3, pStops.GetAddressOf());
            if (pStops) {
                ComPtr<ID2D1LinearGradientBrush> pGradBrush;
                pRT->CreateLinearGradientBrush(
                    D2D1::LinearGradientBrushProperties(neckOrigin, D2D1::Point2F(shoulderX, shoulderY)),
                    pStops.Get(),
                    pGradBrush.GetAddressOf()
                );
                if (pGradBrush) {
                    pRT->FillGeometry(trapGeom.Get(), pGradBrush.Get());
                }
            }

            pBrush->SetColor(D2D1::ColorF(1.0f, 0.85f, 0.40f, 0.60f * ease));
            pRT->DrawGeometry(trapGeom.Get(), pBrush, 1.5f * animScale);
        }
    }

    // 4. 锁骨 (Clavicle) 流线贝塞尔轮廓
    pBrush->SetColor(D2D1::ColorF(0.72f, 0.86f, 0.96f, 0.95f));
    ComPtr<ID2D1PathGeometry> leftClavGeom;
    d2d.GetD2DFactory()->CreatePathGeometry(leftClavGeom.GetAddressOf());
    if (leftClavGeom) {
        ComPtr<ID2D1GeometrySink> sink;
        leftClavGeom->Open(sink.GetAddressOf());
        sink->BeginFigure(notch, D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(cx - 38.0f * animScale, cy + 44.0f * animScale),
            D2D1::Point2F(cx - 72.0f * animScale, cy + 40.0f * animScale),
            leftShoulder
        ));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();
        pRT->DrawGeometry(leftClavGeom.Get(), pBrush, 4.0f * animScale);
    }
    ComPtr<ID2D1PathGeometry> rightClavGeom;
    d2d.GetD2DFactory()->CreatePathGeometry(rightClavGeom.GetAddressOf());
    if (rightClavGeom) {
        ComPtr<ID2D1GeometrySink> sink;
        rightClavGeom->Open(sink.GetAddressOf());
        sink->BeginFigure(notch, D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(cx + 38.0f * animScale, cy + 44.0f * animScale),
            D2D1::Point2F(cx + 72.0f * animScale, cy + 40.0f * animScale),
            rightShoulder
        ));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();
        pRT->DrawGeometry(rightClavGeom.Get(), pBrush, 4.0f * animScale);
    }

    // 5. 胸骨柄中心微标 (Manubrium) 与胸骨纵线
    pBrush->SetColor(D2D1::ColorF(0.85f, 0.95f, 1.0f, 0.95f));
    pRT->FillEllipse(D2D1::Ellipse(notch, 5.0f * animScale, 5.0f * animScale), pBrush);

    pBrush->SetColor(D2D1::ColorF(0.40f, 0.65f, 0.85f, 0.30f));
    pRT->DrawLine(notch, D2D1::Point2F(cx, cy + 96.0f * animScale), pBrush, 1.8f * animScale);
    pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy + 96.0f * animScale), 3.0f * animScale, 3.0f * animScale), pBrush);

    // 6. 三角肌与胸肋两侧流线轮廓
    pBrush->SetColor(D2D1::ColorF(0.55f, 0.72f, 0.88f, 0.75f));
    ComPtr<ID2D1PathGeometry> leftTorso;
    d2d.GetD2DFactory()->CreatePathGeometry(leftTorso.GetAddressOf());
    if (leftTorso) {
        ComPtr<ID2D1GeometrySink> sink;
        leftTorso->Open(sink.GetAddressOf());
        sink->BeginFigure(leftShoulder, D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(cx - 102.0f * animScale, cy + 62.0f * animScale),
            D2D1::Point2F(cx - 92.0f * animScale, cy + 86.0f * animScale),
            D2D1::Point2F(cx - 78.0f * animScale, cy + 106.0f * animScale)
        ));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();
        pRT->DrawGeometry(leftTorso.Get(), pBrush, 3.5f * animScale);
    }
    ComPtr<ID2D1PathGeometry> rightTorso;
    d2d.GetD2DFactory()->CreatePathGeometry(rightTorso.GetAddressOf());
    if (rightTorso) {
        ComPtr<ID2D1GeometrySink> sink;
        rightTorso->Open(sink.GetAddressOf());
        sink->BeginFigure(rightShoulder, D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(cx + 102.0f * animScale, cy + 62.0f * animScale),
            D2D1::Point2F(cx + 92.0f * animScale, cy + 86.0f * animScale),
            D2D1::Point2F(cx + 78.0f * animScale, cy + 106.0f * animScale)
        ));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();
        pRT->DrawGeometry(rightTorso.Get(), pBrush, 3.5f * animScale);
    }
}

void NeckExerciseRenderer::DrawCircularPacer(
    ID2D1RenderTarget* pRT,
    ID2D1SolidColorBrush* pBrush,
    const D2D1_POINT_2F& center,
    float radius,
    float dpiScale,
    int segment,
    [[maybe_unused]] float segProgress,
    float tNorm
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

    // 3. 动态进度扫掠弧线与头部巡航发光珠
    float sweepFraction = std::clamp(tNorm, 0.0f, 1.0f);
    if (sweepFraction > 0.005f) {
        ComPtr<ID2D1PathGeometry> arcGeom;
        d2d.GetD2DFactory()->CreatePathGeometry(arcGeom.GetAddressOf());
        if (arcGeom) {
            ComPtr<ID2D1GeometrySink> sink;
            arcGeom->Open(sink.GetAddressOf());

            const int arcSegments = static_cast<int>(sweepFraction * 72.0f) + 1;
            D2D1_POINT_2F headPt = center;
            for (int i = 0; i <= arcSegments; ++i) {
                float frac = (static_cast<float>(i) / arcSegments) * sweepFraction;
                float angle = -AppConstants::Math::PI * 0.5f + frac * AppConstants::Math::PI * 2.0f;
                D2D1_POINT_2F pt = D2D1::Point2F(
                    center.x + trackRadius * std::cos(angle),
                    center.y + trackRadius * std::sin(angle)
                );
                if (i == 0) sink->BeginFigure(pt, D2D1_FIGURE_BEGIN_HOLLOW);
                else sink->AddLine(pt);
                if (i == arcSegments) headPt = pt;
            }
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            sink->Close();

            D2D1_COLOR_F sweepColor;
            if (segment == 0) sweepColor = D2D1::ColorF(0.20f, 0.85f, 1.0f, 0.95f);
            else if (segment == 1) sweepColor = D2D1::ColorF(1.0f, 0.85f, 0.35f, 1.0f);
            else sweepColor = D2D1::ColorF(0.35f, 1.0f, 0.70f, 0.95f);

            pBrush->SetColor(sweepColor);
            pRT->DrawGeometry(arcGeom.Get(), pBrush, 4.0f * dpiScale);

            // 头部发光微珠
            pBrush->SetColor(D2D1::ColorF(sweepColor.r, sweepColor.g, sweepColor.b, 0.35f));
            pRT->FillEllipse(D2D1::Ellipse(headPt, 6.5f * dpiScale, 6.5f * dpiScale), pBrush);
            pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
            pRT->FillEllipse(D2D1::Ellipse(headPt, 3.0f * dpiScale, 3.0f * dpiScale), pBrush);
        }
    }

    // 4. 中心状态与大字号倒计时展示
    if (segment == 1) {
        float remainHold = (std::max)(0.1f, (0.72f - tNorm) * m_phaseDuration);
        wchar_t buf[16];
        swprintf_s(buf, L"%.1fs", remainHold);

        auto fmtNum = d2d.GetCachedTextFormat(L"Segoe UI", 22.0f * dpiScale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtNum) {
            pBrush->SetColor(D2D1::ColorF(1.0f, 0.88f, 0.40f));
            D2D1_RECT_F rNum = D2D1::RectF(center.x - radius, center.y - 18.0f * dpiScale, center.x + radius, center.y + 6.0f * dpiScale);
            pRT->DrawTextW(buf, static_cast<UINT32>(wcslen(buf)), fmtNum.Get(), rNum, pBrush);
        }

        auto fmtLbl = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 11.5f * dpiScale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtLbl) {
            pBrush->SetColor(D2D1::ColorF(0.85f, 0.95f, 0.90f, 0.88f));
            D2D1_RECT_F rLbl = D2D1::RectF(center.x - radius, center.y + 7.0f * dpiScale, center.x + radius, center.y + 24.0f * dpiScale);
            pRT->DrawTextW(L"深度舒展保持", 6, fmtLbl.Get(), rLbl, pBrush);
        }
    } else {
        std::wstring statusText = (segment == 0) ? L"柔和拉伸" : L"缓慢回正";
        std::wstring subText = (segment == 0) ? L"配合深吸气" : L"平缓呼气";
        D2D1_COLOR_F txtColor = (segment == 0) ? D2D1::ColorF(0.40f, 0.88f, 1.0f) : D2D1::ColorF(0.50f, 1.0f, 0.72f);

        auto fmtMain = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 16.0f * dpiScale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtMain) {
            pBrush->SetColor(txtColor);
            D2D1_RECT_F rMain = D2D1::RectF(center.x - radius, center.y - 14.0f * dpiScale, center.x + radius, center.y + 6.0f * dpiScale);
            pRT->DrawTextW(statusText.c_str(), static_cast<UINT32>(statusText.length()), fmtMain.Get(), rMain, pBrush);
        }

        auto fmtSub = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 11.5f * dpiScale, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtSub) {
            pBrush->SetColor(D2D1::ColorF(0.75f, 0.88f, 0.88f, 0.80f));
            D2D1_RECT_F rSub = D2D1::RectF(center.x - radius, center.y + 7.0f * dpiScale, center.x + radius, center.y + 24.0f * dpiScale);
            pRT->DrawTextW(subText.c_str(), static_cast<UINT32>(subText.length()), fmtSub.Get(), rSub, pBrush);
        }
    }
}

void NeckExerciseRenderer::Render(ID2D1RenderTarget* pRT, const D2D1_RECT_F& bounds, float dpiScale) {
    if (!pRT) return;

    float w = bounds.right - bounds.left;
    float h = bounds.bottom - bounds.top;
    if (w <= 20.0f || h <= 20.0f) return;

    // 1. 基于全屏响应式安全视口计算几何分区与动画缩放 (彻底解除 1.65 锁死)
    auto layout = ExerciseLayout::Calculate(bounds, dpiScale, 460.0f, 260.0f);
    float animScale = layout.animScale;
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

    // 3. 计算节律进度
    float phaseTime = std::fmod(m_animTime, m_phaseDuration);
    float tNorm = phaseTime / m_phaseDuration;
    float ease = 0.0f;
    int segment = 0;
    float segProgress = 0.0f;
    CalculateStretchPacing(tNorm, ease, segment, segProgress);

    auto& d2d = D2DContext::Instance();

    // 4. Zone A: 顶部栏 Header (大字号 Badge 与动作指示)
    float badgeW = 190.0f * uiScale;
    float badgeH = 34.0f * uiScale;
    D2D1_ROUNDED_RECT badgeRect = D2D1::RoundedRect(
        D2D1::RectF(layout.headerRect.left, layout.headerRect.top, layout.headerRect.left + badgeW, layout.headerRect.top + badgeH),
        8.0f * uiScale, 8.0f * uiScale
    );
    if (pBrush) {
        pBrush->SetColor(D2D1::ColorF(0.0f, 0.75f, 0.43f, 0.35f));
        pRT->FillRoundedRectangle(badgeRect, pBrush.Get());
        pBrush->SetColor(D2D1::ColorF(0.35f, 0.95f, 0.65f, 0.60f));
        pRT->DrawRoundedRectangle(badgeRect, pBrush.Get(), 1.2f * uiScale);

        // 发光微标
        float dotSize = 9.0f * uiScale;
        float dotX = badgeRect.rect.left + 12.0f * uiScale;
        float dotY = badgeRect.rect.top + (badgeH - dotSize) / 2.0f;

        pBrush->SetColor(D2D1::ColorF(0.47f, 1.0f, 0.70f, 0.4f));
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX + dotSize / 2.0f, dotY + dotSize / 2.0f), dotSize, dotSize), pBrush.Get());

        pBrush->SetColor(D2D1::ColorF(0.55f, 1.0f, 0.78f, 1.0f));
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX + dotSize / 2.0f, dotY + dotSize / 2.0f), dotSize / 2.0f, dotSize / 2.0f), pBrush.Get());

        pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
        auto fmtBadge = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 13.0f * uiScale, DWRITE_FONT_WEIGHT_BOLD);
        if (fmtBadge) {
            D2D1_RECT_F textRect = D2D1::RectF(dotX + dotSize + 8.0f * uiScale, badgeRect.rect.top + 6.0f * uiScale, badgeRect.rect.right, badgeRect.rect.bottom);
            pRT->DrawTextW(L"🌿 动态解剖肌群导引", 10, fmtBadge.Get(), textRect, pBrush.Get());
        }
    }

    // 顶部右侧阶段概览 (优雅极简指示)
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
        swprintf_s(phaseBuf, L"★ 4阶段调理 · 动作 %d/4", m_currentPhase + 1);

        pBrush->SetColor(D2D1::ColorF(0.70f, 0.95f, 0.85f));
        auto fmtPacer = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 12.0f * uiScale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (fmtPacer) {
            pRT->DrawTextW(phaseBuf, static_cast<UINT32>(wcslen(phaseBuf)), fmtPacer.Get(), pacerRect.rect, pBrush.Get());
        }
    }

    // 5. Zone B: 底部信息卡片 Guidance Dock (黄金分割双栏排版：左侧要领 + 右侧仪表盘)
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

    if (pBrush) {
        D2D1_ROUNDED_RECT dockPlate = D2D1::RoundedRect(layout.dockRect, 14.0f * uiScale, 14.0f * uiScale);
        pBrush->SetColor(D2D1::ColorF(0.04f, 0.08f, 0.12f, 0.85f));
        pRT->FillRoundedRectangle(dockPlate, pBrush.Get());
        pBrush->SetColor(D2D1::ColorF(0.18f, 0.65f, 0.45f, 0.45f));
        pRT->DrawRoundedRectangle(dockPlate, pBrush.Get(), 1.2f * uiScale);

        // 左栏：动作要领与临床依据
        float dockInnerLeft = layout.dockLeftRect.left + 24.0f * uiScale;
        float dockInnerRight = layout.dockLeftRect.right - 12.0f * uiScale;
        float currentY = layout.dockLeftRect.top + 14.0f * uiScale;

        // 行 1：动作标题 (大字号 20 * uiScale，粗体纯白)
        auto fmtTitle = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 20.0f * uiScale, DWRITE_FONT_WEIGHT_BOLD);
        if (fmtTitle) {
            pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f));
            D2D1_RECT_F titleRect = D2D1::RectF(dockInnerLeft, currentY, dockInnerRight, currentY + 28.0f * uiScale);
            pRT->DrawTextW(actionTitle.c_str(), static_cast<UINT32>(actionTitle.length()), fmtTitle.Get(), titleRect, pBrush.Get());
        }

        currentY += 32.0f * uiScale;

        // 行 2：动作要领 (大字号 15 * uiScale，薄荷青绿)
        auto fmtTip = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 15.0f * uiScale, DWRITE_FONT_WEIGHT_BOLD);
        if (fmtTip) {
            pBrush->SetColor(D2D1::ColorF(0.55f, 1.0f, 0.72f));
            std::wstring fullTip = L"● 动作要领：" + actionTip;
            D2D1_RECT_F tipRect = D2D1::RectF(dockInnerLeft, currentY, dockInnerRight, currentY + 24.0f * uiScale);
            pRT->DrawTextW(fullTip.c_str(), static_cast<UINT32>(fullTip.length()), fmtTip.Get(), tipRect, pBrush.Get());
        }

        currentY += 26.0f * uiScale;

        // 行 3：医学依据 (字号 13 * uiScale，淡青灰柔光呼吸色)
        auto fmtSubTip = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 13.0f * uiScale, DWRITE_FONT_WEIGHT_REGULAR);
        if (fmtSubTip) {
            pBrush->SetColor(D2D1::ColorF(0.80f, 0.90f, 0.86f, 0.95f));
            std::wstring fullSubTip = L"● 医学依据：" + subTip;
            D2D1_RECT_F subTipRect = D2D1::RectF(dockInnerLeft, currentY, dockInnerRight, layout.dockLeftRect.bottom - 10.0f * uiScale);
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

        // 右栏：动态环形节拍仪表盘 (彻底利用右侧空间，左右视觉达成平衡)
        DrawCircularPacer(pRT, pBrush.Get(), layout.dockMeterCenter, layout.dockMeterRadius, uiScale, segment, segProgress, tNorm);
    }

    // 6. Zone C: 中央动画独立画布 Central Canvas (大幅面自适应缩放，解剖流线人偶)
    float cx = layout.canvasCenterX;
    float cy = layout.canvasCenterY;
    float headRadius = 38.0f * animScale;

    if (m_currentPhase == 0) {
        // 动作一：水平后缩下巴
        float shiftX = -ease * (38.0f * animScale);
        float liftY = -ease * (7.0f * animScale);

        // 真实解剖学流线躯干与锁骨 (稳固胸腔微光剪影底座)
        DrawAnatomicalTorso(pRT, pBrush.Get(), cx, cy, animScale, ease, 0, layout.canvasRect.bottom);

        // 颈椎 C1-C7 节段
        D2D1_POINT_2F neckBase = D2D1::Point2F(cx, cy + 42.0f * animScale);
        D2D1_POINT_2F headJoint = D2D1::Point2F(cx + shiftX, cy - 6.0f * animScale + liftY);

        if (pBrush) {
            pBrush->SetColor(D2D1::ColorF(0.30f, 0.80f, 1.0f, 0.9f));
            pRT->DrawLine(neckBase, headJoint, pBrush.Get(), 7.0f * animScale);

            pBrush->SetColor(D2D1::ColorF(0.70f, 0.98f, 1.0f));
            for (int i = 0; i < 7; ++i) {
                float f = i / 6.0f;
                float px = neckBase.x + (headJoint.x - neckBase.x) * f - static_cast<float>(std::sin(f * AppConstants::Math::PI)) * (9.0f + ease * 5.0f) * animScale;
                float py = neckBase.y + (headJoint.y - neckBase.y) * f;
                pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), 4.8f * animScale, 4.8f * animScale), pBrush.Get());
            }
        }

        // 绘制带五官轮廓与视线光束的侧面头部
        float hx = cx + shiftX;
        float hy = cy - 44.0f * animScale + liftY;
        DrawProfileHead(pRT, pBrush.Get(), d2d.GetD2DFactory(), hx, hy, headRadius, animScale, true, ease, false);

    } else if (m_currentPhase == 1) {
        // 动作二：受控仰角复位 25°
        float angle = -ease * 25.0f;

        // 真实解剖学流线躯干与锁骨 (稳固胸腔微光剪影底座)
        DrawAnatomicalTorso(pRT, pBrush.Get(), cx, cy, animScale, ease, 0, layout.canvasRect.bottom);

        // 旋转坐标系：绕胸锁关节枢纽点仰角旋转
        D2D1_MATRIX_3X2_F oldTransform;
        pRT->GetTransform(&oldTransform);
        D2D1_POINT_2F pivot = D2D1::Point2F(cx, cy + 40.0f * animScale);
        pRT->SetTransform(D2D1::Matrix3x2F::Rotation(angle, pivot) * oldTransform);

        float hx = cx;
        float hy = cy - 10.0f * animScale - headRadius * 0.90f;

        if (pBrush) {
            // 颈椎 C1-C7
            pBrush->SetColor(D2D1::ColorF(0.30f, 0.80f, 1.0f, 0.9f));
            pRT->DrawLine(pivot, D2D1::Point2F(cx, cy - 10.0f * animScale), pBrush.Get(), 7.0f * animScale);

            pBrush->SetColor(D2D1::ColorF(0.70f, 0.98f, 1.0f));
            for (int i = 0; i < 6; ++i) {
                float f = i / 5.0f;
                float py = pivot.y - 50.0f * animScale * f;
                pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, py), 4.8f * animScale, 4.8f * animScale), pBrush.Get());
            }

            // 动态胸锁乳突肌 (SCM) 与颈前肌群舒展热力拉伸线 (消除串珠怪相)
            if (ease > 0.05f) {
                // 颈前喉肌拉伸流线 (下颌下缘至胸骨柄)
                D2D1_POINT_2F chinPt = D2D1::Point2F(hx + headRadius * 0.70f, hy + headRadius * 0.65f);
                pBrush->SetColor(D2D1::ColorF(0.20f, 0.95f, 0.68f, 0.45f * ease));
                pRT->DrawLine(chinPt, pivot, pBrush.Get(), 3.2f * animScale);

                // 胸锁乳突肌侧束 (耳后乳突至锁骨内侧端)
                D2D1_POINT_2F mastoidPt = D2D1::Point2F(hx - headRadius * 0.06f, hy + headRadius * 0.28f);
                pBrush->SetColor(D2D1::ColorF(0.35f, 0.85f, 1.0f, 0.35f * ease));
                pRT->DrawLine(mastoidPt, pivot, pBrush.Get(), 2.0f * animScale);
            }

            // 绘制向上仰起 25° 的五官侧面头部
            DrawProfileHead(pRT, pBrush.Get(), d2d.GetD2DFactory(), hx, hy, headRadius, animScale, false, 0.0f, true);
        }

        pRT->SetTransform(oldTransform);

    } else {
        // 动作三/四：左右侧拉伸
        float sideAngle = (m_currentPhase == 2 ? -1.0f : 1.0f) * ease * 25.0f;
        int stretchSide = (m_currentPhase == 2) ? 1 : -1;

        // 真实解剖学流线躯干与斜方肌张力热力带 (稳固胸腔微光剪影底座)
        DrawAnatomicalTorso(pRT, pBrush.Get(), cx, cy, animScale, ease, stretchSide, layout.canvasRect.bottom);

        // 旋转正面头部
        D2D1_MATRIX_3X2_F oldTransform;
        pRT->GetTransform(&oldTransform);
        D2D1_POINT_2F pivot = D2D1::Point2F(cx, cy + 40.0f * animScale);
        pRT->SetTransform(D2D1::Matrix3x2F::Rotation(sideAngle, pivot) * oldTransform);

        if (pBrush) {
            // 正面颈椎
            pBrush->SetColor(D2D1::ColorF(0.30f, 0.80f, 1.0f, 0.9f));
            pRT->DrawLine(pivot, D2D1::Point2F(cx, cy - 6.0f * animScale), pBrush.Get(), 7.0f * animScale);

            pBrush->SetColor(D2D1::ColorF(0.70f, 0.98f, 1.0f));
            for (int i = 0; i < 6; ++i) {
                float f = i / 5.0f;
                float py = pivot.y - 48.0f * animScale * f;
                pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, py), 4.8f * animScale, 4.8f * animScale), pBrush.Get());
            }

            // 绘制倾斜的正面五官
            float hx = cx;
            float hy = cy - 6.0f * animScale - headRadius * 0.90f;
            DrawFrontHead(pRT, pBrush.Get(), hx, hy, headRadius, animScale);
        }

        pRT->SetTransform(oldTransform);
    }
}

