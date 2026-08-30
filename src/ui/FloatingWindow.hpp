#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d2d1.h>
#include <wrl/client.h>
#include <string>
#include "../core/StateMachine.hpp"
#include "../core/AppConstants.hpp"

using Microsoft::WRL::ComPtr;

enum class DockState {
    Floating = 0,
    Snapped_Left,           // 贴紧左边缘常驻 (展开展示，不自动折叠)
    Snapped_Right,          // 贴紧右边缘常驻 (展开展示，不自动折叠)
    Snapped_Top,            // 贴紧顶边缘常驻 (展开展示，不自动折叠)
    DockedLeft_Expanded,    // 推入左边缘折叠模式 (展开中)
    DockedLeft_Collapsed,   // 推入左边缘折叠模式 (已折叠为拉手)
    DockedRight_Expanded,   // 推入右边缘折叠模式 (展开中)
    DockedRight_Collapsed,  // 推入右边缘折叠模式 (已折叠为拉手)
    DockedTop_Expanded,     // 推入顶边缘折叠模式 (展开中)
    DockedTop_Collapsed     // 推入顶边缘折叠模式 (已折叠为拉手)
};

class FloatingWindow {
public:
    static FloatingWindow& Instance() {
        static FloatingWindow instance;
        return instance;
    }

    bool Create(HINSTANCE hInstance);
    void Destroy();

    void Show(bool show);
    void UpdateState(AppState state, int remainingSec, int totalSec);
    void RepositionDefault();
    void ClampToWorkArea();
    void OnConfigChanged();
    void OnThemeChanged();
    void StopAnimation();

    HWND GetHwnd() const { return m_hwnd; }

private:
    FloatingWindow() = default;
    ~FloatingWindow() { Destroy(); }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void Render();
    void EnsureMemoryDC(int width, int height);

    void CheckEdgeDock(bool isFinal);
    void StartSlideAnimation(int targetPos, bool isHorizontal, DockState finalState);
    void OnAnimationTick();
    void UpdateHoverTimerState();

    HWND m_hwnd = nullptr;
    ComPtr<ID2D1DCRenderTarget> m_pDCRenderTarget;
    ComPtr<ID2D1SolidColorBrush> m_pBrush;

    HDC m_memDC = nullptr;
    HBITMAP m_hBitmap = nullptr;
    HBITMAP m_hOldBitmap = nullptr;
    void* m_pBits = nullptr;
    int m_dcWidth = 0;
    int m_dcHeight = 0;

    AppState m_state = AppState::Working;
    int m_remainingSeconds = 0;
    int m_totalSeconds = 0;
    int m_animTick = 0;

    int m_width = AppConstants::FloatingWindowDimensions::BASE_WIDTH;
    int m_height = AppConstants::FloatingWindowDimensions::BASE_HEIGHT;

    DockState m_dockState = DockState::Floating;
    POINT m_dragCursorOffset = { 0, 0 };
    bool m_isMouseTracking = false;
    bool m_isMouseHovered = false;
    int m_outsideTicks = 0;

    // 60FPS 缓动平滑动画参数
    bool m_isAnimating = false;
    int m_animStartPos = 0;
    int m_animTargetPos = 0;
    int m_animCurrentFrame = 0;
    int m_animTotalFrames = 11; // ~176ms (11 * 16ms)
    bool m_animIsHorizontal = true;
    DockState m_animFinalState = DockState::Floating;
};

