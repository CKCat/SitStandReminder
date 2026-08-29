#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "MascotRenderer.hpp"
#include "D2DContext.hpp"
#include <algorithm>
#include <cmath>

// 绘制外边框圆角矩形流光进度条 (从顶部 12 点钟方向顺时针流转)
void MascotRenderer::DrawProgressRing(
    ID2D1RenderTarget* /*pTarget*/,
    D2D1_POINT_2F /*center*/,
    float /*radius*/,
    float /*strokeW*/,
    float /*progress*/,
    D2D1_COLOR_F /*accentColor*/
) {
    // 留空以保持头文件兼容
}

void MascotRenderer::DrawRoundedRectProgress(
    ID2D1RenderTarget* pTarget,
    D2D1_RECT_F rect,
    float radius,
    float strokeW,
    float progress,
    D2D1_COLOR_F accentColor
) {
    if (!pTarget) return;

    ComPtr<ID2D1SolidColorBrush> pBrush;
    pTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f), pBrush.GetAddressOf());
    if (!pBrush) return;

    // 1. 底层静默暗光完整边框轨道
    D2D1_ROUNDED_RECT bgRounded = D2D1::RoundedRect(rect, radius, radius);
    pTarget->DrawRoundedRectangle(bgRounded, pBrush.Get(), strokeW);

    if (progress <= 0.001f) return;
    progress = std::clamp(progress, 0.0f, 1.0f);

    // 2. 动态构建顺时针圆角矩形流光几何路径
    float w = rect.right - rect.left;
    float h = rect.bottom - rect.top;
    float r = (std::min)(radius, (std::min)(w, h) / 2.0f);
    float cx = rect.left + w / 2.0f;

    // 9段几何分段长度
    float L_top1 = (rect.right - r) - cx;
    float L_arcTR = AppConstants::Math::PI * 0.5f * r;
    float L_right = (rect.bottom - r) - (rect.top + r);
    float L_arcBR = AppConstants::Math::PI * 0.5f * r;
    float L_bottom = (rect.right - r) - (rect.left + r);
    float L_arcBL = AppConstants::Math::PI * 0.5f * r;
    float L_left = (rect.bottom - r) - (rect.top + r);
    float L_arcTL = AppConstants::Math::PI * 0.5f * r;
    float L_top2 = cx - (rect.left + r);

    float totalLen = L_top1 + L_arcTR + L_right + L_arcBR + L_bottom + L_arcBL + L_left + L_arcTL + L_top2;
    // Bug-5 修复: 添加微小容差防止 9 段浮点累积误差导致末段 remain 计算为负值
    float targetLen = totalLen * progress + 1e-3f;

    ComPtr<ID2D1PathGeometry> pathGeom;
    D2DContext::Instance().GetD2DFactory()->CreatePathGeometry(pathGeom.GetAddressOf());
    if (!pathGeom) return;

    ComPtr<ID2D1GeometrySink> sink;
    pathGeom->Open(sink.GetAddressOf());
    sink->BeginFigure(D2D1::Point2F(cx, rect.top), D2D1_FIGURE_BEGIN_HOLLOW);

    float accumulated = 0.0f;

    // 逐段构建
    // 1. Top Right Line
    if (accumulated + L_top1 <= targetLen) {
        sink->AddLine(D2D1::Point2F(rect.right - r, rect.top));
        accumulated += L_top1;

        // 2. Top-Right Arc
        if (accumulated + L_arcTR <= targetLen) {
            sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(rect.right, rect.top + r), D2D1::SizeF(r, r), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
            accumulated += L_arcTR;

            // 3. Right Line
            if (accumulated + L_right <= targetLen) {
                sink->AddLine(D2D1::Point2F(rect.right, rect.bottom - r));
                accumulated += L_right;

                // 4. Bottom-Right Arc
                if (accumulated + L_arcBR <= targetLen) {
                    sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(rect.right - r, rect.bottom), D2D1::SizeF(r, r), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
                    accumulated += L_arcBR;

                    // 5. Bottom Line
                    if (accumulated + L_bottom <= targetLen) {
                        sink->AddLine(D2D1::Point2F(rect.left + r, rect.bottom));
                        accumulated += L_bottom;

                        // 6. Bottom-Left Arc
                        if (accumulated + L_arcBL <= targetLen) {
                            sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(rect.left, rect.bottom - r), D2D1::SizeF(r, r), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
                            accumulated += L_arcBL;

                            // 7. Left Line
                            if (accumulated + L_left <= targetLen) {
                                sink->AddLine(D2D1::Point2F(rect.left, rect.top + r));
                                accumulated += L_left;

                                // 8. Top-Left Arc
                                if (accumulated + L_arcTL <= targetLen) {
                                    sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(rect.left + r, rect.top), D2D1::SizeF(r, r), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
                                    accumulated += L_arcTL;

                                    // 9. Top Left Line
                                    float remain = targetLen - accumulated;
                                    float endX = (rect.left + r) + remain;
                                    sink->AddLine(D2D1::Point2F(endX, rect.top));
                                } else {
                                    float remain = targetLen - accumulated;
                                    float angle = (remain / L_arcTL) * (AppConstants::Math::PI * 0.5f);
                                    float arcX = (rect.left + r) - r * cosf(angle);
                                    float arcY = (rect.top + r) - r * sinf(angle);
                                    sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(arcX, arcY), D2D1::SizeF(r, r), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
                                }
                            } else {
                                float remain = targetLen - accumulated;
                                sink->AddLine(D2D1::Point2F(rect.left, (rect.bottom - r) - remain));
                            }
                        } else {
                            float remain = targetLen - accumulated;
                            float angle = (remain / L_arcBL) * (AppConstants::Math::PI * 0.5f);
                            float arcX = (rect.left + r) - r * sinf(angle);
                            float arcY = (rect.bottom - r) + r * cosf(angle);
                            sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(arcX, arcY), D2D1::SizeF(r, r), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
                        }
                    } else {
                        float remain = targetLen - accumulated;
                        sink->AddLine(D2D1::Point2F((rect.right - r) - remain, rect.bottom));
                    }
                } else {
                    float remain = targetLen - accumulated;
                    float angle = (remain / L_arcBR) * (AppConstants::Math::PI * 0.5f);
                    float arcX = (rect.right - r) + r * cosf(angle);
                    float arcY = (rect.bottom - r) + r * sinf(angle);
                    sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(arcX, arcY), D2D1::SizeF(r, r), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
                }
            } else {
                float remain = targetLen - accumulated;
                sink->AddLine(D2D1::Point2F(rect.right, (rect.top + r) + remain));
            }
        } else {
            float remain = targetLen - accumulated;
            float angle = (remain / L_arcTR) * (AppConstants::Math::PI * 0.5f);
            float arcX = (rect.right - r) + r * sinf(angle);
            float arcY = (rect.top + r) - r * cosf(angle);
            sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(arcX, arcY), D2D1::SizeF(r, r), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
        }
    } else {
        float remain = targetLen;
        sink->AddLine(D2D1::Point2F(cx + remain, rect.top));
    }

    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    sink->Close();

    pBrush->SetColor(accentColor);
    pTarget->DrawGeometry(pathGeom.Get(), pBrush.Get(), strokeW);
}

