#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "FloatingWindow.hpp"
#include "TrayWindow.hpp"
#include "../graphics/D2DContext.hpp"
#include "../graphics/MascotRenderer.hpp"
#include "../core/ConfigManager.hpp"
#include "../core/AppConstants.hpp"
#include "../platform/ThemeManager.hpp"
#include <algorithm>
#include <cwchar>
#include <cmath>

extern StateMachine* g_pStateMachine;

#define IDT_ANIMATION       1001
#define IDT_HOVER_CHECK     1003

bool FloatingWindow::Create(HINSTANCE hInstance) {
    if (m_hwnd) return true;

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = AppConstants::Identity::CLASS_FLOATING;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        AppConstants::Identity::TITLE_FLOATING,
        WS_POPUP,
        0, 0, m_width, m_height,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hwnd) return false;

    RepositionDefault();
    UpdateHoverTimerState();
    return true;
}

void FloatingWindow::Show(bool show) {
    if (!m_hwnd) return;
    if (show) {
        ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
        auto& config = ConfigManager::Instance().GetConfig();
        HWND insertAfter = config.alwaysTopMost ? HWND_TOPMOST : HWND_NOTOPMOST;
        SetWindowPos(m_hwnd, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        Render();
    } else {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void FloatingWindow::Destroy() {
    if (m_hwnd) {
        KillTimer(m_hwnd, IDT_ANIMATION);
        KillTimer(m_hwnd, IDT_HOVER_CHECK);
    }
    if (m_pBrush) {
        m_pBrush.Reset();
    }
    if (m_pDCRenderTarget) {
        m_pDCRenderTarget.Reset();
    }
    if (m_memDC) {
        if (m_hOldBitmap) SelectObject(m_memDC, m_hOldBitmap);
        if (m_hBitmap) DeleteObject(m_hBitmap);
        DeleteDC(m_memDC);
        m_memDC = nullptr;
        m_hBitmap = nullptr;
        m_hOldBitmap = nullptr;
        m_pBits = nullptr;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void FloatingWindow::EnsureMemoryDC(int width, int height) {
    if (width == m_dcWidth && height == m_dcHeight && m_memDC && m_hBitmap) {
        return;
    }

    if (m_memDC) {
        if (m_hOldBitmap) SelectObject(m_memDC, m_hOldBitmap);
        if (m_hBitmap) DeleteObject(m_hBitmap);
        DeleteDC(m_memDC);
    }

    HDC hdcScreen = GetDC(nullptr);
    m_memDC = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-Down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_hBitmap = CreateDIBSection(m_memDC, &bmi, DIB_RGB_COLORS, &m_pBits, nullptr, 0);
    m_hOldBitmap = static_cast<HBITMAP>(SelectObject(m_memDC, m_hBitmap));

    ReleaseDC(nullptr, hdcScreen);

    m_dcWidth = width;
    m_dcHeight = height;
}

void FloatingWindow::RepositionDefault() {
    if (!m_hwnd) return;

    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

    float scale = D2DContext::GetWindowDpiScale(m_hwnd);
    int scaledW = static_cast<int>(m_width * scale);
    int scaledH = static_cast<int>(m_height * scale);

    int posX = workArea.right - scaledW - static_cast<int>(20 * scale);
    int posY = workArea.bottom - scaledH - static_cast<int>(20 * scale);

    auto& config = ConfigManager::Instance().GetConfig();
    HWND insertAfter = config.alwaysTopMost ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(m_hwnd, insertAfter, posX, posY, scaledW, scaledH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    m_dockState = DockState::Floating;
    Render();
}

void FloatingWindow::ClampToWorkArea() {
    if (!m_hwnd || !IsWindow(m_hwnd)) return;

    RECT workArea = { 0 };
    bool gotWorkArea = false;

    HMONITOR hMon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
    if (hMon) {
        MONITORINFO mi = { sizeof(MONITORINFO) };
        if (GetMonitorInfoW(hMon, &mi)) {
            workArea = mi.rcWork;
            gotWorkArea = true;
        }
    }

    if (!gotWorkArea) {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    }

    RECT winRc = { 0 };
    GetWindowRect(m_hwnd, &winRc);
    int w = winRc.right - winRc.left;
    int h = winRc.bottom - winRc.top;
    float scale = D2DContext::GetWindowDpiScale(m_hwnd);
    int tabWidth = static_cast<int>(32.0f * scale);
    int tabHeight = static_cast<int>(26.0f * scale);

    if (m_dockState == DockState::DockedLeft_Collapsed) {
        int targetX = workArea.left - (w - tabWidth);
        int y = std::clamp<int>(winRc.top, workArea.top, (std::max<int>)(workArea.top, workArea.bottom - h));
        SetWindowPos(m_hwnd, nullptr, targetX, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
    } else if (m_dockState == DockState::DockedRight_Collapsed) {
        int targetX = workArea.right - tabWidth;
        int y = std::clamp<int>(winRc.top, workArea.top, (std::max<int>)(workArea.top, workArea.bottom - h));
        SetWindowPos(m_hwnd, nullptr, targetX, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
    } else if (m_dockState == DockState::DockedTop_Collapsed) {
        int x = std::clamp<int>(winRc.left, workArea.left, (std::max<int>)(workArea.left, workArea.right - w));
        int targetY = workArea.top - (h - tabHeight);
        SetWindowPos(m_hwnd, nullptr, x, targetY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
    } else {
        int x = std::clamp<int>(winRc.left, workArea.left, (std::max<int>)(workArea.left, workArea.right - w));
        int y = std::clamp<int>(winRc.top, workArea.top, (std::max<int>)(workArea.top, workArea.bottom - h));
        if (x != winRc.left || y != winRc.top) {
            SetWindowPos(m_hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        }
    }
}

void FloatingWindow::UpdateState(AppState state, int remainingSec, int totalSec) {
    m_state = state;
    m_remainingSeconds = remainingSec;
    m_totalSeconds = totalSec;
    m_animTick++;

    if (m_hwnd && IsWindowVisible(m_hwnd)) {
        Render();
    }
}

void FloatingWindow::OnConfigChanged() {
    if (!m_hwnd) return;
    auto& config = ConfigManager::Instance().GetConfig();
    HWND insertAfter = config.alwaysTopMost ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(m_hwnd, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    CheckEdgeDock(false);
    Render();
}

void FloatingWindow::OnThemeChanged() {
    if (!m_hwnd || !IsWindow(m_hwnd)) return;
    Render();
}

void FloatingWindow::CheckEdgeDock(bool isFinal) {
    if (!ConfigManager::Instance().GetConfig().enableEdgeDock) {
        m_dockState = DockState::Floating;
        return;
    }

    HMONITOR hMon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfoW(hMon, &mi);
    RECT workArea = mi.rcWork;

    RECT winRc;
    GetWindowRect(m_hwnd, &winRc);
    int w = winRc.right - winRc.left;

    float scale = D2DContext::GetWindowDpiScale(m_hwnd);
    int snapThreshold = static_cast<int>(20.0f * scale);   // 20px 屏幕内磁吸常驻阈值
    int pushTolerance = static_cast<int>(2.0f * scale);    // 2px 意图越界容差阈值

    POINT pt;
    GetCursorPos(&pt);

    // 多显示器跨屏探测：如果鼠标已经进入另外一台显示器的工作区，直接按自由悬浮处理，杜绝跨屏误吸附
    HMONITOR hCursorMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    if (hCursorMon && hCursorMon != hMon) {
        m_dockState = DockState::Floating;
        m_outsideTicks = 0;
        UpdateHoverTimerState();
        return;
    }

    // 计算用户无视屏幕物理阻挡时的真实拉力意图坐标 (Intended Coordinates)
    int intendedLeft = pt.x - m_dragCursorOffset.x;
    int intendedRight = intendedLeft + w;
    int intendedTop = pt.y - m_dragCursorOffset.y;

    // 1. 【判定意图 1：故意推撞/挤出屏幕边界 -> 激活折叠收起模式 (Docked*_Expanded)】
    // 条件：窗口实际被拽出屏幕，或者用户的鼠标拉力意图已经穿透了屏幕物理边缘
    bool pushLeft = (winRc.left < workArea.left - pushTolerance) || (intendedLeft < workArea.left - pushTolerance);
    bool pushRight = (winRc.right > workArea.right + pushTolerance) || (intendedRight > workArea.right + pushTolerance);
    bool pushTop = (winRc.top < workArea.top - pushTolerance) || (intendedTop < workArea.top - pushTolerance);

    if (pushLeft) {
        if (isFinal) {
            SetWindowPos(m_hwnd, nullptr, workArea.left, winRc.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        }
        m_dockState = DockState::DockedLeft_Expanded;
        m_outsideTicks = 0;
    } else if (pushRight) {
        if (isFinal) {
            SetWindowPos(m_hwnd, nullptr, workArea.right - w, winRc.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        }
        m_dockState = DockState::DockedRight_Expanded;
        m_outsideTicks = 0;
    } else if (pushTop) {
        if (isFinal) {
            SetWindowPos(m_hwnd, nullptr, winRc.left, workArea.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        }
        m_dockState = DockState::DockedTop_Expanded;
        m_outsideTicks = 0;
    }
    // 2. 【判定意图 2：在屏幕内靠近边缘 (0 ~ 20px) 释放 -> 磁吸对齐常驻，保持展开绝不自动折叠 (Snapped_*)】
    else if (winRc.left <= workArea.left + snapThreshold) {
        if (isFinal) {
            SetWindowPos(m_hwnd, nullptr, workArea.left, winRc.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        }
        m_dockState = DockState::Snapped_Left;
        m_outsideTicks = 0;
    } else if (winRc.right >= workArea.right - snapThreshold) {
        if (isFinal) {
            SetWindowPos(m_hwnd, nullptr, workArea.right - w, winRc.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        }
        m_dockState = DockState::Snapped_Right;
        m_outsideTicks = 0;
    } else if (winRc.top <= workArea.top + snapThreshold) {
        if (isFinal) {
            SetWindowPos(m_hwnd, nullptr, winRc.left, workArea.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        }
        m_dockState = DockState::Snapped_Top;
        m_outsideTicks = 0;
    }
    // 3. 【判定意图 3：自由悬浮 (Floating)】
    else {
        m_dockState = DockState::Floating;
        m_outsideTicks = 0;
    }

    UpdateHoverTimerState();
}

void FloatingWindow::UpdateHoverTimerState() {
    if (!m_hwnd) return;
    bool needHoverTimer = (m_dockState == DockState::DockedLeft_Collapsed ||
                           m_dockState == DockState::DockedRight_Collapsed ||
                           m_dockState == DockState::DockedTop_Collapsed ||
                           m_dockState == DockState::DockedLeft_Expanded ||
                           m_dockState == DockState::DockedRight_Expanded ||
                           m_dockState == DockState::DockedTop_Expanded);
    if (needHoverTimer && !m_isAnimating && ConfigManager::Instance().GetConfig().enableEdgeDock) {
        SetTimer(m_hwnd, IDT_HOVER_CHECK, 80, nullptr);
    } else {
        KillTimer(m_hwnd, IDT_HOVER_CHECK);
    }
}

void FloatingWindow::StartSlideAnimation(int targetPos, bool isHorizontal, DockState finalState) {
    if (!m_hwnd) return;

    RECT winRc;
    GetWindowRect(m_hwnd, &winRc);

    m_isAnimating = true;
    m_animIsHorizontal = isHorizontal;
    m_animTargetPos = targetPos;
    m_animStartPos = isHorizontal ? winRc.left : winRc.top;
    m_animCurrentFrame = 0;
    m_animFinalState = finalState;

    SetTimer(m_hwnd, IDT_ANIMATION, 16, nullptr); // ~60FPS 动画帧
}

void FloatingWindow::OnAnimationTick() {
    if (!m_isAnimating || !m_hwnd) return;

    m_animCurrentFrame++;
    float t = static_cast<float>(m_animCurrentFrame) / static_cast<float>(m_animTotalFrames);

    if (t >= 1.0f) {
        t = 1.0f;
        KillTimer(m_hwnd, IDT_ANIMATION);
        m_isAnimating = false;
        m_dockState = m_animFinalState;
        UpdateHoverTimerState();
    }

    // 三次缓动曲线 (Cubic Ease-Out)
    float ease = 1.0f - powf(1.0f - t, 3.0f);
    int currentPos = m_animStartPos + static_cast<int>((m_animTargetPos - m_animStartPos) * ease);

    RECT winRc;
    GetWindowRect(m_hwnd, &winRc);

    if (m_animIsHorizontal) {
        SetWindowPos(m_hwnd, nullptr, currentPos, winRc.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
    } else {
        SetWindowPos(m_hwnd, nullptr, winRc.left, currentPos, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
    }

    Render();
}

void FloatingWindow::Render() {
    if (!m_hwnd) return;

    float scale = D2DContext::GetWindowDpiScale(m_hwnd);
    int scaledW = static_cast<int>(AppConstants::FloatingWindowDimensions::BASE_WIDTH * scale);
    int scaledH = static_cast<int>(AppConstants::FloatingWindowDimensions::BASE_HEIGHT * scale);

    EnsureMemoryDC(scaledW, scaledH);
    if (!m_memDC || !m_pBits) return;

    auto& d2d = D2DContext::Instance();
    if (!m_pDCRenderTarget) {
        d2d.CreateDCRenderTarget(m_pDCRenderTarget);
    }

    if (!m_pDCRenderTarget) return;

    RECT rc = { 0, 0, scaledW, scaledH };
    m_pDCRenderTarget->BindDC(m_memDC, &rc);

    m_pDCRenderTarget->BeginDraw();

    // 1. 彻底清空为全透明 Alpha=0
    m_pDCRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    // 2. 状态高亮强调色
    D2D1_COLOR_F accentColor;
    if (m_state == AppState::Standing) {
        accentColor = D2D1::ColorF(0.20f, 0.60f, 1.0f, 0.98f); // 站立蓝
    } else if (m_remainingSeconds <= 30 && m_remainingSeconds > 0) {
        accentColor = D2D1::ColorF(0.96f, 0.26f, 0.21f, 0.98f); // 临界红
    } else if (m_state == AppState::Paused) {
        accentColor = D2D1::ColorF(0.98f, 0.72f, 0.15f, 0.95f); // 暂停黄
    } else {
        accentColor = D2D1::ColorF(0.18f, 0.78f, 0.46f, 0.98f); // 坐姿翡翠绿 #2ec273
    }

    auto mascot = ConfigManager::Instance().GetConfig().mascotTheme;
    bool isDark = ThemeManager::Instance().IsEffectiveDark();

    // 3. 判断是否处于贴边折叠形态 (Collapsed Dock Tab)
    if (m_dockState == DockState::DockedLeft_Collapsed) {
        // 左贴边折叠：在右侧 32px 绘制外露拉手
        D2D1_RECT_F tabRect = D2D1::RectF(scaledW - 32.0f * scale, 4.0f * scale, scaledW - 2.0f * scale, scaledH - 4.0f * scale);
        MascotRenderer::Instance().DrawMascotDockTab(m_pDCRenderTarget.Get(), tabRect, mascot, m_state, m_remainingSeconds, m_totalSeconds, m_animTick, accentColor, true, scale, isDark);
    } else if (m_dockState == DockState::DockedRight_Collapsed) {
        // 右贴边折叠：在左侧 32px 绘制外露拉手
        D2D1_RECT_F tabRect = D2D1::RectF(2.0f * scale, 4.0f * scale, 32.0f * scale, scaledH - 4.0f * scale);
        MascotRenderer::Instance().DrawMascotDockTab(m_pDCRenderTarget.Get(), tabRect, mascot, m_state, m_remainingSeconds, m_totalSeconds, m_animTick, accentColor, false, scale, isDark);
    } else {
        // 正常展开形态 (Floating or Docked_Expanded)
        if (!m_pBrush) {
            m_pDCRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), m_pBrush.GetAddressOf());
        }
        ID2D1SolidColorBrush* pBrush = m_pBrush.Get();

        if (pBrush) {
            // 卡片圆角背景 (32-bit 亚像素平滑半透明磨砂质感)
            float cornerRadius = 12.0f * scale;
            D2D1_RECT_F cardBounds = D2D1::RectF(1.5f * scale, 1.5f * scale, scaledW - 1.5f * scale, scaledH - 1.5f * scale);
            D2D1_ROUNDED_RECT cardRect = D2D1::RoundedRect(cardBounds, cornerRadius, cornerRadius);

            if (isDark) {
                pBrush->SetColor(D2D1::ColorF(0.09f, 0.11f, 0.15f, 0.95f));
            } else {
                pBrush->SetColor(D2D1::ColorF(0.96f, 0.97f, 0.99f, 0.96f));
            }
            m_pDCRenderTarget->FillRoundedRectangle(cardRect, pBrush);

            // 外边框周长圆角流光倒计时 (支持用户自定义边框粗细)
            float progress = (m_totalSeconds > 0) ? std::clamp(static_cast<float>(m_remainingSeconds) / static_cast<float>(m_totalSeconds), 0.0f, 1.0f) : 1.0f;
            float strokeW = GetStrokeWidthForBorder(ConfigManager::Instance().GetConfig().borderWidth) * scale;
            MascotRenderer::Instance().DrawRoundedRectProgress(m_pDCRenderTarget.Get(), cardBounds, cornerRadius, strokeW, progress, accentColor);

            // 左侧释放出大画幅坐姿 / 伴侣空间 (精准适配当前深浅色主题)
            D2D1_POINT_2F ringCenter = D2D1::Point2F(26.0f * scale, scaledH / 2.0f);
            float ringRadius = 18.0f * scale;
            MascotRenderer::Instance().DrawMascotFloating(
                m_pDCRenderTarget.Get(), ringCenter, ringRadius,
                mascot, m_state, m_remainingSeconds, m_totalSeconds,
                m_animTick, accentColor, scale, isDark
            );

            // 右侧状态标签 (Top: y=8~24)
            float textLeft = 48.0f * scale;
            float textRight = scaledW - 6.0f * scale;

            std::wstring stateLabel;
            D2D1_COLOR_F stateColor;

            if (m_remainingSeconds <= 30 && m_state == AppState::Working && ConfigManager::Instance().GetConfig().strongReminder) {
                stateLabel = L"即将休息";
                stateColor = isDark ? D2D1::ColorF(0.98f, 0.42f, 0.42f) : D2D1::ColorF(0.88f, 0.18f, 0.18f);
            } else if (m_state == AppState::Standing) {
                stateLabel = L"站立办公";
                stateColor = isDark ? D2D1::ColorF(0.98f, 0.75f, 0.15f) : D2D1::ColorF(0.85f, 0.52f, 0.05f);
            } else if (m_state == AppState::Resting) {
                stateLabel = L"工间放松";
                stateColor = isDark ? D2D1::ColorF(0.22f, 0.80f, 0.98f) : D2D1::ColorF(0.05f, 0.55f, 0.82f);
            } else if (m_state == AppState::Paused) {
                stateLabel = L"计时暂停";
                stateColor = isDark ? D2D1::ColorF(0.65f, 0.70f, 0.78f) : D2D1::ColorF(0.45f, 0.50f, 0.58f);
            } else {
                stateLabel = L"坐姿专注";
                stateColor = isDark ? D2D1::ColorF(0.20f, 0.88f, 0.58f) : D2D1::ColorF(0.06f, 0.65f, 0.35f);
            }

            auto fmtLabel = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 11.5f * scale, DWRITE_FONT_WEIGHT_BOLD);
            if (fmtLabel) {
                pBrush->SetColor(stateColor);
                D2D1_RECT_F labelRect = D2D1::RectF(textLeft, 8.0f * scale, textRight, 24.0f * scale);
                m_pDCRenderTarget->DrawTextW(stateLabel.c_str(), static_cast<UINT32>(stateLabel.length()), fmtLabel.Get(), labelRect, pBrush);
            }

            // 右侧核心倒计时 mm:ss (Middle: y=24~62)
            int minutes = m_remainingSeconds / 60;
            int seconds = m_remainingSeconds % 60;
            wchar_t timeBuf[16];
            swprintf_s(timeBuf, L"%02d:%02d", minutes, seconds);

            auto fmtTime = d2d.GetCachedTextFormat(L"Segoe UI", 24.0f * scale, DWRITE_FONT_WEIGHT_BOLD);
            if (fmtTime) {
                pBrush->SetColor(isDark ? D2D1::ColorF(0.96f, 0.98f, 1.0f) : D2D1::ColorF(0.09f, 0.13f, 0.20f));
                D2D1_RECT_F timeRect = D2D1::RectF(textLeft - 1.0f * scale, 24.0f * scale, textRight, 62.0f * scale);
                m_pDCRenderTarget->DrawTextW(timeBuf, static_cast<UINT32>(wcslen(timeBuf)), fmtTime.Get(), timeRect, pBrush);
            }
        }
    }

    HRESULT hr = m_pDCRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        m_pBrush.Reset();
        m_pDCRenderTarget.Reset();
    }

    if (SUCCEEDED(hr)) {
        // 交付 DWM 完成全硬件级透明合成
        HDC hdcScreen = GetDC(nullptr);
        POINT ptSrc = { 0, 0 };
        SIZE sizeDst = { scaledW, scaledH };
        RECT winRc;
        GetWindowRect(m_hwnd, &winRc);
        POINT ptDst = { winRc.left, winRc.top };
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        UpdateLayeredWindow(m_hwnd, hdcScreen, &ptDst, &sizeDst, m_memDC, &ptSrc, 0, &bf, ULW_ALPHA);
        ReleaseDC(nullptr, hdcScreen);
    }
}

LRESULT CALLBACK FloatingWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    try {
        auto& self = FloatingWindow::Instance();

        switch (msg) {
        case WM_DISPLAYCHANGE: {
            self.ClampToWorkArea();
            return 0;
        }

        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED: {
            ThemeManager::Instance().Refresh();
            self.ClampToWorkArea();
            self.OnThemeChanged();
            return 0;
        }

        case WM_MOUSEMOVE: {
            self.m_outsideTicks = 0;
            if (!self.m_isAnimating && (
                self.m_dockState == DockState::DockedLeft_Collapsed ||
                self.m_dockState == DockState::DockedRight_Collapsed ||
                self.m_dockState == DockState::DockedTop_Collapsed)) {
                HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi = { sizeof(MONITORINFO) };
                GetMonitorInfoW(hMon, &mi);
                RECT winRc;
                GetWindowRect(hwnd, &winRc);
                int w = winRc.right - winRc.left;

                if (self.m_dockState == DockState::DockedLeft_Collapsed) {
                    self.StartSlideAnimation(mi.rcWork.left, true, DockState::DockedLeft_Expanded);
                } else if (self.m_dockState == DockState::DockedRight_Collapsed) {
                    self.StartSlideAnimation(mi.rcWork.right - w, true, DockState::DockedRight_Expanded);
                } else if (self.m_dockState == DockState::DockedTop_Collapsed) {
                    self.StartSlideAnimation(mi.rcWork.top, false, DockState::DockedTop_Expanded);
                }
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            return 0;
        }

        case WM_TIMER: {
            if (wParam == IDT_ANIMATION) {
                self.OnAnimationTick();
                return 0;
            }
            if (wParam == IDT_HOVER_CHECK) {
                if (self.m_isAnimating || !self.m_hwnd || !IsWindowVisible(self.m_hwnd)) return 0;
                if (!ConfigManager::Instance().GetConfig().enableEdgeDock) return 0;

                POINT pt;
                GetCursorPos(&pt);
                RECT winRc;
                GetWindowRect(self.m_hwnd, &winRc);

                float scale = D2DContext::GetWindowDpiScale(self.m_hwnd);

                // 折叠形态下的热区优化：在贴边外露拉手区域增加贴近屏幕物理边缘的容差判定
                bool isCollapsed = (self.m_dockState == DockState::DockedLeft_Collapsed ||
                                    self.m_dockState == DockState::DockedRight_Collapsed ||
                                    self.m_dockState == DockState::DockedTop_Collapsed);
                RECT hitRc = winRc;
                if (isCollapsed) {
                    int edgePad = static_cast<int>(8.0f * scale);
                    if (self.m_dockState == DockState::DockedLeft_Collapsed) hitRc.left -= edgePad;
                    else if (self.m_dockState == DockState::DockedRight_Collapsed) hitRc.right += edgePad;
                    else if (self.m_dockState == DockState::DockedTop_Collapsed) hitRc.top -= edgePad;
                }
                bool isInside = PtInRect(&hitRc, pt);

                HMONITOR hMon = MonitorFromWindow(self.m_hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi = { sizeof(MONITORINFO) };
                GetMonitorInfoW(hMon, &mi);
                RECT workArea = mi.rcWork;
                int w = winRc.right - winRc.left;
                int h = winRc.bottom - winRc.top;

                if (isInside) {
                    self.m_outsideTicks = 0;
                    // 如果处于折叠形态，鼠标碰触立即平滑展开！
                    if (self.m_dockState == DockState::DockedLeft_Collapsed) {
                        self.StartSlideAnimation(workArea.left, true, DockState::DockedLeft_Expanded);
                    } else if (self.m_dockState == DockState::DockedRight_Collapsed) {
                        self.StartSlideAnimation(workArea.right - w, true, DockState::DockedRight_Expanded);
                    } else if (self.m_dockState == DockState::DockedTop_Collapsed) {
                        self.StartSlideAnimation(workArea.top, false, DockState::DockedTop_Expanded);
                    }
                } else {
                    // 鼠标在窗体外，且处于展开停靠状态 -> 连续检测 5 次 (约 400ms) 后自动平滑收起！
                    if (self.m_dockState == DockState::DockedLeft_Expanded ||
                        self.m_dockState == DockState::DockedRight_Expanded ||
                        self.m_dockState == DockState::DockedTop_Expanded) {
                        self.m_outsideTicks++;
                        if (self.m_outsideTicks >= 5) { // 5 * 80ms = 400ms
                            self.m_outsideTicks = 0;
                            int tabWidth = static_cast<int>(32.0f * scale);
                            int tabHeight = static_cast<int>(26.0f * scale);

                            if (self.m_dockState == DockState::DockedLeft_Expanded) {
                                int targetX = workArea.left - (w - tabWidth);
                                self.StartSlideAnimation(targetX, true, DockState::DockedLeft_Collapsed);
                            } else if (self.m_dockState == DockState::DockedRight_Expanded) {
                                int targetX = workArea.right - tabWidth;
                                self.StartSlideAnimation(targetX, true, DockState::DockedRight_Collapsed);
                            } else if (self.m_dockState == DockState::DockedTop_Expanded) {
                                int targetY = workArea.top - (h - tabHeight);
                                self.StartSlideAnimation(targetY, false, DockState::DockedTop_Collapsed);
                            }
                        }
                    }
                }
                return 0;
            }
            break;
        }

        case WM_LBUTTONDOWN: {
            POINT pt;
            GetCursorPos(&pt);
            RECT winRc;
            GetWindowRect(hwnd, &winRc);
            self.m_dragCursorOffset.x = pt.x - winRc.left;
            self.m_dragCursorOffset.y = pt.y - winRc.top;

            // 原生标准 DWM 硬件级移动（系统进入模态拖拽循环，退出时会自动发送 WM_EXITSIZEMOVE）
            ReleaseCapture();
            SendMessageW(hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
            return 0;
        }

        case WM_EXITSIZEMOVE: {
            self.CheckEdgeDock(true);
            return 0;
        }

        case WM_DPICHANGED: {
            auto* lprc = reinterpret_cast<RECT*>(lParam);
            bool isCollapsed = (self.m_dockState == DockState::DockedLeft_Collapsed ||
                                self.m_dockState == DockState::DockedRight_Collapsed ||
                                self.m_dockState == DockState::DockedTop_Collapsed);
            if (isCollapsed) {
                // 折叠形态下按当前新 DPI 与工作区重新计算贴边拉手坐标，杜绝被系统展开矩形拉扯撕裂
                self.ClampToWorkArea();
            } else {
                if (lprc) {
                    SetWindowPos(hwnd, nullptr, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top, SWP_NOZORDER | SWP_NOACTIVATE);
                }
                self.ClampToWorkArea();
            }
            self.Render();
            return 0;
        }

        case WM_RBUTTONUP: {
            // 右键快捷菜单：与托盘使用 100% 相同自绘深浅色现代菜单与命令体系
            POINT pt;
            GetCursorPos(&pt);
            TrayWindow::Instance().ShowContextMenu(&pt);
            return 0;
        }

        case WM_DESTROY: {
            self.m_hwnd = nullptr;
            return 0;
        }
        }
    } catch (...) {
        // C++ 异常屏障
#ifdef _DEBUG
        OutputDebugStringW(L"[SitStandReminder] Exception caught in FloatingWindow::WndProc\n");
#endif
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
