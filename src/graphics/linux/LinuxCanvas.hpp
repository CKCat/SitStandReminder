#pragma once

#include "../D2DCompat.hpp"
#include <vector>
#include <string>
#include <memory>

class LinuxBrush : public ID2D1SolidColorBrush {
public:
    explicit LinuxBrush(const D2D1_COLOR_F& color) { m_color = color; }
};

class LinuxGradientStopCollection : public ID2D1GradientStopCollection {
public:
    explicit LinuxGradientStopCollection(const D2D1_GRADIENT_STOP* pStops, uint32_t count) {
        if (pStops && count > 0) {
            stops.assign(pStops, pStops + count);
        }
    }
};

class LinuxLinearGradientBrush : public ID2D1LinearGradientBrush {
public:
    LinuxLinearGradientBrush(const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES& props, ID2D1GradientStopCollection* pStops) {
        m_start = props.startPoint;
        m_end = props.endPoint;
        if (pStops) {
            m_stops = pStops->stops;
            std::sort(m_stops.begin(), m_stops.end(), [](const auto& a, const auto& b) {
                return a.position < b.position;
            });
        }
    }
    D2D1_COLOR_F GetPrimaryColor() const {
        if (!m_stops.empty()) {
            return m_stops[0].color;
        }
        return D2D1_COLOR_F{ 0.5f, 0.5f, 0.5f, 1.0f };
    }
    D2D1_COLOR_F SampleColor(float x, float y) const {
        if (m_stops.empty()) return D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f };
        float dx = m_end.x - m_start.x;
        float dy = m_end.y - m_start.y;
        float lenSq = dx * dx + dy * dy;
        float t = 0.0f;
        if (lenSq > 1e-6f) {
            t = ((x - m_start.x) * dx + (y - m_start.y) * dy) / lenSq;
        }
        t = std::clamp(t, 0.0f, 1.0f);
        return SampleStops(t);
    }
private:
    D2D1_COLOR_F SampleStops(float t) const {
        if (m_stops.size() == 1 || t <= m_stops.front().position) return m_stops.front().color;
        if (t >= m_stops.back().position) return m_stops.back().color;
        for (size_t i = 0; i + 1 < m_stops.size(); ++i) {
            if (t >= m_stops[i].position && t <= m_stops[i + 1].position) {
                float span = m_stops[i + 1].position - m_stops[i].position;
                float u = (span > 1e-5f) ? (t - m_stops[i].position) / span : 0.0f;
                u = std::clamp(u, 0.0f, 1.0f);
                return D2D1_COLOR_F{
                    m_stops[i].color.r + u * (m_stops[i + 1].color.r - m_stops[i].color.r),
                    m_stops[i].color.g + u * (m_stops[i + 1].color.g - m_stops[i].color.g),
                    m_stops[i].color.b + u * (m_stops[i + 1].color.b - m_stops[i].color.b),
                    m_stops[i].color.a + u * (m_stops[i + 1].color.a - m_stops[i].color.a)
                };
            }
        }
        return m_stops.back().color;
    }
    std::vector<D2D1_GRADIENT_STOP> m_stops;
};

class LinuxRadialGradientBrush : public ID2D1RadialGradientBrush {
public:
    LinuxRadialGradientBrush(const D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES& props, ID2D1GradientStopCollection* pStops) {
        m_center = props.center;
        m_originOffset = props.gradientOriginOffset;
        m_radiusX = props.radiusX;
        m_radiusY = props.radiusY;
        if (pStops) {
            m_stops = pStops->stops;
            std::sort(m_stops.begin(), m_stops.end(), [](const auto& a, const auto& b) {
                return a.position < b.position;
            });
        }
    }
    D2D1_COLOR_F GetPrimaryColor() const {
        if (!m_stops.empty()) {
            return m_stops[0].color;
        }
        return D2D1_COLOR_F{ 0.5f, 0.5f, 0.5f, 1.0f };
    }
    D2D1_COLOR_F SampleColor(float x, float y) const {
        if (m_stops.empty()) return D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f };
        float rx = (std::max)(1e-4f, m_radiusX);
        float ry = (std::max)(1e-4f, m_radiusY);
        float nx = (x - m_center.x - m_originOffset.x) / rx;
        float ny = (y - m_center.y - m_originOffset.y) / ry;
        float t = std::clamp(std::hypot(nx, ny), 0.0f, 1.0f);
        return SampleStops(t);
    }
private:
    D2D1_COLOR_F SampleStops(float t) const {
        if (m_stops.size() == 1 || t <= m_stops.front().position) return m_stops.front().color;
        if (t >= m_stops.back().position) return m_stops.back().color;
        for (size_t i = 0; i + 1 < m_stops.size(); ++i) {
            if (t >= m_stops[i].position && t <= m_stops[i + 1].position) {
                float span = m_stops[i + 1].position - m_stops[i].position;
                float u = (span > 1e-5f) ? (t - m_stops[i].position) / span : 0.0f;
                u = std::clamp(u, 0.0f, 1.0f);
                return D2D1_COLOR_F{
                    m_stops[i].color.r + u * (m_stops[i + 1].color.r - m_stops[i].color.r),
                    m_stops[i].color.g + u * (m_stops[i + 1].color.g - m_stops[i].color.g),
                    m_stops[i].color.b + u * (m_stops[i + 1].color.b - m_stops[i].color.b),
                    m_stops[i].color.a + u * (m_stops[i + 1].color.a - m_stops[i].color.a)
                };
            }
        }
        return m_stops.back().color;
    }
    std::vector<D2D1_GRADIENT_STOP> m_stops;
};

