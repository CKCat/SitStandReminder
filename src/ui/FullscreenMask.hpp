#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d2d1.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

struct MonitorWindowInfo {
    HWND hwnd = nullptr;
    RECT rect = { 0 };
    bool isPrimary = false;
    ComPtr<ID2D1HwndRenderTarget> pRenderTarget;
    ComPtr<ID2D1SolidColorBrush> pBrush;
};
#else
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "../graphics/D2DCompat.hpp"
#include "../graphics/linux/LinuxCanvas.hpp"
#endif

#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include "../core/StateMachine.hpp"
#include "../graphics/NeckExerciseRenderer.hpp"
#include "../graphics/EyeExerciseRenderer.hpp"

class FullscreenMask {
public:
    static FullscreenMask& Instance() {
        static FullscreenMask instance;
        return instance;
    }

#ifdef _WIN32
    bool Initialize(HINSTANCE hInstance);
    HWND GetPrimaryHwnd() const;
#else
    bool Initialize(void* hInstance = nullptr);
    Window GetPrimaryWindow() const { return m_window; }
    void Destroy();
#endif

    void Show(bool show);
    void UpdateDisplay(int remainingSec, int totalSec, int stageIndex, const std::wstring& stageName);
    void OnEscape();

    bool IsVisible() const { return m_isVisible; }

private:
    FullscreenMask() = default;
    ~FullscreenMask() {
#ifdef _WIN32
        CloseAll();
#else
        Destroy();
#endif
    }

    NeckExerciseRenderer m_neckRenderer;
    EyeExerciseRenderer m_eyeRenderer;

    bool m_isVisible = false;
    int m_remainingSeconds = 0;
    int m_totalSeconds = 0;
    int m_currentStage = 0;
    std::wstring m_currentStageName;

#ifdef _WIN32
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC hdc, LPRECT lprc, LPARAM pData);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void CreateWindows();
    void CloseAll();
    void OnPaint(MonitorWindowInfo& info);
    void OnTimer();

    HINSTANCE m_hInstance = nullptr;
    std::vector<std::unique_ptr<MonitorWindowInfo>> m_monitors;
    UINT_PTR m_timerId = 0;
    ULONGLONG m_lastTickCount = 0;
#else
    void HandleEvent(const XEvent& ev);
    void OnAnimationTick();
    void Render();
    void RenderBackground();

    Window m_window = 0;
    GC m_gc = nullptr;
    std::unique_ptr<LinuxRenderTarget> m_pRenderTarget;
    std::unique_ptr<LinuxRenderTarget> m_pBgRenderTarget;
    XImage* m_ximage = nullptr;
    bool m_bgDirty = true;
    int m_lastExercisePhase = -1;
    int m_width = 0;
    int m_height = 0;
    std::chrono::steady_clock::time_point m_lastFrameTime;
    std::chrono::steady_clock::time_point m_lastRenderTime;

    void DestroyXImage();

public:
    float GetNeckProgress() const { return m_neckRenderer.GetPhaseProgress(); }
    float GetEyeProgress() const { return m_eyeRenderer.GetPhaseProgress(); }
    int GetCurrentStage() const { return m_currentStage; }
    void TriggerAnimationTick() { OnAnimationTick(); }
#endif
};
