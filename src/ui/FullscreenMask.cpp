#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "FullscreenMask.hpp"
#include "../graphics/D2DContext.hpp"
#include "../platform/KeyboardHook.hpp"
#include "../core/ConfigManager.hpp"
#include "../core/AppConstants.hpp"
#include <algorithm>
#include <cwchar>

extern StateMachine* g_pStateMachine;

bool FullscreenMask::Initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = AppConstants::Identity::CLASS_MASK;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    return RegisterClassExW(&wc) != 0;
}

HWND FullscreenMask::GetPrimaryHwnd() const {
    for (const auto& pInfo : m_monitors) {
        if (pInfo && pInfo->isPrimary) return pInfo->hwnd;
    }
    return nullptr;
}

BOOL CALLBACK FullscreenMask::MonitorEnumProc(HMONITOR hMon, HDC, LPRECT, LPARAM pData) {
    auto* self = reinterpret_cast<FullscreenMask*>(pData);
    
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (GetMonitorInfoW(hMon, &mi)) {
        auto info = std::make_unique<MonitorWindowInfo>();
        info->rect = mi.rcMonitor;
        info->isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
        self->m_monitors.push_back(std::move(info));
    }
    return TRUE;
}

void FullscreenMask::CreateWindows() {
    CloseAll();

    m_monitors.clear();
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(this));

    HWND primaryHwnd = nullptr;
    bool allowMultiScreen = ConfigManager::Instance().GetConfig().multiScreen;

    for (auto& pInfo : m_monitors) {
        if (!allowMultiScreen && !pInfo->isPrimary) {
            continue;
        }

        int w = pInfo->rect.right - pInfo->rect.left;
        int h = pInfo->rect.bottom - pInfo->rect.top;

        pInfo->hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            AppConstants::Identity::CLASS_MASK,
            AppConstants::Identity::TITLE_MASK,
            WS_POPUP | WS_VISIBLE,
            pInfo->rect.left, pInfo->rect.top, w, h,
            nullptr, nullptr, m_hInstance, this
        );

        if (pInfo->hwnd) {
            SetWindowLongPtrW(pInfo->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pInfo.get()));
            D2DContext::Instance().CreateHwndRenderTarget(pInfo->hwnd, pInfo->pRenderTarget, w, h);
            if (pInfo->pRenderTarget) {
                pInfo->pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), pInfo->pBrush.GetAddressOf());
            }
            if (pInfo->isPrimary) {
                primaryHwnd = pInfo->hwnd;
            }
        }
    }

    if (primaryHwnd) {
        SetForegroundWindow(primaryHwnd);
        SetFocus(primaryHwnd);
        m_lastTickCount = GetTickCount64();
        m_timerId = SetTimer(primaryHwnd, 1001, 16, nullptr);
    }

    if (ConfigManager::Instance().GetConfig().blockInput) {
        KeyboardHook::Instance().Install(primaryHwnd);
        KeyboardHook::Instance().SetOnEscapePressed([this]() {
            OnEscape();
        });
    }
}

void FullscreenMask::CloseAll() {
    if (m_timerId && !m_monitors.empty()) {
        for (auto& pInfo : m_monitors) {
            if (pInfo && pInfo->hwnd && pInfo->isPrimary) {
                KillTimer(pInfo->hwnd, m_timerId);
                break;
            }
        }
        m_timerId = 0;
    }

    KeyboardHook::Instance().Uninstall();

    for (auto& pInfo : m_monitors) {
        if (pInfo) {
            pInfo->pBrush.Reset();
            pInfo->pRenderTarget.Reset();
            if (pInfo->hwnd) {
                DestroyWindow(pInfo->hwnd);
                pInfo->hwnd = nullptr;
            }
        }
    }
    m_monitors.clear();
}

void FullscreenMask::Show(bool show) {
    m_isVisible = show;
    if (show) {
        m_neckRenderer.Reset();
        m_eyeRenderer.Reset();
        auto& config = ConfigManager::Instance().GetConfig();

        if (config.exerciseMode == ExerciseMode::Comprehensive) {
            float neckSec = (std::max)(30.0f, static_cast<float>(config.restSeconds) / 2.0f);
            float eyeSec = (std::max)(30.0f, static_cast<float>(config.restSeconds) - neckSec);
            m_neckRenderer.SetTotalDuration(neckSec);
            m_eyeRenderer.SetTotalDuration(eyeSec);
        } else if (config.exerciseMode == ExerciseMode::NeckOnly) {
            m_neckRenderer.SetTotalDuration(static_cast<float>(config.restSeconds));
        } else if (config.exerciseMode == ExerciseMode::EyeOnly) {
            m_eyeRenderer.SetTotalDuration(static_cast<float>(config.restSeconds));
        }

        m_currentStage = 0;
        CreateWindows();
    } else {
        CloseAll();
    }
}

