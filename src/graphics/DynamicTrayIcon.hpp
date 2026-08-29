#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <functional>
#include <string>
#include <vector>
#include "../core/StateMachine.hpp"
#include "../core/ConfigManager.hpp"

using Microsoft::WRL::ComPtr;

class DynamicTrayIcon {
public:
    static DynamicTrayIcon& Instance() {
        static DynamicTrayIcon instance;
        return instance;
    }

    // 生成微缩数字倒计时图标 (带微型环形进度)
    HICON CreateCountdownIcon(int remainingSec, int totalSec, AppState state, bool isDark);

    // 生成 RunCat 奔跑动画小猫图标
    HICON CreateRunCatIcon(int frameIndex, AppState state, bool isDark, float cpuUsage = 0.0f);

    // 获取系统实时 CPU 占用率 (0.0f ~ 100.0f)
    static float GetCpuUsage();

private:
    DynamicTrayIcon();
    ~DynamicTrayIcon();

    void EnsureMemoryDC(int width, int height);
    HICON RenderToHIcon(int size, const std::function<void(ID2D1DCRenderTarget* pRT, float scale, int size)>& drawCallback);
    ID2D1SolidColorBrush* GetSolidBrush(const D2D1_COLOR_F& color);

    HDC m_memDC = nullptr;
    HBITMAP m_hBitmap = nullptr;
    HBITMAP m_hOldBitmap = nullptr;
    void* m_pBits = nullptr;
    int m_dcWidth = 0;
    int m_dcHeight = 0;

    ComPtr<ID2D1DCRenderTarget> m_pDCRenderTarget;
    ComPtr<ID2D1SolidColorBrush> m_pBrush;
};