// 极简基础模式：大画幅、高辨识度、现代人体工学健康姿态微标 (Round-Cap + 主题强对比 + 状态色点缀)
static void DrawMinimalistErgonomic(
    ID2D1RenderTarget* pTarget,
    ID2D1SolidColorBrush* pBrush,
    D2D1_POINT_2F center,
    float radius,
    AppState state,
    float scale,
    D2D1_COLOR_F accentColor,
    bool isDark = true
) {
    if (!pTarget || !pBrush) return;

    float r = radius * 1.15f; // 大幅面全景展示

    // 1. 配色体系：强对比主体 + 状态色点缀 + 层次化家具结构
    D2D1_COLOR_F bodyMainColor = isDark 
        ? D2D1::ColorF(0.96f, 0.98f, 1.0f, 1.0f)       // 深色：纯净雪白
        : D2D1::ColorF(0.09f, 0.13f, 0.20f, 1.0f);      // 浅色：深邃墨黑 (与右侧倒计时数字同色)

    D2D1_COLOR_F chairColor = isDark 
        ? D2D1::ColorF(0.48f, 0.58f, 0.72f, 0.85f)     // 深色：科技蓝灰
        : D2D1::ColorF(0.55f, 0.62f, 0.72f, 0.90f);     // 浅色：清晰钢灰

    D2D1_COLOR_F deskColor = isDark 
        ? D2D1::ColorF(0.38f, 0.48f, 0.60f, 0.75f) 
        : D2D1::ColorF(0.68f, 0.73f, 0.80f, 0.85f);

    // 2. 复用常驻圆头笔触样式 (Round Cap，零每帧 COM 堆分配)
    ID2D1StrokeStyle* pStrokeStyle = D2DContext::Instance().GetRoundStrokeStyle();

    if (state == AppState::Standing) {
        // ========== 挺拔站立姿态 (Standing Posture) ==========
        // 1. 头部 (采用 Accent 状态色，活力点睛)
        pBrush->SetColor(accentColor);
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x, center.y - r * 0.68f), 4.2f * scale, 4.2f * scale), pBrush);

        // 2. 挺拔脊椎躯干 (加粗 3.6px 饱满挺拔)
        pBrush->SetColor(bodyMainColor);
        pTarget->DrawLine(
            D2D1::Point2F(center.x, center.y - r * 0.38f),
            D2D1::Point2F(center.x, center.y + r * 0.22f),
            pBrush, 3.6f * scale, pStrokeStyle
        );

        // 3. 双腿挺拔直立 (加粗 3.2px)
        pTarget->DrawLine(
            D2D1::Point2F(center.x, center.y + r * 0.22f),
            D2D1::Point2F(center.x - r * 0.24f, center.y + r * 0.90f),
            pBrush, 3.2f * scale, pStrokeStyle
        );
        pTarget->DrawLine(
            D2D1::Point2F(center.x, center.y + r * 0.22f),
            D2D1::Point2F(center.x + r * 0.24f, center.y + r * 0.90f),
            pBrush, 3.2f * scale, pStrokeStyle
        );

        // 4. 双臂舒展向上伸展
        pBrush->SetColor(accentColor);
        pTarget->DrawLine(
            D2D1::Point2F(center.x, center.y - r * 0.22f),
            D2D1::Point2F(center.x - r * 0.48f, center.y - r * 0.58f),
            pBrush, 2.8f * scale, pStrokeStyle
        );
        pTarget->DrawLine(
            D2D1::Point2F(center.x, center.y - r * 0.22f),
            D2D1::Point2F(center.x + r * 0.48f, center.y - r * 0.58f),
            pBrush, 2.8f * scale, pStrokeStyle
        );

    } else {
        // ========== 90° 科学健康工学坐姿 (Ergonomic Sitting Posture) ==========
        // 1. 人体工学椅 (Chassis & Ergonomic Backrest)
        pBrush->SetColor(chairColor);

        // 工学弧度椅背 (垂直贴合脊椎)
        pTarget->DrawLine(
            D2D1::Point2F(center.x - r * 0.46f, center.y - r * 0.60f),
            D2D1::Point2F(center.x - r * 0.46f, center.y + r * 0.35f),
            pBrush, 2.8f * scale, pStrokeStyle
        );
        // 工学座垫 (水平加厚承托)
        pTarget->DrawLine(
            D2D1::Point2F(center.x - r * 0.56f, center.y + r * 0.35f),
            D2D1::Point2F(center.x + r * 0.18f, center.y + r * 0.35f),
            pBrush, 2.8f * scale, pStrokeStyle
        );
        // 椅子中轴立柱与底座
        pTarget->DrawLine(
            D2D1::Point2F(center.x - r * 0.25f, center.y + r * 0.35f),
            D2D1::Point2F(center.x - r * 0.25f, center.y + r * 0.85f),
            pBrush, 2.2f * scale, pStrokeStyle
        );
        pTarget->DrawLine(
            D2D1::Point2F(center.x - r * 0.50f, center.y + r * 0.85f),
            D2D1::Point2F(center.x - r * 0.02f, center.y + r * 0.85f),
            pBrush, 2.2f * scale, pStrokeStyle
        );

        // 2. 前方办公桌面 (Desk)
        pBrush->SetColor(deskColor);
        pTarget->DrawLine(
            D2D1::Point2F(center.x + r * 0.26f, center.y + r * 0.08f),
            D2D1::Point2F(center.x + r * 0.72f, center.y + r * 0.08f),
            pBrush, 2.2f * scale, pStrokeStyle
        );

        // 3. 人体头部 (采用 AccentColor 状态翡翠绿/科技蓝高亮，与标题色呼应)
        pBrush->SetColor(accentColor);
        pTarget->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(center.x - r * 0.12f, center.y - r * 0.58f), 4.0f * scale, 4.0f * scale),
            pBrush
        );

        // 4. 90° 挺拔背部脊椎 (加粗 3.6px 强对比)
        pBrush->SetColor(bodyMainColor);
        pTarget->DrawLine(
            D2D1::Point2F(center.x - r * 0.16f, center.y - r * 0.30f),
            D2D1::Point2F(center.x - r * 0.16f, center.y + r * 0.22f),
            pBrush, 3.6f * scale, pStrokeStyle
        );

        // 5. 大腿 (水平 90° 屈膝，加粗 3.4px)
        pTarget->DrawLine(
            D2D1::Point2F(center.x - r * 0.16f, center.y + r * 0.22f),
            D2D1::Point2F(center.x + r * 0.40f, center.y + r * 0.22f),
            pBrush, 3.4f * scale, pStrokeStyle
        );

        // 6. 小腿 (垂直 90° 脚平放地面，加粗 3.2px)
        pTarget->DrawLine(
            D2D1::Point2F(center.x + r * 0.40f, center.y + r * 0.22f),
            D2D1::Point2F(center.x + r * 0.40f, center.y + r * 0.82f),
            pBrush, 3.2f * scale, pStrokeStyle
        );

        // 7. 手臂前伸打字姿态 (采用 Accent 状态色，生动专注)
        pBrush->SetColor(accentColor);
        pTarget->DrawLine(
            D2D1::Point2F(center.x - r * 0.14f, center.y - r * 0.05f),
            D2D1::Point2F(center.x + r * 0.36f, center.y + r * 0.05f),
            pBrush, 2.6f * scale, pStrokeStyle
        );
    }
}

