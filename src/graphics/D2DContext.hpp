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
#include <string>
#include <unordered_map>

using Microsoft::WRL::ComPtr;

struct FontKey {
    std::wstring fontFamily;
    float fontSize;
    DWRITE_FONT_WEIGHT weight;
    DWRITE_FONT_STYLE style;
    DWRITE_TEXT_ALIGNMENT textAlignment;
    DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment;

    bool operator==(const FontKey& other) const {
        return fontSize == other.fontSize &&
               weight == other.weight &&
               style == other.style &&
               textAlignment == other.textAlignment &&
               paragraphAlignment == other.paragraphAlignment &&
               fontFamily == other.fontFamily;
    }
};

struct FontKeyHash {
    std::size_t operator()(const FontKey& k) const {
        std::size_t h1 = std::hash<std::wstring>{}(k.fontFamily);
        std::size_t h2 = std::hash<float>{}(k.fontSize);
        std::size_t h3 = static_cast<std::size_t>(k.weight) |
                         (static_cast<std::size_t>(k.style) << 8) |
                         (static_cast<std::size_t>(k.textAlignment) << 16) |
                         (static_cast<std::size_t>(k.paragraphAlignment) << 24);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class D2DContext {
public:
    static D2DContext& Instance() {
        static D2DContext instance;
        return instance;
    }

    bool Initialize();
    void Uninitialize();

    ID2D1Factory* GetD2DFactory() const { return m_d2dFactory.Get(); }
    IDWriteFactory* GetDWriteFactory() const { return m_dwriteFactory.Get(); }

    // 创建针对特定 HWND 的渲染目标
    bool CreateHwndRenderTarget(HWND hwnd, ComPtr<ID2D1HwndRenderTarget>& outTarget, UINT width = 0, UINT height = 0);

    // 创建针对内存 DC 的 32-bit Premultiplied Alpha 渲染目标（完美用于 UpdateLayeredWindow）
    bool CreateDCRenderTarget(ComPtr<ID2D1DCRenderTarget>& outTarget);

    // 高性能全局字体格式缓存池（杜绝 60FPS 帧循环中重复分配 COM 实例与临时堆字符串）
    ComPtr<IDWriteTextFormat> GetCachedTextFormat(
        const std::wstring& fontFamily,
        float fontSize,
        DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL,
        DWRITE_TEXT_ALIGNMENT textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING,
        DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_NEAR
    );

    void ClearFontCache();

    // 常驻圆头笔触样式 (Round Cap Stroke Style)
    ID2D1StrokeStyle* GetRoundStrokeStyle() const { return m_roundStrokeStyle.Get(); }

    static float GetWindowDpiScale(HWND hwnd);

private:
    D2DContext() = default;
    ~D2DContext() { Uninitialize(); }

    ComPtr<ID2D1Factory> m_d2dFactory;
    ComPtr<IDWriteFactory> m_dwriteFactory;
    ComPtr<ID2D1StrokeStyle> m_roundStrokeStyle;
    std::unordered_map<FontKey, ComPtr<IDWriteTextFormat>, FontKeyHash> m_fontCache;
    bool m_initialized = false;
};