void FullscreenMask::UpdateDisplay(int remainingSec, int totalSec, int stageIndex, const std::wstring& stageName) {
    if (stageIndex != m_currentStage) {
        m_currentStage = stageIndex;
        if (m_currentStage == 1) {
            // 切入护眼操阶段时，重置护眼操动画时间轴从头播放
            m_eyeRenderer.Reset();
        } else if (m_currentStage == 0) {
            m_neckRenderer.Reset();
        }
    }
    m_remainingSeconds = remainingSec;
    m_totalSeconds = totalSec;
    m_currentStageName = stageName;
}

void FullscreenMask::OnEscape() {
    if (g_pStateMachine) {
        g_pStateMachine->ExitRestEarly();
    }
}

void FullscreenMask::OnTimer() {
    ULONGLONG now = GetTickCount64();
    float dt = (now - m_lastTickCount) / 1000.0f;
    m_lastTickCount = now;

    if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;

    // 仅驱动当前正在激活的阶段渲染器，彻底杜绝后台提前空转透支时间轴
    auto exerciseMode = ConfigManager::Instance().GetConfig().exerciseMode;
    if (exerciseMode == ExerciseMode::Comprehensive) {
        if (m_currentStage == 0) {
            m_neckRenderer.Update(dt);
        } else {
            m_eyeRenderer.Update(dt);
        }
    } else if (exerciseMode == ExerciseMode::NeckOnly) {
        m_neckRenderer.Update(dt);
    } else if (exerciseMode == ExerciseMode::EyeOnly) {
        m_eyeRenderer.Update(dt);
    }

    for (auto& pInfo : m_monitors) {
        if (pInfo && pInfo->hwnd && IsWindowVisible(pInfo->hwnd)) {
            InvalidateRect(pInfo->hwnd, nullptr, FALSE);
        }
    }
}

