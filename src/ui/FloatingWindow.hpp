#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d2d1.h>
#include <windows.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#else
#include "../graphics/D2DCompat.hpp"
#include "../graphics/linux/LinuxCanvas.hpp"
#include <X11/Xlib.h>
#include <chrono>
#endif

#include "../core/AppConstants.hpp"
#include "../core/StateMachine.hpp"
#include <memory>
#include <string>

enum class DockState {
  Floating = 0,
  Snapped_Left,        // 贴紧左边缘常驻 (展开展示，不自动折叠)
  Snapped_Right,       // 贴紧右边缘常驻 (展开展示，不自动折叠)
  Snapped_Top,         // 贴紧顶边缘常驻 (展开展示，不自动折叠)
  DockedLeft_Expanded, // 推入左边缘折叠模式 (展开中)
  DockedLeft_Collapsed,  // 推入左边缘折叠模式 (已折叠为拉手)
  DockedRight_Expanded,  // 推入右边缘折叠模式 (展开中)
  DockedRight_Collapsed, // 推入右边缘折叠模式 (已折叠为拉手)
  DockedTop_Expanded,    // 推入顶边缘折叠模式 (展开中)
  DockedTop_Collapsed    // 推入顶边缘折叠模式 (已折叠为拉手)
};

class FloatingWindow {
public:
  static FloatingWindow &Instance() {
    static FloatingWindow instance;
    return instance;
  }

#ifdef _WIN32
  bool Create(HINSTANCE hInstance);
  HWND GetHwnd() const { return m_hwnd; }
#else
  bool Create(void *hInstance = nullptr);
  Window GetWindow() const { return m_window; }
#endif

  void Destroy();
  void Show(bool show);
  void UpdateState(AppState state, int remainingSec, int totalSec);
  void RepositionDefault();
  void ClampToWorkArea();
  void OnConfigChanged();
  void OnThemeChanged();
  void StopAnimation();

private:
  FloatingWindow() = default;
  ~FloatingWindow() { Destroy(); }

#ifdef _WIN32
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);
  void Render();
  void EnsureMemoryDC(int width, int height);

  void CheckEdgeDock(bool isFinal);
  void StartSlideAnimation(int targetPos, bool isHorizontal,
                           DockState finalState);
  void OnAnimationTick();
  void UpdateHoverTimerState();

  HWND m_hwnd = nullptr;
  ComPtr<ID2D1DCRenderTarget> m_pDCRenderTarget;
  ComPtr<ID2D1SolidColorBrush> m_pBrush;

  HDC m_memDC = nullptr;
  HBITMAP m_hBitmap = nullptr;
  HBITMAP m_hOldBitmap = nullptr;
  void *m_pBits = nullptr;
  int m_dcWidth = 0;
  int m_dcHeight = 0;

  AppState m_state = AppState::Working;
  int m_remainingSeconds = 45 * 60;
  int m_totalSeconds = 45 * 60;
  int m_animTick = 0;

  int m_width = AppConstants::FloatingWindowDimensions::BASE_WIDTH;
  int m_height = AppConstants::FloatingWindowDimensions::BASE_HEIGHT;

  DockState m_dockState = DockState::Floating;
  POINT m_dragCursorOffset = {0, 0};
  bool m_isMouseTracking = false;
  bool m_isMouseHovered = false;
  int m_outsideTicks = 0;

  // 60FPS 缓动平滑动画参数
  bool m_isAnimating = false;
  int m_animStartPos = 0;
  int m_animTargetPos = 0;
  int m_animCurrentFrame = 0;
  int m_animTotalFrames = 11;
  bool m_animIsHorizontal = true;
  DockState m_animFinalState = DockState::Floating;

#else
public:
  void HandleEvent(const XEvent &ev);
  void SimulateClick(int x = 10, int y = 10, int button = 1);
  const LinuxRenderTarget *GetRenderTarget() const {
    return m_pRenderTarget.get();
  }
  void TriggerRender() { Render(); }
  bool IsTopMost() const { return m_isTopMost; }
  void SetTopMost(bool topMost);
  int GetX() const { return m_x; }
  int GetY() const { return m_y; }
  int GetWidth() const { return m_width; }
  int GetHeight() const { return m_height; }
  DockState GetDockState() const { return m_dockState; }
  void TestSetDockStateAndRender(DockState state) {
    m_dockState = state;
    Render();
  }

private:
  void Render();
  void CheckEdgeDock(bool isFinal);
  void StartSlideAnimation(int targetPos, bool isHorizontal,
                           DockState finalState);
  void OnAnimationTick();
  void UpdateHoverTimerState();

  Window m_window = 0;
  GC m_gc = nullptr;
  std::unique_ptr<LinuxRenderTarget> m_pRenderTarget;
  bool m_visible = false;
  bool m_isTopMost = true;

  int m_x = 100;
  int m_y = 100;
  int m_dragStartMouseX = 0;
  int m_dragStartMouseY = 0;
  int m_dragStartWinX = 0;
  int m_dragStartWinY = 0;
  bool m_isDragging = false;

  AppState m_state = AppState::Working;
  int m_remainingSeconds = 45 * 60;
  int m_totalSeconds = 45 * 60;
  int m_animTick = 0;

  int m_width = AppConstants::FloatingWindowDimensions::BASE_WIDTH;
  int m_height = AppConstants::FloatingWindowDimensions::BASE_HEIGHT;

  DockState m_dockState = DockState::Floating;
  bool m_isMouseHovered = false;
  int m_outsideTicks = 0;

  // 60FPS 平滑动画
  bool m_isAnimating = false;
  int m_animStartPos = 0;
  int m_animTargetPos = 0;
  int m_animCurrentFrame = 0;
  int m_animTotalFrames = 11;
  bool m_animIsHorizontal = true;
  DockState m_animFinalState = DockState::Floating;

  std::chrono::steady_clock::time_point m_lastClickTime{};
  bool m_hasLastClick = false;
#endif
};
