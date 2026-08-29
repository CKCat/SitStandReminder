#include "D2DContext.hpp"

bool D2DContext::Initialize() {
    if (m_initialized) return true;

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2dFactory.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf())
    );
    if (FAILED(hr)) return false;

    // 创建常驻圆头笔触样式
    D2D1_STROKE_STYLE_PROPERTIES props = D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
        D2D1_LINE_JOIN_ROUND, 10.0f, D2D1_DASH_STYLE_SOLID, 0.0f
    );
    m_d2dFactory->CreateStrokeStyle(&props, nullptr, 0, m_roundStrokeStyle.GetAddressOf());

    m_initialized = true;
    return true;
}

void D2DContext::Uninitialize() {
    ClearFontCache();
    m_roundStrokeStyle.Reset();
    m_dwriteFactory.Reset();
    m_d2dFactory.Reset();
    m_initialized = false;
}

void D2DContext::ClearFontCache() {
    m_fontCache.clear();
}

bool D2DContext::CreateHwndRenderTarget(HWND hwnd, ComPtr<ID2D1HwndRenderTarget>& outTarget, UINT width, UINT height) {
    if (!m_initialized && !Initialize()) return false;

    RECT rc;
    GetClientRect(hwnd, &rc);

    UINT w = (width > 0) ? width : static_cast<UINT>(rc.right - rc.left);
    UINT h = (height > 0) ? height : static_cast<UINT>(rc.bottom - rc.top);
    if (w == 0) w = 1;
    if (h == 0) h = 1;

    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        0.0f, 0.0f,
        D2D1_RENDER_TARGET_USAGE_NONE,
        D2D1_FEATURE_LEVEL_DEFAULT
    );

    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
        hwnd,
        D2D1::SizeU(w, h),
        D2D1_PRESENT_OPTIONS_IMMEDIATELY
    );

    HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(
        rtProps,
        hwndProps,
        outTarget.ReleaseAndGetAddressOf()
    );

    if (SUCCEEDED(hr)) {
        // 显式锁定渲染目标基准为 96 DPI，确保与上层物理像素/scale 统一度量，杜绝高分屏二次缩放
        outTarget->SetDpi(96.0f, 96.0f);
        return true;
    }
    return false;
}

bool D2DContext::CreateDCRenderTarget(ComPtr<ID2D1DCRenderTarget>& outTarget) {
    if (!m_initialized && !Initialize()) return false;

    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        0.0f, 0.0f,
        D2D1_RENDER_TARGET_USAGE_NONE,
        D2D1_FEATURE_LEVEL_DEFAULT
    );

    HRESULT hr = m_d2dFactory->CreateDCRenderTarget(&rtProps, outTarget.ReleaseAndGetAddressOf());
    return SUCCEEDED(hr);
}

ComPtr<IDWriteTextFormat> D2DContext::GetCachedTextFormat(
    const std::wstring& fontFamily,
    float fontSize,
    DWRITE_FONT_WEIGHT weight,
    DWRITE_FONT_STYLE style,
    DWRITE_TEXT_ALIGNMENT textAlignment,
    DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment
) {
    if (!m_initialized && !Initialize()) return nullptr;

    FontKey key{ fontFamily, fontSize, weight, style, textAlignment, paragraphAlignment };

    auto it = m_fontCache.find(key);
    if (it != m_fontCache.end()) {
        return it->second;
    }

    ComPtr<IDWriteTextFormat> textFormat;
    HRESULT hr = m_dwriteFactory->CreateTextFormat(
        fontFamily.c_str(),
        nullptr,
        weight,
        style,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSize,
        L"zh-CN",
        textFormat.GetAddressOf()
    );

    if (SUCCEEDED(hr)) {
        textFormat->SetTextAlignment(textAlignment);
        textFormat->SetParagraphAlignment(paragraphAlignment);
        m_fontCache.emplace(std::move(key), textFormat);
    }
    return textFormat;
}

float D2DContext::GetWindowDpiScale(HWND hwnd) {
    if (!hwnd) return 1.0f;
    UINT dpi = GetDpiForWindow(hwnd);
    if (dpi == 0) dpi = 96;
    return static_cast<float>(dpi) / 96.0f;
}