void MascotRenderer::DrawCapybara(
    ID2D1RenderTarget* pTarget,
    ID2D1SolidColorBrush* pBrush,
    D2D1_POINT_2F center,
    float radius,
    AppState state,
    int animTick,
    float scale,
    bool /*isDark*/
) {
    if (!pTarget || !pBrush) return;

    float r = radius * 1.10f; // 大幅面全景

    if (state == AppState::Standing) {
        // ========== 水豚挺拔站立 (Standing Capybara) ==========
        pBrush->SetColor(D2D1::ColorF(0.48f, 0.32f, 0.20f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x - r * 0.28f, center.y + r * 0.82f), 3.2f * scale, 2.2f * scale), pBrush);
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x + r * 0.28f, center.y + r * 0.82f), 3.2f * scale, 2.2f * scale), pBrush);

        pBrush->SetColor(D2D1::ColorF(0.68f, 0.48f, 0.32f));
        D2D1_ROUNDED_RECT bodyRect = D2D1::RoundedRect(
            D2D1::RectF(center.x - r * 0.48f, center.y - r * 0.20f, center.x + r * 0.48f, center.y + r * 0.82f),
            r * 0.25f, r * 0.25f
        );
        pTarget->FillRoundedRectangle(bodyRect, pBrush);

        pBrush->SetColor(D2D1::ColorF(0.72f, 0.52f, 0.36f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x - r * 0.58f, center.y - r * 0.30f), 3.0f * scale, 3.0f * scale), pBrush);
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x + r * 0.58f, center.y - r * 0.30f), 3.0f * scale, 3.0f * scale), pBrush);

        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x, center.y - r * 0.35f), r * 0.48f, r * 0.40f), pBrush);

        pBrush->SetColor(D2D1::ColorF(0.52f, 0.35f, 0.22f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x - r * 0.42f, center.y - r * 0.65f), 2.8f * scale, 2.8f * scale), pBrush);
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x + r * 0.42f, center.y - r * 0.65f), 2.8f * scale, 2.8f * scale), pBrush);

        pBrush->SetColor(D2D1::ColorF(0.20f, 0.14f, 0.10f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x, center.y - r * 0.22f), 2.2f * scale, 1.6f * scale), pBrush);
        pTarget->DrawLine(D2D1::Point2F(center.x - r * 0.25f, center.y - r * 0.38f), D2D1::Point2F(center.x - r * 0.08f, center.y - r * 0.38f), pBrush, 1.6f * scale);
        pTarget->DrawLine(D2D1::Point2F(center.x + r * 0.08f, center.y - r * 0.38f), D2D1::Point2F(center.x + r * 0.25f, center.y - r * 0.38f), pBrush, 1.6f * scale);

        float orangeY = center.y - r * 0.85f;
        pBrush->SetColor(D2D1::ColorF(1.0f, 0.55f, 0.05f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x, orangeY), 3.4f * scale, 3.2f * scale), pBrush);
    } else {
        // ========== 真实工学坐姿：水豚坐在椅子上认真敲代码 (True Sitting Pose) ==========
        // 1. 办公工学椅背与座垫
        pBrush->SetColor(D2D1::ColorF(0.22f, 0.28f, 0.38f));
        D2D1_ROUNDED_RECT chairBack = D2D1::RoundedRect(
            D2D1::RectF(center.x - r * 0.70f, center.y - r * 0.50f, center.x - r * 0.40f, center.y + r * 0.45f),
            2.5f * scale, 2.5f * scale
        );
        pTarget->FillRoundedRectangle(chairBack, pBrush);

        D2D1_ROUNDED_RECT chairSeat = D2D1::RoundedRect(
            D2D1::RectF(center.x - r * 0.72f, center.y + r * 0.40f, center.x + r * 0.15f, center.y + r * 0.55f),
            2.0f * scale, 2.0f * scale
        );
        pTarget->FillRoundedRectangle(chairSeat, pBrush);

        pBrush->SetColor(D2D1::ColorF(0.40f, 0.45f, 0.55f));
        pTarget->DrawLine(D2D1::Point2F(center.x - r * 0.35f, center.y + r * 0.55f), D2D1::Point2F(center.x - r * 0.35f, center.y + r * 0.85f), pBrush, 2.0f * scale);
        pTarget->DrawLine(D2D1::Point2F(center.x - r * 0.55f, center.y + r * 0.85f), D2D1::Point2F(center.x - r * 0.15f, center.y + r * 0.85f), pBrush, 2.0f * scale);

        // 2. 稳坐的水豚躯干
        pBrush->SetColor(D2D1::ColorF(0.68f, 0.48f, 0.32f));
        D2D1_ROUNDED_RECT bodyRect = D2D1::RoundedRect(
            D2D1::RectF(center.x - r * 0.45f, center.y - r * 0.15f, center.x + r * 0.18f, center.y + r * 0.45f),
            r * 0.20f, r * 0.20f
        );
        pTarget->FillRoundedRectangle(bodyRect, pBrush);

        pBrush->SetColor(D2D1::ColorF(0.48f, 0.32f, 0.20f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x - r * 0.05f, center.y + r * 0.46f), 3.2f * scale, 2.2f * scale), pBrush);

        // 3. 水豚头部
        pBrush->SetColor(D2D1::ColorF(0.72f, 0.52f, 0.36f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x - r * 0.10f, center.y - r * 0.35f), r * 0.44f, r * 0.38f), pBrush);

        pBrush->SetColor(D2D1::ColorF(0.52f, 0.35f, 0.22f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x - r * 0.40f, center.y - r * 0.58f), 2.5f * scale, 2.5f * scale), pBrush);

        pBrush->SetColor(D2D1::ColorF(0.20f, 0.14f, 0.10f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x + r * 0.15f, center.y - r * 0.25f), 2.0f * scale, 1.5f * scale), pBrush);
        pTarget->DrawLine(D2D1::Point2F(center.x - r * 0.18f, center.y - r * 0.38f), D2D1::Point2F(center.x - r * 0.02f, center.y - r * 0.38f), pBrush, 1.6f * scale);

        // 4. 前方办公桌与打字小爪爪
        pBrush->SetColor(D2D1::ColorF(0.24f, 0.30f, 0.40f));
        D2D1_ROUNDED_RECT deskRect = D2D1::RoundedRect(
            D2D1::RectF(center.x + r * 0.25f, center.y + r * 0.10f, center.x + r * 0.75f, center.y + r * 0.85f),
            2.0f * scale, 2.0f * scale
        );
        pTarget->FillRoundedRectangle(deskRect, pBrush);

        pBrush->SetColor(D2D1::ColorF(0.0f, 0.85f, 1.0f, 0.85f));
        pTarget->DrawLine(D2D1::Point2F(center.x + r * 0.55f, center.y - r * 0.15f), D2D1::Point2F(center.x + r * 0.38f, center.y + r * 0.10f), pBrush, 1.8f * scale);

        float pawOffset = ((animTick / 4) % 2 == 0) ? 1.0f * scale : -1.0f * scale;
        pBrush->SetColor(D2D1::ColorF(0.58f, 0.40f, 0.28f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x + r * 0.22f, center.y + r * 0.12f + pawOffset), 2.5f * scale, 1.8f * scale), pBrush);

        // 5. 头顶小橘子 🍊
        float orangeY = center.y - r * 0.75f;
        pBrush->SetColor(D2D1::ColorF(1.0f, 0.55f, 0.05f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x - r * 0.10f, orangeY), 3.2f * scale, 3.0f * scale), pBrush);
        pBrush->SetColor(D2D1::ColorF(0.20f, 0.75f, 0.35f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x - r * 0.10f + 1.8f * scale, orangeY - 2.0f * scale), 1.5f * scale, 0.9f * scale), pBrush);
    }
}

