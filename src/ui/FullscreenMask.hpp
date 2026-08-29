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
#include <vector>
#include <memory>
#include "../core/StateMachine.hpp"
#include "../graphics/NeckExerciseRenderer.hpp"
#include "../graphics/EyeExerciseRenderer.hpp"

using Microsoft::WRL::ComPtr;

struct MonitorWindowInfo {
    HWND hwnd = nullptr;
    RECT rect = { 0 };
    bool isPrimary = false;
    ComPtr<ID2D1HwndRenderTarget> pRenderTarget;
    ComPtr<ID2D1SolidColorBrush> pBrush;
};

class FullscreenMask {
public:
    static FullscreenMask& Instance() {
        static FullscreenMask instance;
        return instance;
    }

    bool Initialize(HINSTANCE hInstance);
    void Show(bool show);
    void UpdateDisplay(int remainingSec, int totalSec, int stageIndex, const std::wstring& stageName);
    void OnEscape();

    bool IsVisible() const { return m_isVisible; }
    HWND GetPrimaryHwnd() const;

private:
    FullscreenMask() = default;
    ~FullscreenMask() { CloseAll(); }

    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC hdc, LPRECT lprc, LPARAM pData);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void CreateWindows();
    void CloseAll();
    void OnPaint(MonitorWindowInfo& info);
    void OnTimer();

    HINSTANCE m_hInstance = nullptr;
    bool m_isVisible = false;
    std::vector<std::unique_ptr<MonitorWindowInfo>> m_monitors;

    NeckExerciseRenderer m_neckRenderer;
    EyeExerciseRenderer m_eyeRenderer;

    int m_remainingSeconds = 0;
    int m_totalSeconds = 0;
    int m_currentStage = 0;
    std::wstring m_currentStageName;

    UINT_PTR m_timerId = 0;
    ULONGLONG m_lastTickCount = 0;
};