void FullscreenMask::OnPaint(MonitorWindowInfo& info) {
    if (!info.hwnd) return;

    // 自愈机制: 若设备丢失导致 RenderTarget 为空，即时动态重建
    if (!info.pRenderTarget) {
        RECT rc;
        GetClientRect(info.hwnd, &rc);
        UINT w = static_cast<UINT>(rc.right - rc.left);
        UINT h = static_cast<UINT>(rc.bottom - rc.top);
        if (!D2DContext::Instance().CreateHwndRenderTarget(info.hwnd, info.pRenderTarget, w, h)) {
            return;
        }
        info.pBrush.Reset();
    }

    PAINTSTRUCT ps;
    BeginPaint(info.hwnd, &ps);

    info.pRenderTarget->BeginDraw();
    info.pRenderTarget->Clear(D2D1::ColorF(0.03f, 0.05f, 0.08f, 1.0f));

    D2D1_SIZE_F size = info.pRenderTarget->GetSize();
    float scale = D2DContext::GetWindowDpiScale(info.hwnd);

    if (!info.pBrush) {
        info.pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), info.pBrush.GetAddressOf());
    }
    ID2D1SolidColorBrush* pBrush = info.pBrush.Get();

    auto& d2d = D2DContext::Instance();

    if (info.isPrimary) {
        // 主屏幕：顶部状态栏、工间操大视窗、底部快捷退出说明
        float topBarH = 64.0f * scale;

        // 1. 顶部当前阶段文案
        auto fmtStage = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 16.0f * scale, DWRITE_FONT_WEIGHT_BOLD);
        if (fmtStage && pBrush) {
            pBrush->SetColor(D2D1::ColorF(0.60f, 0.95f, 0.75f));
            D2D1_RECT_F stageRect = D2D1::RectF(32.0f * scale, 18.0f * scale, size.width - 240.0f * scale, topBarH);
            info.pRenderTarget->DrawTextW(m_currentStageName.c_str(), static_cast<UINT32>(m_currentStageName.length()), fmtStage.Get(), stageRect, pBrush);
        }

        // 2. 右上角倒计时与退出提示
        int minutes = m_remainingSeconds / 60;
        int seconds = m_remainingSeconds % 60;
        wchar_t timeBuf[32];
        swprintf_s(timeBuf, L"⏱️ 剩余 %02d:%02d", minutes, seconds);

        auto fmtTime = d2d.GetCachedTextFormat(L"Segoe UI", 16.0f * scale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
        if (fmtTime && pBrush) {
            pBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f));
            D2D1_RECT_F timeRect = D2D1::RectF(size.width - 240.0f * scale, 18.0f * scale, size.width - 32.0f * scale, topBarH);
            info.pRenderTarget->DrawTextW(timeBuf, static_cast<UINT32>(wcslen(timeBuf)), fmtTime.Get(), timeRect, pBrush);
        }

        // 3. 核心内容区域
        float padX = (std::max)(20.0f * scale, size.width * 0.06f);
        float padY = topBarH + 10.0f * scale;
        float bottomMargin = 50.0f * scale;
        D2D1_RECT_F contentBounds = D2D1::RectF(padX, padY, size.width - padX, size.height - bottomMargin);

        auto exerciseMode = ConfigManager::Instance().GetConfig().exerciseMode;
        if (exerciseMode == ExerciseMode::Comprehensive) {
            if (m_currentStage == 0) {
                m_neckRenderer.Render(info.pRenderTarget.Get(), contentBounds, scale);
            } else {
                m_eyeRenderer.Render(info.pRenderTarget.Get(), contentBounds, scale);
            }
        } else if (exerciseMode == ExerciseMode::NeckOnly) {
            m_neckRenderer.Render(info.pRenderTarget.Get(), contentBounds, scale);
        } else if (exerciseMode == ExerciseMode::EyeOnly) {
            m_eyeRenderer.Render(info.pRenderTarget.Get(), contentBounds, scale);
        } else {
            // 极简休息
            if (pBrush) {
                auto fmtSimple = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 24.0f * scale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
                pBrush->SetColor(D2D1::ColorF(0.7f, 0.9f, 0.8f));
                std::wstring msg = L"🍃 暂别屏幕，极目远眺或闭目深呼吸，让身心重获活力";
                D2D1_RECT_F centerRect = D2D1::RectF(0, size.height * 0.45f, size.width, size.height * 0.55f);
                if (fmtSimple) info.pRenderTarget->DrawTextW(msg.c_str(), static_cast<UINT32>(msg.length()), fmtSimple.Get(), centerRect, pBrush);
            }
        }

        // 4. 底部 ESC 退出提示
        auto fmtEsc = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 11.0f * scale, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        if (fmtEsc && pBrush) {
            pBrush->SetColor(D2D1::ColorF(0.6f, 0.65f, 0.72f));
            std::wstring escText = L"如遇紧急事务需处理，可按【ESC 键】立即退出全屏休息";
            D2D1_RECT_F escRect = D2D1::RectF(0, size.height - 36.0f * scale, size.width, size.height - 8.0f * scale);
            info.pRenderTarget->DrawTextW(escText.c_str(), static_cast<UINT32>(escText.length()), fmtEsc.Get(), escRect, pBrush);
        }
    } else {
        // 副屏幕：环境流光
        if (pBrush) {
            pBrush->SetColor(D2D1::ColorF(0.08f, 0.14f, 0.20f, 0.6f));
            info.pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(size.width / 2.0f, size.height / 2.0f), size.width * 0.35f, size.height * 0.35f), pBrush);

            auto fmtSub = d2d.GetCachedTextFormat(L"Microsoft YaHei UI", 18.0f * scale, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
            pBrush->SetColor(D2D1::ColorF(0.5f, 0.8f, 0.7f, 0.8f));
            std::wstring ambientText = L"🍃 休息工间 · 保持深长平缓呼吸";
            D2D1_RECT_F subRect = D2D1::RectF(0, size.height * 0.47f, size.width, size.height * 0.53f);
            if (fmtSub) info.pRenderTarget->DrawTextW(ambientText.c_str(), static_cast<UINT32>(ambientText.length()), fmtSub.Get(), subRect, pBrush);
        }
    }

    HRESULT hr = info.pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        info.pBrush.Reset();
        info.pRenderTarget.Reset();
    }

    EndPaint(info.hwnd, &ps);
}

LRESULT CALLBACK FullscreenMask::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    try {
        auto* info = reinterpret_cast<MonitorWindowInfo*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        switch (msg) {
            case WM_TIMER:
                FullscreenMask::Instance().OnTimer();
                return 0;

            case WM_PAINT:
                if (info) {
                    FullscreenMask::Instance().OnPaint(*info);
                }
                return 0;

            case WM_ERASEBKGND:
                return 1;

            case WM_APP_REST_ESCAPE:
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                if (msg == WM_APP_REST_ESCAPE || wParam == VK_ESCAPE) {
                    FullscreenMask::Instance().OnEscape();
                    return 0;
                }
                break;

            case WM_DISPLAYCHANGE:
                // 响应分辨率或多显示器插拔重构
                if (FullscreenMask::Instance().IsVisible()) {
                    FullscreenMask::Instance().CreateWindows();
                }
                return 0;

            case WM_SIZE:
                if (info && info->pRenderTarget) {
                    RECT rc;
                    GetClientRect(hwnd, &rc);
                    info->pRenderTarget->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
                }
                return 0;
        }
    } catch (...) {
        // C++ 异常屏障
#ifdef _DEBUG
        OutputDebugStringW(L"[SitStandReminder] Exception caught in FullscreenMask::WndProc\n");
#endif
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

