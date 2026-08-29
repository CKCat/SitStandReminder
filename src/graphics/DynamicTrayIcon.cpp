#include "DynamicTrayIcon.hpp"
#include "D2DContext.hpp"
#include "../platform/ThemeManager.hpp"
#include <cmath>
#include <algorithm>

DynamicTrayIcon::DynamicTrayIcon() {
    D2DContext::Instance().Initialize();
    D2DContext::Instance().CreateDCRenderTarget(m_pDCRenderTarget);
}

DynamicTrayIcon::~DynamicTrayIcon() {
    if (m_pBrush) {
        m_pBrush.Reset();
    }
    if (m_pDCRenderTarget) {
        m_pDCRenderTarget.Reset();
    }
    if (m_memDC) {
        if (m_hOldBitmap) SelectObject(m_memDC, m_hOldBitmap);
        DeleteDC(m_memDC);
        m_memDC = nullptr;
    }
    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = nullptr;
    }
}

ID2D1SolidColorBrush* DynamicTrayIcon::GetSolidBrush(const D2D1_COLOR_F& color) {
    if (!m_pBrush && m_pDCRenderTarget) {
        m_pDCRenderTarget->CreateSolidColorBrush(color, m_pBrush.GetAddressOf());
    }
    if (m_pBrush) {
        m_pBrush->SetColor(color);
        return m_pBrush.Get();
    }
    return nullptr;
}

void DynamicTrayIcon::EnsureMemoryDC(int width, int height) {
    if (m_memDC && m_hBitmap && m_dcWidth == width && m_dcHeight == height) {
        return;
    }

    if (m_memDC) {
        if (m_hOldBitmap) SelectObject(m_memDC, m_hOldBitmap);
        DeleteDC(m_memDC);
        m_memDC = nullptr;
    }
    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = nullptr;
    }

    HDC hdcScreen = GetDC(nullptr);
    m_memDC = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height; // Top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    m_hBitmap = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &m_pBits, nullptr, 0);
    m_hOldBitmap = static_cast<HBITMAP>(SelectObject(m_memDC, m_hBitmap));
    ReleaseDC(nullptr, hdcScreen);

    m_dcWidth = width;
    m_dcHeight = height;
}

HICON DynamicTrayIcon::RenderToHIcon(int size, const std::function<void(ID2D1DCRenderTarget* pRT, float scale, int size)>& drawCallback) {
    if (size <= 0) size = 16;
    EnsureMemoryDC(size, size);
    if (!m_memDC || !m_hBitmap) return nullptr;

    if (!m_pDCRenderTarget) {
        D2DContext::Instance().CreateDCRenderTarget(m_pDCRenderTarget);
    }
    if (!m_pDCRenderTarget) return nullptr;

    RECT rc = { 0, 0, size, size };
    m_pDCRenderTarget->BindDC(m_memDC, &rc);

    m_pDCRenderTarget->BeginDraw();
    m_pDCRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0.0f));

    float scale = size / 16.0f;
    drawCallback(m_pDCRenderTarget.Get(), scale, size);

    HRESULT hr = m_pDCRenderTarget->EndDraw();
    if (FAILED(hr)) {
        m_pBrush.Reset();
        m_pDCRenderTarget.Reset();
        return nullptr;
    }

    HBITMAP hMask = CreateBitmap(size, size, 1, 1, nullptr);
    if (!hMask) return nullptr;

    ICONINFO ii = { 0 };
    ii.fIcon = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmMask = hMask;
    ii.hbmColor = m_hBitmap;

    HICON hIcon = CreateIconIndirect(&ii);
    DeleteObject(hMask);

    return hIcon;
}

