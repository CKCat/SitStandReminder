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
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#else
// Linux / POSIX D2D & DirectWrite Compatibility Layer
#include <cstdint>
#include <cmath>
#include <cwchar>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

using UINT32 = uint32_t;
using BOOL = int;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef swprintf_s
#define swprintf_s(buf, ...) swprintf((buf), sizeof(buf) / sizeof((buf)[0]), __VA_ARGS__)
#endif

// 1. 跨平台极简智能指针 ComPtr
template <typename T>
class ComPtr {
public:
    ComPtr() : m_ptr(nullptr) {}
    ComPtr(T* ptr) : m_ptr(ptr) { if (m_ptr) m_ptr->AddRef(); }
    ComPtr(const ComPtr& other) : m_ptr(other.m_ptr) { if (m_ptr) m_ptr->AddRef(); }
    ComPtr(ComPtr&& other) noexcept : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }
    ~ComPtr() { Reset(); }

    ComPtr& operator=(const ComPtr& other) {
        if (this != &other) {
            Reset();
            m_ptr = other.m_ptr;
            if (m_ptr) m_ptr->AddRef();
        }
        return *this;
    }
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            Reset();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    T* Get() const { return m_ptr; }
    T* operator->() const { return m_ptr; }
    T& operator*() const { return *m_ptr; }
    T** GetAddressOf() { Reset(); return &m_ptr; }
    explicit operator bool() const { return m_ptr != nullptr; }
    bool operator==(std::nullptr_t) const { return m_ptr == nullptr; }
    bool operator!=(std::nullptr_t) const { return m_ptr != nullptr; }

    void Reset() {
        if (m_ptr) {
            m_ptr->Release();
            m_ptr = nullptr;
        }
    }
    void Attach(T* ptr) {
        Reset();
        m_ptr = ptr;
    }
    T* Detach() {
        T* temp = m_ptr;
        m_ptr = nullptr;
        return temp;
    }

private:
    T* m_ptr = nullptr;
};

// 2. 基础几何与颜色数据结构
struct D2D1_POINT_2F {
    float x = 0.0f;
    float y = 0.0f;
};