void MascotRenderer::DrawPixelCat(
    ID2D1RenderTarget* pTarget,
    ID2D1SolidColorBrush* pBrush,
    D2D1_POINT_2F center,
    float radius,
    AppState state,
    int animTick,
    float scale,
    bool /*isDark*/
) {
    if (!pTarget || !pBrush) return;

    float r = radius * 1.10f;

    if (state == AppState::Standing) {
        pBrush->SetColor(D2D1::ColorF(0.96f, 0.70f, 0.38f));
        D2D1_ROUNDED_RECT catBody = D2D1::RoundedRect(
            D2D1::RectF(center.x - r * 0.35f, center.y - r * 0.15f, center.x + r * 0.35f, center.y + r * 0.82f),
            r * 0.18f, r * 0.18f
        );
        pTarget->FillRoundedRectangle(catBody, pBrush);

        pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x - r * 0.50f, center.y - r * 0.45f), 2.8f * scale, 2.8f * scale), pBrush);
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x + r * 0.50f, center.y - r * 0.45f), 2.8f * scale, 2.8f * scale), pBrush);

        pBrush->SetColor(D2D1::ColorF(0.96f, 0.70f, 0.38f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x, center.y - r * 0.30f), r * 0.50f, r * 0.42f), pBrush);

        pBrush->SetColor(D2D1::ColorF(1.0f, 0.75f, 0.80f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x - r * 0.35f, center.y - r * 0.65f), 2.2f * scale, 2.5f * scale), pBrush);
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x + r * 0.35f, center.y - r * 0.65f), 2.2f * scale, 2.5f * scale), pBrush);
    } else {
        // 坐姿小猫
        pBrush->SetColor(D2D1::ColorF(0.22f, 0.28f, 0.38f));
        D2D1_ROUNDED_RECT chairBack = D2D1::RoundedRect(
            D2D1::RectF(center.x - r * 0.70f, center.y - r * 0.45f, center.x - r * 0.40f, center.y + r * 0.45f),
            2.5f * scale, 2.5f * scale
        );
        pTarget->FillRoundedRectangle(chairBack, pBrush);

        D2D1_ROUNDED_RECT chairSeat = D2D1::RoundedRect(
            D2D1::RectF(center.x - r * 0.72f, center.y + r * 0.40f, center.x + r * 0.15f, center.y + r * 0.55f),
            2.0f * scale, 2.0f * scale
        );
        pTarget->FillRoundedRectangle(chairSeat, pBrush);

        pBrush->SetColor(D2D1::ColorF(0.96f, 0.70f, 0.38f));
        D2D1_ROUNDED_RECT catBody = D2D1::RoundedRect(
            D2D1::RectF(center.x - r * 0.42f, center.y - r * 0.15f, center.x + r * 0.18f, center.y + r * 0.45f),
            r * 0.18f, r * 0.18f
        );
        pTarget->FillRoundedRectangle(catBody, pBrush);

        pBrush->SetColor(D2D1::ColorF(0.90f, 0.62f, 0.32f));
        pTarget->DrawLine(D2D1::Point2F(center.x - r * 0.40f, center.y + r * 0.35f), D2D1::Point2F(center.x - r * 0.60f, center.y + r * 0.10f), pBrush, 2.0f * scale);

        pBrush->SetColor(D2D1::ColorF(0.96f, 0.70f, 0.38f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x - r * 0.08f, center.y - r * 0.32f), r * 0.44f, r * 0.38f), pBrush);

        pBrush->SetColor(D2D1::ColorF(1.0f, 0.75f, 0.80f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x - r * 0.32f, center.y - r * 0.60f), 2.2f * scale, 2.6f * scale), pBrush);
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x + r * 0.08f, center.y - r * 0.60f), 2.2f * scale, 2.6f * scale), pBrush);

        pBrush->SetColor(D2D1::ColorF(0.25f, 0.18f, 0.12f));
        pTarget->DrawLine(D2D1::Point2F(center.x - r * 0.18f, center.y - r * 0.32f), D2D1::Point2F(center.x - r * 0.05f, center.y - r * 0.32f), pBrush, 1.6f * scale);

        pBrush->SetColor(D2D1::ColorF(0.24f, 0.30f, 0.40f));
        D2D1_ROUNDED_RECT deskRect = D2D1::RoundedRect(
            D2D1::RectF(center.x + r * 0.25f, center.y + r * 0.10f, center.x + r * 0.75f, center.y + r * 0.85f),
            2.0f * scale, 2.0f * scale
        );
        pTarget->FillRoundedRectangle(deskRect, pBrush);

        float catPawOffset = ((animTick / 4) % 2 == 0) ? 0.9f * scale : -0.9f * scale;
        pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f));
        pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x + r * 0.20f, center.y + r * 0.12f + catPawOffset), 2.5f * scale, 1.8f * scale), pBrush);
    }
}