HICON DynamicTrayIcon::CreateCountdownIcon(int remainingSec, int totalSec, AppState state, bool isDark) {
    int iconSize = GetSystemMetrics(SM_CXSMICON);
    if (iconSize <= 0) iconSize = 16;

    return RenderToHIcon(iconSize, [remainingSec, totalSec, state, isDark](ID2D1DCRenderTarget* pRT, float scale, int size) {
        float cx = size * 0.5f;
        float cy = size * 0.5f;
        float strokeW = (std::max)(1.4f, 1.8f * scale);
        float radius = (size - strokeW) * 0.48f;

        // 1. 进度环底轨
        auto* pBrush = DynamicTrayIcon::Instance().GetSolidBrush(
            isDark ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.18f) : D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.15f)
        );
        if (!pBrush) return;

        pRT->DrawEllipse(
            D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius),
            pBrush,
            strokeW
        );

        // 2. 动态进度扇弧
        float progress = 1.0f;
        if (totalSec > 0) {
            progress = (std::max)(0.0f, (std::min)(1.0f, static_cast<float>(remainingSec) / static_cast<float>(totalSec)));
        }

        if (progress > 0.005f) {
            D2D1_COLOR_F progColor;
            if (state == AppState::Resting) {
                progColor = D2D1::ColorF(0.96f, 0.25f, 0.35f, 0.95f); // 珊瑚粉红
            } else if (remainingSec <= 300) {
                progColor = D2D1::ColorF(0.96f, 0.62f, 0.04f, 0.95f); // 警示琥珀黄
            } else if (state == AppState::Standing) {
                progColor = D2D1::ColorF(0.05f, 0.65f, 0.91f, 0.95f); // 站立天青蓝
            } else {
                progColor = D2D1::ColorF(0.06f, 0.72f, 0.50f, 0.95f); // 专注翡翠绿
            }

            pBrush->SetColor(progColor);

            if (progress >= 0.999f) {
                pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), pBrush, strokeW);
            } else {
                float startAngle = -90.0f;
                float sweepAngle = progress * 360.0f;
                float endAngle = startAngle + sweepAngle;

                float startRad = startAngle * AppConstants::Math::PI / 180.0f;
                float endRad = endAngle * AppConstants::Math::PI / 180.0f;

                D2D1_POINT_2F p0 = D2D1::Point2F(cx + radius * cosf(startRad), cy + radius * sinf(startRad));
                D2D1_POINT_2F p1 = D2D1::Point2F(cx + radius * cosf(endRad), cy + radius * sinf(endRad));

                ComPtr<ID2D1PathGeometry> pArcGeo;
                D2DContext::Instance().GetD2DFactory()->CreatePathGeometry(&pArcGeo);
                if (pArcGeo) {
                    ComPtr<ID2D1GeometrySink> pSink;
                    pArcGeo->Open(&pSink);
                    pSink->BeginFigure(p0, D2D1_FIGURE_BEGIN_HOLLOW);
                    pSink->AddArc(D2D1::ArcSegment(
                        p1,
                        D2D1::SizeF(radius, radius),
                        0.0f,
                        D2D1_SWEEP_DIRECTION_CLOCKWISE,
                        (sweepAngle > 180.0f) ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL
                    ));
                    pSink->EndFigure(D2D1_FIGURE_END_OPEN);
                    pSink->Close();

                    pRT->DrawGeometry(pArcGeo.Get(), pBrush, strokeW);
                }
            }
        }

        // 3. 居中分钟数字
        int displayVal = (remainingSec + 59) / 60;
        if (displayVal <= 0 && remainingSec > 0) displayVal = 1;
        std::wstring str = std::to_wstring(displayVal);

        float fontSize = size * 0.52f;
        auto pFormat = D2DContext::Instance().GetCachedTextFormat(
            L"Segoe UI",
            fontSize,
            DWRITE_FONT_WEIGHT_BOLD,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_TEXT_ALIGNMENT_CENTER,
            DWRITE_PARAGRAPH_ALIGNMENT_CENTER
        );

        pBrush->SetColor(isDark ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f) : D2D1::ColorF(0.08f, 0.12f, 0.18f, 0.95f));

        if (pFormat) {
            D2D1_RECT_F textRc = D2D1::RectF(0, 0, static_cast<float>(size), static_cast<float>(size));
            pRT->DrawTextW(str.c_str(), static_cast<UINT32>(str.length()), pFormat.Get(), textRc, pBrush);
        }
    });
}

