#include "../D2DContext.hpp"
#include "LinuxCanvas.hpp"

#ifndef _WIN32

class LinuxD2DFactory : public ID2D1Factory {
public:
    bool CreatePathGeometry(ID2D1PathGeometry** ppPathGeometry) override {
        if (!ppPathGeometry) return false;
        *ppPathGeometry = new LinuxPathGeometry();
        return true;
    }
    bool CreateStrokeStyle(const void* /*props*/, const float* /*dashes*/, uint32_t /*dashesCount*/, ID2D1StrokeStyle** ppStrokeStyle) override {
        if (!ppStrokeStyle) return false;
        *ppStrokeStyle = new LinuxStrokeStyle();
        return true;
    }
};

class LinuxDWriteFactory : public IDWriteFactory {
public:
    bool CreateTextFormat(
        const wchar_t* fontFamilyName,
        void* /*fontCollection*/,
        DWRITE_FONT_WEIGHT fontWeight,
        DWRITE_FONT_STYLE fontStyle,
        int /*fontStretch*/,
        float fontSize,
        const wchar_t* /*localeName*/,
        IDWriteTextFormat** ppTextFormat
    ) override {
        if (!ppTextFormat) return false;
        auto* tf = new IDWriteTextFormat();
        tf->fontFamily = fontFamilyName ? fontFamilyName : L"sans-serif";
        tf->fontSize = fontSize;
        tf->weight = fontWeight;
        tf->style = fontStyle;
        *ppTextFormat = tf;
        return true;
    }
};

bool D2DContext::Initialize() {
    if (m_initialized) return true;
    m_pLinuxFactory = new LinuxD2DFactory();
    m_pLinuxWriteFactory = new LinuxDWriteFactory();
    m_pLinuxStrokeStyle = new LinuxStrokeStyle();
    m_initialized = true;
    return true;
}

void D2DContext::Uninitialize() {
    ClearFontCache();
    if (m_pLinuxFactory) {
        delete m_pLinuxFactory;
        m_pLinuxFactory = nullptr;
    }
    if (m_pLinuxWriteFactory) {
        delete m_pLinuxWriteFactory;
        m_pLinuxWriteFactory = nullptr;
    }
    if (m_pLinuxStrokeStyle) {
        delete m_pLinuxStrokeStyle;
        m_pLinuxStrokeStyle = nullptr;
    }
    m_initialized = false;
}

ID2D1Factory* D2DContext::GetD2DFactory() const {
    return m_pLinuxFactory;
}

IDWriteFactory* D2DContext::GetDWriteFactory() const {
    return m_pLinuxWriteFactory;
}

ID2D1StrokeStyle* D2DContext::GetRoundStrokeStyle() const {
    return m_pLinuxStrokeStyle;
}

ComPtr<IDWriteTextFormat> D2DContext::GetCachedTextFormat(
    const std::wstring& fontFamily,
    float fontSize,
    DWRITE_FONT_WEIGHT weight,
    DWRITE_FONT_STYLE style,
    DWRITE_TEXT_ALIGNMENT textAlignment,
    DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment
) {
    FontKey key{ fontFamily, fontSize, weight, style, textAlignment, paragraphAlignment };
    auto it = m_fontCache.find(key);
    if (it != m_fontCache.end()) {
        return it->second;
    }

    IDWriteTextFormat* tf = nullptr;
    if (m_pLinuxWriteFactory && m_pLinuxWriteFactory->CreateTextFormat(fontFamily.c_str(), nullptr, weight, style, 1, fontSize, L"zh-CN", &tf)) {
        tf->SetTextAlignment(textAlignment);
        tf->SetParagraphAlignment(paragraphAlignment);
        ComPtr<IDWriteTextFormat> ptr;
        ptr.Attach(tf);
        m_fontCache.emplace(std::move(key), ptr);
        return ptr;
    }
    return ComPtr<IDWriteTextFormat>();
}

void D2DContext::ClearFontCache() {
    m_fontCache.clear();
}

#endif