void MascotRenderer::DrawCyberBot(
    ID2D1RenderTarget* pTarget,
    ID2D1SolidColorBrush* pBrush,
    D2D1_POINT_2F center,
    float radius,
    AppState state,
    int /*animTick*/,
    float scale,
    bool /*isDark*/
) {
    if (!pTarget || !pBrush) return;

    float r = radius * 1.10f;

    pBrush->SetColor(D2D1::ColorF(0.72f, 0.78f, 0.88f));
    D2D1_ROUNDED_RECT botHead = D2D1::RoundedRect(
        D2D1::RectF(center.x - r * 0.45f, center.y - r * 0.65f, center.x + r * 0.25f, center.y - r * 0.05f),
        3.5f * scale, 3.5f * scale
    );
    pTarget->FillRoundedRectangle(botHead, pBrush);

    pBrush->SetColor(D2D1::ColorF(0.0f, 0.90f, 1.0f));
    pTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x + r * 0.05f, center.y - r * 0.35f), 2.6f * scale, 2.6f * scale), pBrush);

    if (state == AppState::Standing) {
        pBrush->SetColor(D2D1::ColorF(0.40f, 0.50f, 0.65f));
        pTarget->DrawLine(D2D1::Point2F(center.x - r * 0.20f, center.y - r * 0.05f), D2D1::Point2F(center.x - r * 0.20f, center.y + r * 0.80f), pBrush, 2.4f * scale);
        pTarget->DrawLine(D2D1::Point2F(center.x + r * 0.05f, center.y - r * 0.05f), D2D1::Point2F(center.x + r * 0.05f, center.y + r * 0.80f), pBrush, 2.4f * scale);
    } else {
        pBrush->SetColor(D2D1::ColorF(0.22f, 0.28f, 0.38f));
        D2D1_ROUNDED_RECT botSeat = D2D1::RoundedRect(
            D2D1::RectF(center.x - r * 0.55f, center.y - r * 0.05f, center.x + r * 0.15f, center.y + r * 0.45f),
            2.5f * scale, 2.5f * scale
        );
        pTarget->FillRoundedRectangle(botSeat, pBrush);

        pBrush->SetColor(D2D1::ColorF(0.0f, 0.85f, 1.0f, 0.75f));
        pTarget->DrawLine(D2D1::Point2F(center.x + r * 0.22f, center.y + r * 0.15f), D2D1::Point2F(center.x + r * 0.65f, center.y + r * 0.15f), pBrush, 1.8f * scale);
    }
}