HICON DynamicTrayIcon::CreateRunCatIcon(int frameIndex, AppState state, bool isDark, float /*cpuUsage*/) {
    int iconSize = GetSystemMetrics(SM_CXSMICON);
    if (iconSize <= 0) iconSize = 16;

    return RenderToHIcon(iconSize, [frameIndex, state, isDark](ID2D1DCRenderTarget* pRT, float scale, int size) {
        float cx = size * 0.48f;
        float cy = size * 0.52f;

        auto* pBrush = DynamicTrayIcon::Instance().GetSolidBrush(
            isDark ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f) : D2D1::ColorF(0.12f, 0.15f, 0.22f, 0.95f)
        );
        if (!pBrush) return;

        D2D1_COLOR_F catColor = isDark ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f) : D2D1::ColorF(0.12f, 0.15f, 0.22f, 0.95f);
        D2D1_COLOR_F pinkColor = D2D1::ColorF(1.0f, 0.62f, 0.70f, 0.95f);
        D2D1_COLOR_F eyeColor = isDark ? D2D1::ColorF(0.1f, 0.1f, 0.12f, 0.95f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f);

        if (state == AppState::Resting) {
            // 休息打盹模式：卷缩睡眠猫猫 + 呼呼泡泡
            float sleepY = cy + 1.0f * scale;
            pBrush->SetColor(catColor);
            pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, sleepY), 4.5f * scale, 3.2f * scale), pBrush);

            // 猫头
            float headX = cx - 2.8f * scale;
            float headY = sleepY - 1.2f * scale;
            pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX, headY), 2.4f * scale, 2.4f * scale), pBrush);

            // 闭合眼睛 ^ ^
            pBrush->SetColor(eyeColor);
            pRT->DrawLine(D2D1::Point2F(headX - 1.2f * scale, headY), D2D1::Point2F(headX - 0.4f * scale, headY - 0.5f * scale), pBrush, 0.8f * scale);
            pRT->DrawLine(D2D1::Point2F(headX - 0.4f * scale, headY - 0.5f * scale), D2D1::Point2F(headX + 0.4f * scale, headY), pBrush, 0.8f * scale);

            // 耳朵
            pBrush->SetColor(catColor);
            D2D1_POINT_2F e1[] = { D2D1::Point2F(headX - 1.8f * scale, headY - 1.2f * scale), D2D1::Point2F(headX - 1.2f * scale, headY - 3.0f * scale), D2D1::Point2F(headX - 0.4f * scale, headY - 1.5f * scale) };
            ComPtr<ID2D1PathGeometry> pEar;
            D2DContext::Instance().GetD2DFactory()->CreatePathGeometry(&pEar);
            if (pEar) {
                ComPtr<ID2D1GeometrySink> pSink;
                pEar->Open(&pSink);
                pSink->BeginFigure(e1[0], D2D1_FIGURE_BEGIN_FILLED);
                pSink->AddLines(e1, 3);
                pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                pSink->Close();
                pRT->FillGeometry(pEar.Get(), pBrush);
            }

            // 打盹小 'z' 泡泡
            int zPhase = frameIndex % 3;
            float zX = cx + 2.0f * scale + zPhase * 0.8f * scale;
            float zY = cy - 2.5f * scale - zPhase * 1.2f * scale;
            auto pZFormat = D2DContext::Instance().GetCachedTextFormat(L"Arial", 4.0f * scale, DWRITE_FONT_WEIGHT_BOLD);
            if (pZFormat) {
                D2D1_RECT_F zRc = D2D1::RectF(zX, zY, zX + 6 * scale, zY + 6 * scale);
                pRT->DrawTextW(L"z", 1, pZFormat.Get(), zRc, pBrush);
            }
            return;
        }

        // 奔跑模式 (5 帧经典流畅 RunCycle)
        int f = frameIndex % 5;
        float legStroke = 1.2f * scale;

        // 帧状态参数化
        float bodyAngle = 0.0f;
        float bodyY = cy;
        float fLeg1X = 0, fLeg1Y = 0, fLeg2X = 0, fLeg2Y = 0;
        float bLeg1X = 0, bLeg1Y = 0, bLeg2X = 0, bLeg2Y = 0;
        float tailY = cy - 1.5f * scale;

        switch (f) {
            case 0: // 舒展跨步
                bodyAngle = -8.0f; bodyY = cy - 0.8f * scale;
                fLeg1X = 3.5f; fLeg1Y = 3.8f; fLeg2X = 1.8f; fLeg2Y = 2.8f;
                bLeg1X = -4.2f; bLeg1Y = 3.6f; bLeg2X = -2.8f; bLeg2Y = 2.4f;
                tailY = cy - 3.5f * scale;
                break;
            case 1: // 前爪着地
                bodyAngle = 0.0f; bodyY = cy;
                fLeg1X = 2.8f; fLeg1Y = 3.8f; fLeg2X = 3.4f; fLeg2Y = 3.6f;
                bLeg1X = -3.2f; bLeg1Y = 2.0f; bLeg2X = -1.5f; bLeg2Y = 2.8f;
                tailY = cy - 2.0f * scale;
                break;
            case 2: // 身体卷缩蓄力
                bodyAngle = 8.0f; bodyY = cy + 0.6f * scale;
                fLeg1X = 0.8f; fLeg1Y = 2.8f; fLeg2X = 1.4f; fLeg2Y = 2.5f;
                bLeg1X = -1.2f; bLeg1Y = 3.0f; bLeg2X = -0.5f; bLeg2Y = 2.6f;
                tailY = cy - 1.2f * scale;
                break;
            case 3: // 后腿蹬地起跳
                bodyAngle = -12.0f; bodyY = cy - 0.4f * scale;
                fLeg1X = 3.2f; fLeg1Y = 1.8f; fLeg2X = 2.2f; fLeg2Y = 1.4f;
                bLeg1X = -4.5f; bLeg1Y = 3.8f; bLeg2X = -3.5f; bLeg2Y = 3.6f;
                tailY = cy - 2.8f * scale;
                break;
            case 4: // 腾空飞跃
                bodyAngle = -6.0f; bodyY = cy - 1.4f * scale;
                fLeg1X = 4.2f; fLeg1Y = 2.4f; fLeg2X = 2.8f; fLeg2Y = 2.0f;
                bLeg1X = -3.8f; bLeg1Y = 2.4f; bLeg2X = -2.6f; bLeg2Y = 1.8f;
                tailY = cy - 3.8f * scale;
                break;
        }

        // 绘制后腿
        pBrush->SetColor(catColor);
        pRT->DrawLine(D2D1::Point2F(cx - 2.5f * scale, bodyY), D2D1::Point2F(cx + bLeg1X * scale, cy + bLeg1Y * scale), pBrush, legStroke);
        pRT->DrawLine(D2D1::Point2F(cx - 1.8f * scale, bodyY), D2D1::Point2F(cx + bLeg2X * scale, cy + bLeg2Y * scale), pBrush, legStroke);

        // 绘制猫身 (椭圆身体)
        pRT->SetTransform(D2D1::Matrix3x2F::Rotation(bodyAngle, D2D1::Point2F(cx, bodyY)));
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, bodyY), 4.2f * scale, 2.6f * scale), pBrush);
        pRT->SetTransform(D2D1::Matrix3x2F::Identity());

        // 绘制前爪
        pRT->DrawLine(D2D1::Point2F(cx + 2.0f * scale, bodyY), D2D1::Point2F(cx + fLeg1X * scale, cy + fLeg1Y * scale), pBrush, legStroke);
        pRT->DrawLine(D2D1::Point2F(cx + 1.2f * scale, bodyY), D2D1::Point2F(cx + fLeg2X * scale, cy + fLeg2Y * scale), pBrush, legStroke);

        // 绘制猫头
        float headX = cx + 3.8f * scale;
        float headY = bodyY - 1.6f * scale;
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX, headY), 2.2f * scale, 2.2f * scale), pBrush);

        // 眼睛
        pBrush->SetColor(eyeColor);
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX + 0.8f * scale, headY - 0.2f * scale), 0.5f * scale, 0.5f * scale), pBrush);

        // 耳朵
        pBrush->SetColor(catColor);
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX - 0.8f * scale, headY - 2.0f * scale), 0.8f * scale, 1.2f * scale), pBrush);
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX + 0.6f * scale, headY - 2.0f * scale), 0.8f * scale, 1.2f * scale), pBrush);
        pBrush->SetColor(pinkColor);
        pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX + 0.6f * scale, headY - 2.0f * scale), 0.4f * scale, 0.7f * scale), pBrush);

        // 尾巴 (上扬弯曲曲线)
        pBrush->SetColor(catColor);
        pRT->DrawLine(D2D1::Point2F(cx - 3.8f * scale, bodyY), D2D1::Point2F(cx - 5.5f * scale, tailY), pBrush, 1.0f * scale);
    });
}

float DynamicTrayIcon::GetCpuUsage() {
    static FILETIME prevIdleTime = { 0, 0 };
    static FILETIME prevKernelTime = { 0, 0 };
    static FILETIME prevUserTime = { 0, 0 };
    static bool firstCall = true;

    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) return 0.0f;

    if (firstCall) {
        prevIdleTime = idleTime;
        prevKernelTime = kernelTime;
        prevUserTime = userTime;
        firstCall = false;
        return 0.0f;
    }

    auto FT2ULL = [](const FILETIME& ft) -> ULONGLONG {
        return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    };

    ULONGLONG idle = FT2ULL(idleTime) - FT2ULL(prevIdleTime);
    ULONGLONG kernel = FT2ULL(kernelTime) - FT2ULL(prevKernelTime);
    ULONGLONG user = FT2ULL(userTime) - FT2ULL(prevUserTime);

    prevIdleTime = idleTime;
    prevKernelTime = kernelTime;
    prevUserTime = userTime;

    ULONGLONG total = kernel + user;
    if (total == 0) return 0.0f;

    double percent = (total - idle) * 100.0 / total;
    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;
    return static_cast<float>(percent);
}