struct D2D1_RECT_F {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

struct D2D1_COLOR_F {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct D2D1_ELLIPSE {
    D2D1_POINT_2F point;
    float radiusX = 0.0f;
    float radiusY = 0.0f;
};

struct D2D1_ROUNDED_RECT {
    D2D1_RECT_F rect;
    float radiusX = 0.0f;
    float radiusY = 0.0f;
};

struct D2D1_MATRIX_3X2_F {
    float _11 = 1.0f; float _12 = 0.0f;
    float _21 = 0.0f; float _22 = 1.0f;
    float _31 = 0.0f; float _32 = 0.0f;
};

inline D2D1_MATRIX_3X2_F operator*(const D2D1_MATRIX_3X2_F& a, const D2D1_MATRIX_3X2_F& b) {
    return D2D1_MATRIX_3X2_F{
        a._11 * b._11 + a._12 * b._21,
        a._11 * b._12 + a._12 * b._22,
        a._21 * b._11 + a._22 * b._21,
        a._21 * b._12 + a._22 * b._22,
        a._31 * b._11 + a._32 * b._21 + b._31,
        a._31 * b._12 + a._32 * b._22 + b._32
    };
}

struct D2D1_BEZIER_SEGMENT {
    D2D1_POINT_2F point1;
    D2D1_POINT_2F point2;
    D2D1_POINT_2F point3;
};

struct D2D1_GRADIENT_STOP {
    float position = 0.0f;
    D2D1_COLOR_F color = { 0.0f, 0.0f, 0.0f, 1.0f };
};

struct D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES {
    D2D1_POINT_2F startPoint;
    D2D1_POINT_2F endPoint;
};

struct D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES {
    D2D1_POINT_2F center;
    D2D1_POINT_2F gradientOriginOffset;
    float radiusX = 0.0f;
    float radiusY = 0.0f;
};

struct D2D1_SIZE_F {
    float width = 0.0f;
    float height = 0.0f;
};

enum D2D1_SWEEP_DIRECTION {
    D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE = 0,
    D2D1_SWEEP_DIRECTION_CLOCKWISE = 1
};

enum D2D1_ARC_SIZE {
    D2D1_ARC_SIZE_SMALL = 0,
    D2D1_ARC_SIZE_LARGE = 1
};

struct D2D1_ARC_SEGMENT {
    D2D1_POINT_2F point;
    D2D1_SIZE_F size;
    float rotationAngle = 0.0f;
    D2D1_SWEEP_DIRECTION sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
    D2D1_ARC_SIZE arcSize = D2D1_ARC_SIZE_SMALL;
};

enum D2D1_FIGURE_BEGIN {
    D2D1_FIGURE_BEGIN_FILLED = 0,
    D2D1_FIGURE_BEGIN_HOLLOW = 1
};

enum D2D1_FIGURE_END {
    D2D1_FIGURE_END_OPEN = 0,
    D2D1_FIGURE_END_CLOSED = 1
};

enum D2D1_ANTIALIAS_MODE {
    D2D1_ANTIALIAS_MODE_PER_PRIMITIVE = 0,
    D2D1_ANTIALIAS_MODE_ALIASED = 1
};

enum D2D1_DRAW_TEXT_OPTIONS {
    D2D1_DRAW_TEXT_OPTIONS_NONE = 0,
    D2D1_DRAW_TEXT_OPTIONS_NO_SNAP = 1,
    D2D1_DRAW_TEXT_OPTIONS_CLIP = 2,
    D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT = 4
};

enum DWRITE_FONT_WEIGHT {
    DWRITE_FONT_WEIGHT_NORMAL = 400,
    DWRITE_FONT_WEIGHT_REGULAR = 400,
    DWRITE_FONT_WEIGHT_SEMI_BOLD = 600,
    DWRITE_FONT_WEIGHT_BOLD = 700
};

enum DWRITE_FONT_STYLE {
    DWRITE_FONT_STYLE_NORMAL = 0,
    DWRITE_FONT_STYLE_OBLIQUE = 1,
    DWRITE_FONT_STYLE_ITALIC = 2
};

enum DWRITE_TEXT_ALIGNMENT {
    DWRITE_TEXT_ALIGNMENT_LEADING = 0,
    DWRITE_TEXT_ALIGNMENT_TRAILING = 1,
    DWRITE_TEXT_ALIGNMENT_CENTER = 2,
    DWRITE_TEXT_ALIGNMENT_JUSTIFIED = 3
};

enum DWRITE_PARAGRAPH_ALIGNMENT {
    DWRITE_PARAGRAPH_ALIGNMENT_NEAR = 0,
    DWRITE_PARAGRAPH_ALIGNMENT_FAR = 1,
    DWRITE_PARAGRAPH_ALIGNMENT_CENTER = 2
};

namespace D2D1 {
    inline D2D1_POINT_2F Point2F(float x = 0.0f, float y = 0.0f) {
        return D2D1_POINT_2F{ x, y };
    }
    inline D2D1_RECT_F RectF(float l = 0.0f, float t = 0.0f, float r = 0.0f, float b = 0.0f) {
        return D2D1_RECT_F{ l, t, r, b };
    }
    inline D2D1_COLOR_F ColorF(float r, float g, float b, float a = 1.0f) {
        return D2D1_COLOR_F{ r, g, b, a };
    }
    inline D2D1_COLOR_F ColorF(uint32_t rgb, float a = 1.0f) {
        float r = static_cast<float>((rgb >> 16) & 0xFF) / 255.0f;
        float g = static_cast<float>((rgb >> 8) & 0xFF) / 255.0f;
        float b = static_cast<float>(rgb & 0xFF) / 255.0f;
        return D2D1_COLOR_F{ r, g, b, a };
    }
    inline D2D1_ELLIPSE Ellipse(const D2D1_POINT_2F& center, float rx, float ry) {
        return D2D1_ELLIPSE{ center, rx, ry };
    }
    inline D2D1_ROUNDED_RECT RoundedRect(const D2D1_RECT_F& rect, float rx, float ry) {
        return D2D1_ROUNDED_RECT{ rect, rx, ry };
    }
    inline D2D1_BEZIER_SEGMENT BezierSegment(const D2D1_POINT_2F& p1, const D2D1_POINT_2F& p2, const D2D1_POINT_2F& p3) {
        return D2D1_BEZIER_SEGMENT{ p1, p2, p3 };
    }
    inline D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES LinearGradientBrushProperties(const D2D1_POINT_2F& start, const D2D1_POINT_2F& end) {
        return D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES{ start, end };
    }
    inline D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES RadialGradientBrushProperties(
        const D2D1_POINT_2F& center,
        const D2D1_POINT_2F& gradientOriginOffset,
        float radiusX,
        float radiusY
    ) {
        return D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES{ center, gradientOriginOffset, radiusX, radiusY };
    }
    inline D2D1_SIZE_F SizeF(float width = 0.0f, float height = 0.0f) {
        return D2D1_SIZE_F{ width, height };
    }
    inline D2D1_ARC_SEGMENT ArcSegment(
        const D2D1_POINT_2F& point,
        const D2D1_SIZE_F& size,
        float rotationAngle,
        D2D1_SWEEP_DIRECTION sweepDirection,
        D2D1_ARC_SIZE arcSize
    ) {
        return D2D1_ARC_SEGMENT{ point, size, rotationAngle, sweepDirection, arcSize };
    }
    namespace Matrix3x2F {
        inline D2D1_MATRIX_3X2_F Identity() {
            return D2D1_MATRIX_3X2_F{ 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
        }
        inline D2D1_MATRIX_3X2_F Translation(float x, float y) {
            return D2D1_MATRIX_3X2_F{ 1.0f, 0.0f, 0.0f, 1.0f, x, y };
        }
        inline D2D1_MATRIX_3X2_F Rotation(float angleDeg, D2D1_POINT_2F center = { 0, 0 }) {
            float rad = angleDeg * 3.14159265358979323846f / 180.0f;
            float c = std::cos(rad);
            float s = std::sin(rad);
            return D2D1_MATRIX_3X2_F{
                c, s,
                -s, c,
                center.x * (1.0f - c) + center.y * s,
                center.y * (1.0f - c) - center.x * s
            };
        }
        inline D2D1_MATRIX_3X2_F Scale(float sx, float sy, D2D1_POINT_2F center = { 0, 0 }) {
            return D2D1_MATRIX_3X2_F{
                sx, 0.0f,
                0.0f, sy,
                center.x * (1.0f - sx),
                center.y * (1.0f - sy)
            };
        }
    }
}

// 3. 抽象绘图接口与引用计数基类
class IUnknownCompat {
public:
    virtual ~IUnknownCompat() = default;
    virtual uint32_t AddRef() { return ++m_refCount; }
    virtual uint32_t Release() {
        if (--m_refCount == 0) {
            delete this;
            return 0;
        }
        return m_refCount;
    }
private:
    uint32_t m_refCount = 1;
};

class ID2D1Resource : public IUnknownCompat {};

class ID2D1StrokeStyle : public ID2D1Resource {};

class ID2D1Brush : public ID2D1Resource {};

class ID2D1SolidColorBrush : public ID2D1Brush {
public:
    virtual void SetColor(const D2D1_COLOR_F& color) { m_color = color; }
    virtual D2D1_COLOR_F GetColor() const { return m_color; }
protected:
    D2D1_COLOR_F m_color{ 0, 0, 0, 1 };
};

class ID2D1GradientStopCollection : public ID2D1Resource {
public:
    std::vector<D2D1_GRADIENT_STOP> stops;
};

class ID2D1LinearGradientBrush : public ID2D1Brush {
public:
    virtual void SetStartPoint(D2D1_POINT_2F startPoint) { m_start = startPoint; }
    virtual void SetEndPoint(D2D1_POINT_2F endPoint) { m_end = endPoint; }
    virtual D2D1_POINT_2F GetStartPoint() const { return m_start; }
    virtual D2D1_POINT_2F GetEndPoint() const { return m_end; }
protected:
    D2D1_POINT_2F m_start;
    D2D1_POINT_2F m_end;
};

class ID2D1RadialGradientBrush : public ID2D1Brush {
public:
    virtual void SetCenter(D2D1_POINT_2F center) { m_center = center; }
    virtual void SetGradientOriginOffset(D2D1_POINT_2F gradientOriginOffset) { m_originOffset = gradientOriginOffset; }
    virtual void SetRadiusX(float radiusX) { m_radiusX = radiusX; }
    virtual void SetRadiusY(float radiusY) { m_radiusY = radiusY; }
    virtual D2D1_POINT_2F GetCenter() const { return m_center; }
    virtual D2D1_POINT_2F GetGradientOriginOffset() const { return m_originOffset; }
    virtual float GetRadiusX() const { return m_radiusX; }
    virtual float GetRadiusY() const { return m_radiusY; }
protected:
    D2D1_POINT_2F m_center;
    D2D1_POINT_2F m_originOffset;
    float m_radiusX = 0.0f;
    float m_radiusY = 0.0f;
};

class IDWriteTextFormat : public IUnknownCompat {
public:
    std::wstring fontFamily;
    float fontSize = 12.0f;
    DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_REGULAR;
    DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
    DWRITE_TEXT_ALIGNMENT textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING;
    DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;