void MascotRenderer::DrawMascotFloating(
    ID2D1RenderTarget* pTarget,
    D2D1_POINT_2F center,
    float radius,
    MascotTheme theme,
    AppState state,
    int /*remainingSec*/,
    int /*totalSec*/,
    int animTick,
    D2D1_COLOR_F accentColor,
    float scale,
    bool isDark
) {
    if (!pTarget) return;

    ComPtr<ID2D1SolidColorBrush> pBrush;
    pTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), pBrush.GetAddressOf());
    if (!pBrush) return;

    // 内部大画幅坐姿/站立形象分支绘制 (共享单一画刷并复用，消灭内部重复 COM 分配)
    if (theme == MascotTheme::Capybara) {
        DrawCapybara(pTarget, pBrush.Get(), center, radius, state, animTick, scale, isDark);
    } else if (theme == MascotTheme::PixelCat) {
        DrawPixelCat(pTarget, pBrush.Get(), center, radius, state, animTick, scale, isDark);
    } else if (theme == MascotTheme::CyberBot) {
        DrawCyberBot(pTarget, pBrush.Get(), center, radius, state, animTick, scale, isDark);
    } else {
        // 极简商务基础模式：呈现清晰的大画幅标准人体工学 90° 坐姿 / 站立姿态 (支持主题深浅色高对比)
        DrawMinimalistErgonomic(pTarget, pBrush.Get(), center, radius, state, scale, accentColor, isDark);
    }
}

