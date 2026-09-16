#pragma once

#include "D2DCompat.hpp"
#include <string>
#include <unordered_map>

#ifdef _WIN32
#include <d2d1.h>
#include <dwrite.h>
#endif

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

    ID2D1Factory* GetD2DFactory() const;
    IDWriteFactory* GetDWriteFactory() const;

#ifdef _WIN32
    // 创建针对特定 HWND 的渲染目标
    bool CreateHwndRenderTarget(HWND hwnd, ComPtr<ID2D1HwndRenderTarget>& outTarget, UINT width = 0, UINT height = 0);

    // 创建针对内存 DC 的 32-bit Premultiplied Alpha 渲染目标（完美用于 UpdateLayeredWindow）
    bool CreateDCRenderTarget(ComPtr<ID2D1DCRenderTarget>& outTarget);

    static float GetWindowDpiScale(HWND hwnd);
#endif

    // 高性能全局字体格式缓存池
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
    ID2D1StrokeStyle* GetRoundStrokeStyle() const;

private:
    D2DContext() = default;
    ~D2DContext() { Uninitialize(); }

#ifdef _WIN32
    ComPtr<ID2D1Factory> m_d2dFactory;
    ComPtr<IDWriteFactory> m_dwriteFactory;
    ComPtr<ID2D1StrokeStyle> m_roundStrokeStyle;
#else
    ID2D1Factory* m_pLinuxFactory = nullptr;
    IDWriteFactory* m_pLinuxWriteFactory = nullptr;
    ID2D1StrokeStyle* m_pLinuxStrokeStyle = nullptr;
#endif

    std::unordered_map<FontKey, ComPtr<IDWriteTextFormat>, FontKeyHash> m_fontCache;
    bool m_initialized = false;
};