    void SetTextAlignment(DWRITE_TEXT_ALIGNMENT align) { textAlignment = align; }
    void SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT align) { paragraphAlignment = align; }
    DWRITE_TEXT_ALIGNMENT GetTextAlignment() const { return textAlignment; }
    DWRITE_PARAGRAPH_ALIGNMENT GetParagraphAlignment() const { return paragraphAlignment; }
};

class ID2D1GeometrySink : public IUnknownCompat {
public:
    virtual void BeginFigure(D2D1_POINT_2F startPoint, D2D1_FIGURE_BEGIN figureBegin) = 0;
    virtual void AddLine(D2D1_POINT_2F point) = 0;
    virtual void AddBezier(const D2D1_BEZIER_SEGMENT& bezier) = 0;
    virtual void AddArc(const D2D1_ARC_SEGMENT& arc) = 0;
    virtual void EndFigure(D2D1_FIGURE_END figureEnd) = 0;
    virtual void Close() = 0;
};

class ID2D1Geometry : public ID2D1Resource {};

class ID2D1PathGeometry : public ID2D1Geometry {
public:
    virtual bool Open(ID2D1GeometrySink** ppGeometrySink) = 0;
};

class ID2D1RenderTarget : public ID2D1Resource {
public:
    virtual void BeginDraw() {}
    virtual void EndDraw() {}
    virtual void Clear(const D2D1_COLOR_F& clearColor) = 0;
    virtual void DrawLine(D2D1_POINT_2F p0, D2D1_POINT_2F p1, ID2D1Brush* brush, float strokeWidth = 1.0f, ID2D1StrokeStyle* strokeStyle = nullptr) = 0;
    virtual void DrawRectangle(const D2D1_RECT_F& rect, ID2D1Brush* brush, float strokeWidth = 1.0f, ID2D1StrokeStyle* strokeStyle = nullptr) = 0;
    virtual void FillRectangle(const D2D1_RECT_F& rect, ID2D1Brush* brush) = 0;
    virtual void DrawRoundedRectangle(const D2D1_ROUNDED_RECT& roundedRect, ID2D1Brush* brush, float strokeWidth = 1.0f, ID2D1StrokeStyle* strokeStyle = nullptr) = 0;
    virtual void FillRoundedRectangle(const D2D1_ROUNDED_RECT& roundedRect, ID2D1Brush* brush) = 0;
    virtual void DrawEllipse(const D2D1_ELLIPSE& ellipse, ID2D1Brush* brush, float strokeWidth = 1.0f, ID2D1StrokeStyle* strokeStyle = nullptr) = 0;
    virtual void FillEllipse(const D2D1_ELLIPSE& ellipse, ID2D1Brush* brush) = 0;
    virtual void DrawGeometry(ID2D1Geometry* geometry, ID2D1Brush* brush, float strokeWidth = 1.0f, ID2D1StrokeStyle* strokeStyle = nullptr) = 0;
    virtual void FillGeometry(ID2D1Geometry* geometry, ID2D1Brush* brush, ID2D1Brush* opacityBrush = nullptr) = 0;
    virtual void DrawText(const wchar_t* string, uint32_t stringLength, IDWriteTextFormat* textFormat, const D2D1_RECT_F& layoutRect, ID2D1Brush* defaultFillBrush, D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE) = 0;
    inline void DrawTextW(const wchar_t* string, uint32_t stringLength, IDWriteTextFormat* textFormat, const D2D1_RECT_F& layoutRect, ID2D1Brush* defaultFillBrush, D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE) {
        DrawText(string, stringLength, textFormat, layoutRect, defaultFillBrush, options);
    }
    virtual void SetTransform(const D2D1_MATRIX_3X2_F& transform) { m_transform = transform; }
    virtual void GetTransform(D2D1_MATRIX_3X2_F* transform) const { if (transform) *transform = m_transform; }
    virtual void PushAxisAlignedClip(const D2D1_RECT_F& clipRect, D2D1_ANTIALIAS_MODE antialiasMode) {}
    virtual void PopAxisAlignedClip() {}
    virtual void CreateSolidColorBrush(const D2D1_COLOR_F& color, ID2D1SolidColorBrush** ppBrush) = 0;
    virtual void CreateGradientStopCollection(const D2D1_GRADIENT_STOP* gradientStops, uint32_t gradientStopsCount, ID2D1GradientStopCollection** ppGradientStopCollection) = 0;
    virtual void CreateLinearGradientBrush(const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES& linearGradientBrushProperties, ID2D1GradientStopCollection* gradientStopCollection, ID2D1LinearGradientBrush** ppLinearGradientBrush) = 0;
    virtual void CreateRadialGradientBrush(const D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES& radialGradientBrushProperties, ID2D1GradientStopCollection* gradientStopCollection, ID2D1RadialGradientBrush** ppRadialGradientBrush) = 0;

protected:
    D2D1_MATRIX_3X2_F m_transform = D2D1::Matrix3x2F::Identity();
};

class ID2D1Factory : public IUnknownCompat {
public:
    virtual bool CreatePathGeometry(ID2D1PathGeometry** ppPathGeometry) = 0;
    virtual bool CreateStrokeStyle(const void* props, const float* dashes, uint32_t dashesCount, ID2D1StrokeStyle** ppStrokeStyle) = 0;
};

class IDWriteFactory : public IUnknownCompat {
public:
    virtual bool CreateTextFormat(
        const wchar_t* fontFamilyName,
        void* fontCollection,
        DWRITE_FONT_WEIGHT fontWeight,
        DWRITE_FONT_STYLE fontStyle,
        int fontStretch,
        float fontSize,
        const wchar_t* localeName,
        IDWriteTextFormat** ppTextFormat
    ) = 0;
};

#endif