void MascotRenderer::DrawMascotDockTab(
    ID2D1RenderTarget* pTarget,
    D2D1_RECT_F tabRect,
    MascotTheme theme,
    AppState state,
    int remainingSec,
    int totalSec,
    int animTick,
    D2D1_COLOR_F accentColor,
    bool /*isLeftEdge*/,
    float scale,
    bool isDark
) {
    if (!pTarget) return;

    ComPtr<ID2D1SolidColorBrush> pBrush;
    D2D1_COLOR_F pillBg = isDark 
        ? D2D1::ColorF(0.09f, 0.11f, 0.15f, 0.96f) 
        : D2D1::ColorF(0.96f, 0.97f, 0.99f, 0.96f);
    pTarget->CreateSolidColorBrush(pillBg, pBrush.GetAddressOf());
    if (!pBrush) return;

    // 绘制微型胶囊拉手背景 (Capsule Tab)
    float cornerRadius = 12.0f * scale;
    D2D1_ROUNDED_RECT dockPill = D2D1::RoundedRect(tabRect, cornerRadius, cornerRadius);
    pTarget->FillRoundedRectangle(dockPill, pBrush.Get());

    // 外边框微型流光
    float progress = (totalSec > 0) ? std::clamp(static_cast<float>(remainingSec) / static_cast<float>(totalSec), 0.0f, 1.0f) : 1.0f;
    DrawRoundedRectProgress(pTarget, tabRect, cornerRadius, 1.5f * scale, progress, accentColor);

    // 拉手中心点
    D2D1_POINT_2F center = D2D1::Point2F(
        (tabRect.left + tabRect.right) / 2.0f,
        (tabRect.top + tabRect.bottom) / 2.0f
    );

    float miniRadius = (std::min)(tabRect.right - tabRect.left, tabRect.bottom - tabRect.top) * 0.38f;
    DrawMascotFloating(pTarget, center, miniRadius, theme, state, remainingSec, totalSec, animTick, accentColor, scale, isDark);
}