class LinuxStrokeStyle : public ID2D1StrokeStyle {};

class LinuxPathGeometry;

class LinuxGeometrySink : public ID2D1GeometrySink {
public:
    struct Figure {
        D2D1_POINT_2F startPoint;
        D2D1_FIGURE_BEGIN begin = D2D1_FIGURE_BEGIN_FILLED;
        D2D1_FIGURE_END end = D2D1_FIGURE_END_OPEN;
        std::vector<D2D1_POINT_2F> points;
    };

    explicit LinuxGeometrySink(LinuxPathGeometry* pParent = nullptr) : m_pParent(pParent) {}

    std::vector<Figure> figures;

    void BeginFigure(D2D1_POINT_2F startPoint, D2D1_FIGURE_BEGIN figureBegin) override;
    void AddLine(D2D1_POINT_2F point) override;
    void AddBezier(const D2D1_BEZIER_SEGMENT& bezier) override;
    void AddArc(const D2D1_ARC_SEGMENT& arc) override;
    void EndFigure(D2D1_FIGURE_END figureEnd) override;
    void Close() override;

private:
    LinuxPathGeometry* m_pParent = nullptr;
};

class LinuxPathGeometry : public ID2D1PathGeometry {
public:
    std::vector<LinuxGeometrySink::Figure> figures;

    bool Open(ID2D1GeometrySink** ppGeometrySink) override;
};

class LinuxRenderTarget : public ID2D1RenderTarget {
public:
    LinuxRenderTarget(int width = 100, int height = 100);
    ~LinuxRenderTarget() override = default;

    void Resize(int width, int height);
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    uint32_t* GetPixels() { return m_pixels.data(); }
    const uint32_t* GetPixels() const { return m_pixels.data(); }

    void Clear(const D2D1_COLOR_F& clearColor) override;

    void DrawLine(D2D1_POINT_2F p0, D2D1_POINT_2F p1, ID2D1Brush* brush, float strokeWidth = 1.0f, ID2D1StrokeStyle* strokeStyle = nullptr) override;
    void DrawRectangle(const D2D1_RECT_F& rect, ID2D1Brush* brush, float strokeWidth = 1.0f, ID2D1StrokeStyle* strokeStyle = nullptr) override;
    void FillRectangle(const D2D1_RECT_F& rect, ID2D1Brush* brush) override;
    void DrawRoundedRectangle(const D2D1_ROUNDED_RECT& roundedRect, ID2D1Brush* brush, float strokeWidth = 1.0f, ID2D1StrokeStyle* strokeStyle = nullptr) override;
    void FillRoundedRectangle(const D2D1_ROUNDED_RECT& roundedRect, ID2D1Brush* brush) override;
    void DrawEllipse(const D2D1_ELLIPSE& ellipse, ID2D1Brush* brush, float strokeWidth = 1.0f, ID2D1StrokeStyle* strokeStyle = nullptr) override;
    void FillEllipse(const D2D1_ELLIPSE& ellipse, ID2D1Brush* brush) override;
    void DrawGeometry(ID2D1Geometry* geometry, ID2D1Brush* brush, float strokeWidth = 1.0f, ID2D1StrokeStyle* strokeStyle = nullptr) override;
    void FillGeometry(ID2D1Geometry* geometry, ID2D1Brush* brush, ID2D1Brush* opacityBrush = nullptr) override;
    void DrawText(const wchar_t* string, uint32_t stringLength, IDWriteTextFormat* textFormat, const D2D1_RECT_F& layoutRect, ID2D1Brush* defaultFillBrush, D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_NONE) override;

    void PushAxisAlignedClip(const D2D1_RECT_F& clipRect, D2D1_ANTIALIAS_MODE antialiasMode) override;
    void PopAxisAlignedClip() override;

    void CreateSolidColorBrush(const D2D1_COLOR_F& color, ID2D1SolidColorBrush** ppBrush) override;
    void CreateGradientStopCollection(const D2D1_GRADIENT_STOP* gradientStops, uint32_t gradientStopsCount, ID2D1GradientStopCollection** ppGradientStopCollection) override;
    void CreateLinearGradientBrush(const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES& linearGradientBrushProperties, ID2D1GradientStopCollection* gradientStopCollection, ID2D1LinearGradientBrush** ppLinearGradientBrush) override;
    void CreateRadialGradientBrush(const D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES& radialGradientBrushProperties, ID2D1GradientStopCollection* gradientStopCollection, ID2D1RadialGradientBrush** ppRadialGradientBrush) override;

    D2D1_POINT_2F TransformPoint(D2D1_POINT_2F p) const;
    D2D1_POINT_2F InverseTransformPoint(D2D1_POINT_2F p) const;
    float GetTransformScale() const;

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<uint32_t> m_pixels;
    D2D1_RECT_F m_currentClip;
    std::vector<D2D1_RECT_F> m_clipStack;

    void BlendPixel(int x, int y, uint32_t premulColor, float coverage = 1.0f);
    void BlendPixelPremul(int x, int y, uint32_t premulColor);
};
