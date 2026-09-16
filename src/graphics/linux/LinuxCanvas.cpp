#include "LinuxCanvas.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <fontconfig/fontconfig.h>
#include <iostream>
#include <unordered_map>

// 辅助颜色转换与 Premultiplied Alpha 计算
static inline uint32_t ColorToPremul(const D2D1_COLOR_F& c, float extraAlpha = 1.0f) {
    float a = std::clamp(c.a * extraAlpha, 0.0f, 1.0f);
    float r = std::clamp(c.r, 0.0f, 1.0f);
    float g = std::clamp(c.g, 0.0f, 1.0f);
    float b = std::clamp(c.b, 0.0f, 1.0f);

    uint32_t aByte = static_cast<uint32_t>(a * 255.0f + 0.5f);
    uint32_t rByte = static_cast<uint32_t>(r * a * 255.0f + 0.5f);
    uint32_t gByte = static_cast<uint32_t>(g * a * 255.0f + 0.5f);
    uint32_t bByte = static_cast<uint32_t>(b * a * 255.0f + 0.5f);

    return (aByte << 24) | (rByte << 16) | (gByte << 8) | bByte;
}

static inline D2D1_COLOR_F GetBrushColor(ID2D1Brush* brush) {
    if (!brush) return D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f };
    if (auto* solid = dynamic_cast<ID2D1SolidColorBrush*>(brush)) {
        return solid->GetColor();
    }
    if (auto* grad = dynamic_cast<LinuxLinearGradientBrush*>(brush)) {
        return grad->GetPrimaryColor();
    }
    if (auto* rad = dynamic_cast<LinuxRadialGradientBrush*>(brush)) {
        return rad->GetPrimaryColor();
    }
    return D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f };
}

static inline D2D1_COLOR_F SampleBrushColor(ID2D1Brush* brush, float localX, float localY) {
    if (!brush) return D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f };
    if (auto* solid = dynamic_cast<ID2D1SolidColorBrush*>(brush)) {
        return solid->GetColor();
    }
    if (auto* lin = dynamic_cast<LinuxLinearGradientBrush*>(brush)) {
        return lin->SampleColor(localX, localY);
    }
    if (auto* rad = dynamic_cast<LinuxRadialGradientBrush*>(brush)) {
        return rad->SampleColor(localX, localY);
    }
    return GetBrushColor(brush);
}

static inline void BlendPremul(uint32_t& dst, uint32_t src) {
    uint32_t srcA = (src >> 24) & 0xFF;
    if (srcA == 0) return;
    if (srcA == 255) {
        dst = src;
        return;
    }
    uint32_t invA = 255 - srcA;

    uint32_t dstA = (dst >> 24) & 0xFF;
    uint32_t dstR = (dst >> 16) & 0xFF;
    uint32_t dstG = (dst >> 8) & 0xFF;
    uint32_t dstB = dst & 0xFF;

    uint32_t srcR = (src >> 16) & 0xFF;
    uint32_t srcG = (src >> 8) & 0xFF;
    uint32_t srcB = src & 0xFF;

    uint32_t outA = srcA + ((dstA * invA + 127) / 255);
    uint32_t outR = srcR + ((dstR * invA + 127) / 255);
    uint32_t outG = srcG + ((dstG * invA + 127) / 255);
    uint32_t outB = srcB + ((dstB * invA + 127) / 255);

    if (outA > 255) outA = 255;
    if (outR > 255) outR = 255;
    if (outG > 255) outG = 255;
    if (outB > 255) outB = 255;

    dst = (outA << 24) | (outR << 16) | (outG << 8) | outB;
}

// ---------------- LinuxGeometrySink & LinuxPathGeometry ----------------

void LinuxGeometrySink::BeginFigure(D2D1_POINT_2F startPoint, D2D1_FIGURE_BEGIN figureBegin) {
    figures.push_back(Figure{});
    figures.back().startPoint = startPoint;
    figures.back().begin = figureBegin;
    figures.back().points.push_back(startPoint);
}

void LinuxGeometrySink::AddLine(D2D1_POINT_2F point) {
    if (!figures.empty()) {
        figures.back().points.push_back(point);
    }
}

void LinuxGeometrySink::AddBezier(const D2D1_BEZIER_SEGMENT& bezier) {
    if (figures.empty()) return;
    auto& pts = figures.back().points;
    D2D1_POINT_2F p0 = pts.empty() ? figures.back().startPoint : pts.back();

    // 贝塞尔曲线等分细分采样 (16 阶精细多边形化)
    constexpr int STEPS = 16;
    for (int i = 1; i <= STEPS; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(STEPS);
        float u = 1.0f - t;
        float tt = t * t;
        float uu = u * u;
        float uuu = uu * u;
        float ttt = tt * t;

        float x = uuu * p0.x + 3.0f * uu * t * bezier.point1.x + 3.0f * u * tt * bezier.point2.x + ttt * bezier.point3.x;
        float y = uuu * p0.y + 3.0f * uu * t * bezier.point1.y + 3.0f * u * tt * bezier.point2.y + ttt * bezier.point3.y;
        pts.push_back(D2D1_POINT_2F{ x, y });
    }
}

void LinuxGeometrySink::AddArc(const D2D1_ARC_SEGMENT& arc) {
    if (figures.empty()) return;
    auto& pts = figures.back().points;
    D2D1_POINT_2F p0 = pts.empty() ? figures.back().startPoint : pts.back();
    D2D1_POINT_2F p1 = arc.point;

    float rx = std::abs(arc.size.width);
    float ry = std::abs(arc.size.height);

    // 边界退化处理：半径过小或起点终点重合
    float dx = p0.x - p1.x;
    float dy = p0.y - p1.y;
    float dist = std::hypot(dx, dy);
    if (rx < 1e-4f || ry < 1e-4f || dist < 1e-4f) {
        pts.push_back(p1);
        return;
    }

    constexpr float PI = 3.14159265358979323846f;
    float phi = arc.rotationAngle * PI / 180.0f;
    float cosPhi = std::cos(phi);
    float sinPhi = std::sin(phi);

    // 1. 旋转到未倾斜局部坐标系
    float dx2 = (p0.x - p1.x) * 0.5f;
    float dy2 = (p0.y - p1.y) * 0.5f;
    float x1Prime = cosPhi * dx2 + sinPhi * dy2;
    float y1Prime = -sinPhi * dx2 + cosPhi * dy2;

    // 2. 检验并自适应放大半轴
    float rxSq = rx * rx;
    float rySq = ry * ry;
    float x1PrimeSq = x1Prime * x1Prime;
    float y1PrimeSq = y1Prime * y1Prime;

    float radiiCheck = x1PrimeSq / rxSq + y1PrimeSq / rySq;
    if (radiiCheck > 1.0f) {
        float scale = std::sqrt(radiiCheck);
        rx *= scale;
        ry *= scale;
        rxSq = rx * rx;
        rySq = ry * ry;
    }

    // 3. 计算未倾斜局部坐标系下的椭圆中心 (cxPrime, cyPrime)
    float numerator = rxSq * rySq - rxSq * y1PrimeSq - rySq * x1PrimeSq;
    float denominator = rxSq * y1PrimeSq + rySq * x1PrimeSq;
    float sq = (denominator > 1e-6f) ? std::sqrt((std::max)(0.0f, numerator / denominator)) : 0.0f;

    bool isLargeArc = (arc.arcSize == D2D1_ARC_SIZE_LARGE);
    bool isClockwise = (arc.sweepDirection == D2D1_SWEEP_DIRECTION_CLOCKWISE);
    if (isLargeArc == isClockwise) {
        sq = -sq;
    }

    float cxPrime = sq * (rx * y1Prime / ry);
    float cyPrime = sq * -(ry * x1Prime / rx);

    // 4. 旋转变换回绝对坐标系得到圆心 (cx, cy)
    float cx = cosPhi * cxPrime - sinPhi * cyPrime + (p0.x + p1.x) * 0.5f;
    float cy = sinPhi * cxPrime + cosPhi * cyPrime + (p0.y + p1.y) * 0.5f;

    // 5. 计算起始角与扫描角跨度
    auto angleBetween = [](float ux, float uy, float vx, float vy) -> float {
        float dot = ux * vx + uy * vy;
        float det = ux * vy - uy * vx;
        return std::atan2(det, dot);
    };

    float v1x = (x1Prime - cxPrime) / rx;
    float v1y = (y1Prime - cyPrime) / ry;
    float v2x = (-x1Prime - cxPrime) / rx;
    float v2y = (-y1Prime - cyPrime) / ry;

    float theta1 = std::atan2(v1y, v1x);
    float dTheta = angleBetween(v1x, v1y, v2x, v2y);

    if (!isClockwise && dTheta > 0.0f) {
        dTheta -= 2.0f * PI;
    } else if (isClockwise && dTheta < 0.0f) {
        dTheta += 2.0f * PI;
    }

    // 6. 根据扫掠跨度执行平滑细分采样
    int segments = (std::max)(12, static_cast<int>(std::ceil(std::abs(dTheta) / (PI / 18.0f))));
    for (int i = 1; i <= segments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        float ang = theta1 + t * dTheta;
        float ex = rx * std::cos(ang);
        float ey = ry * std::sin(ang);
        float px = cosPhi * ex - sinPhi * ey + cx;
        float py = sinPhi * ex + cosPhi * ey + cy;
        pts.push_back(D2D1_POINT_2F{ px, py });
    }
}

void LinuxGeometrySink::EndFigure(D2D1_FIGURE_END figureEnd) {
    if (!figures.empty()) {
        figures.back().end = figureEnd;
        if (figureEnd == D2D1_FIGURE_END_CLOSED) {
            figures.back().points.push_back(figures.back().startPoint);
        }
    }
}

void LinuxGeometrySink::Close() {
    if (m_pParent) {
        m_pParent->figures = std::move(figures);
    }
}

bool LinuxPathGeometry::Open(ID2D1GeometrySink** ppGeometrySink) {
    if (!ppGeometrySink) return false;
    auto* sink = new LinuxGeometrySink(this);
    *ppGeometrySink = sink;
    return true;
}

// ---------------- FreeType 字体渲染引擎 ----------------

static std::string WStringToUtf8(const std::wstring& wstr) {
    std::string utf8;
    for (wchar_t wc : wstr) {
        uint32_t cp = static_cast<uint32_t>(wc);
        if (cp < 0x80) {
            utf8.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            utf8.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
            utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            utf8.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
            utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            utf8.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
            utf8.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return utf8;
}

class FreeTypeFontEngine {
public:
    static FreeTypeFontEngine& Instance() {
        static FreeTypeFontEngine inst;
        return inst;
    }

    struct ResolvedFont {
        std::string path;
        int index = 0;
    };

    FT_Face GetFace(const std::wstring& family, float size, DWRITE_FONT_WEIGHT weight) {
        ResolvedFont font = ResolveFont(family, weight, false);
        return LoadFace(font, size);
    }

    FT_Face GetCjkFallbackFace(float size, DWRITE_FONT_WEIGHT weight) {
        ResolvedFont font = ResolveFont(L"Noto Sans CJK SC", weight, true);
        return LoadFace(font, size);
    }

private:
    FreeTypeFontEngine() {
        FT_Init_FreeType(&m_library);
        m_fcConfig = FcInitLoadConfigAndFonts();
    }

    ~FreeTypeFontEngine() {
        for (auto& pair : m_faceCache) {
            FT_Done_Face(pair.second);
        }
        if (m_library) FT_Done_FreeType(m_library);
        if (m_fcConfig) FcConfigDestroy(m_fcConfig);
    }

    FT_Face LoadFace(const ResolvedFont& font, float size) {
        if (font.path.empty() || !m_library) return nullptr;
        std::string key = font.path + "#" + std::to_string(font.index) + "@" + std::to_string(static_cast<int>(size + 0.5f));
        auto it = m_faceCache.find(key);
        FT_Face face = nullptr;
        if (it != m_faceCache.end()) {
            face = it->second;
        } else {
            if (FT_New_Face(m_library, font.path.c_str(), font.index, &face) == 0) {
                m_faceCache[key] = face;
            } else {
                return nullptr;
            }
        }
        if (face) {
            FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(size));
        }
        return face;
    }

    ResolvedFont ResolveFont(const std::wstring& family, DWRITE_FONT_WEIGHT weight, bool forceCjk) {
        if (!m_fcConfig) return {};

        std::string famUtf8 = WStringToUtf8(family);
        std::string cacheKey = std::to_string(static_cast<int>(weight)) + ":" + (forceCjk ? "CJK" : "NONCJK") + ":" + famUtf8;
        auto itCache = m_resolvedFontCache.find(cacheKey);
        if (itCache != m_resolvedFontCache.end()) {
            return itCache->second;
        }

        FcPattern* pat = FcPatternCreate();
        if (forceCjk) {
            FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)"Noto Sans CJK SC");
            FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)"Droid Sans Fallback");
            FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)"WenQuanYi Micro Hei");
            FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)"sans-serif");
            FcPatternAddString(pat, FC_LANG, (const FcChar8*)"zh-cn");
        } else {
            if (!famUtf8.empty() && famUtf8 != "sans-serif") {
                FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)famUtf8.c_str());
            }
            FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)"Noto Sans CJK SC");
            FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)"Droid Sans Fallback");
            FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)"sans-serif");
            FcPatternAddString(pat, FC_LANG, (const FcChar8*)"zh-cn");
        }
        if (weight >= DWRITE_FONT_WEIGHT_BOLD) {
            FcPatternAddInteger(pat, FC_WEIGHT, FC_WEIGHT_BOLD);
        } else if (weight >= DWRITE_FONT_WEIGHT_SEMI_BOLD) {
            FcPatternAddInteger(pat, FC_WEIGHT, FC_WEIGHT_DEMIBOLD);
        } else {
            FcPatternAddInteger(pat, FC_WEIGHT, FC_WEIGHT_REGULAR);
        }

        FcConfigSubstitute(m_fcConfig, pat, FcMatchPattern);
        FcDefaultSubstitute(pat);

        FcResult result;
        FcPattern* font = FcFontMatch(m_fcConfig, pat, &result);
        ResolvedFont res;
        if (font) {
            FcChar8* file = nullptr;
            if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch && file) {
                res.path = (const char*)file;
            }
            int idx = 0;
            if (FcPatternGetInteger(font, FC_INDEX, 0, &idx) == FcResultMatch) {
                res.index = idx;
            }
            FcPatternDestroy(font);
        }
        FcPatternDestroy(pat);
        m_resolvedFontCache[cacheKey] = res;
        return res;
    }

    FT_Library m_library = nullptr;
    FcConfig* m_fcConfig = nullptr;
    std::unordered_map<std::string, FT_Face> m_faceCache;
    std::unordered_map<std::string, ResolvedFont> m_resolvedFontCache;
};

// ---------------- LinuxRenderTarget 实现 ----------------

LinuxRenderTarget::LinuxRenderTarget(int width, int height) {
    Resize(width, height);
}

void LinuxRenderTarget::Resize(int width, int height) {
    m_width = (std::max)(1, width);
    m_height = (std::max)(1, height);
    m_pixels.assign(m_width * m_height, 0);
    m_currentClip = D2D1::RectF(0, 0, static_cast<float>(m_width), static_cast<float>(m_height));
    m_clipStack.clear();
}

void LinuxRenderTarget::Clear(const D2D1_COLOR_F& clearColor) {
    uint32_t val = ColorToPremul(clearColor);
    std::fill(m_pixels.begin(), m_pixels.end(), val);
}

D2D1_POINT_2F LinuxRenderTarget::TransformPoint(D2D1_POINT_2F p) const {
    return D2D1_POINT_2F{
        p.x * m_transform._11 + p.y * m_transform._21 + m_transform._31,
        p.x * m_transform._12 + p.y * m_transform._22 + m_transform._32
    };
}

D2D1_POINT_2F LinuxRenderTarget::InverseTransformPoint(D2D1_POINT_2F p) const {
    float det = m_transform._11 * m_transform._22 - m_transform._12 * m_transform._21;
    if (std::abs(det) < 1e-6f) {
        return p;
    }
    float dx = p.x - m_transform._31;
    float dy = p.y - m_transform._32;
    float x = (dx * m_transform._22 - dy * m_transform._21) / det;
    float y = (-dx * m_transform._12 + dy * m_transform._11) / det;
    return D2D1_POINT_2F{ x, y };
}

float LinuxRenderTarget::GetTransformScale() const {
    float det = m_transform._11 * m_transform._22 - m_transform._12 * m_transform._21;
    return (std::abs(det) > 1e-6f) ? std::sqrt(std::abs(det)) : 1.0f;
}

void LinuxRenderTarget::BlendPixel(int x, int y, uint32_t premulColor, float coverage) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
    if (x < m_currentClip.left || x >= m_currentClip.right || y < m_currentClip.top || y >= m_currentClip.bottom) return;

    if (coverage <= 0.0f) return;
    if (coverage < 1.0f) {
        uint32_t a = (premulColor >> 24) & 0xFF;
        uint32_t r = (premulColor >> 16) & 0xFF;
        uint32_t g = (premulColor >> 8) & 0xFF;
        uint32_t b = premulColor & 0xFF;

        a = static_cast<uint32_t>(a * coverage + 0.5f);
        r = static_cast<uint32_t>(r * coverage + 0.5f);
        g = static_cast<uint32_t>(g * coverage + 0.5f);
        b = static_cast<uint32_t>(b * coverage + 0.5f);
        premulColor = (a << 24) | (r << 16) | (g << 8) | b;
    }

    BlendPremul(m_pixels[y * m_width + x], premulColor);
}

void LinuxRenderTarget::BlendPixelPremul(int x, int y, uint32_t premulColor) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
    if (x < m_currentClip.left || x >= m_currentClip.right || y < m_currentClip.top || y >= m_currentClip.bottom) return;
    BlendPremul(m_pixels[y * m_width + x], premulColor);
}

void LinuxRenderTarget::CreateSolidColorBrush(const D2D1_COLOR_F& color, ID2D1SolidColorBrush** ppBrush) {
    if (ppBrush) {
        *ppBrush = new LinuxBrush(color);
    }
}

void LinuxRenderTarget::PushAxisAlignedClip(const D2D1_RECT_F& clipRect, D2D1_ANTIALIAS_MODE /*antialiasMode*/) {
    m_clipStack.push_back(m_currentClip);
    D2D1_POINT_2F tl = TransformPoint(D2D1::Point2F(clipRect.left, clipRect.top));
    D2D1_POINT_2F br = TransformPoint(D2D1::Point2F(clipRect.right, clipRect.bottom));

    D2D1_RECT_F transformedClip = D2D1::RectF(
        (std::min)(tl.x, br.x), (std::min)(tl.y, br.y),
        (std::max)(tl.x, br.x), (std::max)(tl.y, br.y)
    );

    m_currentClip.left   = (std::max)(m_currentClip.left, transformedClip.left);
    m_currentClip.top    = (std::max)(m_currentClip.top, transformedClip.top);
    m_currentClip.right  = (std::min)(m_currentClip.right, transformedClip.right);
    m_currentClip.bottom = (std::min)(m_currentClip.bottom, transformedClip.bottom);
}

void LinuxRenderTarget::PopAxisAlignedClip() {
    if (!m_clipStack.empty()) {
        m_currentClip = m_clipStack.back();
        m_clipStack.pop_back();
    }
}

// 绘制抗锯齿直线段 (沿主轴高速步进抗锯齿光栅化)
void LinuxRenderTarget::DrawLine(D2D1_POINT_2F p0, D2D1_POINT_2F p1, ID2D1Brush* brush, float strokeWidth, ID2D1StrokeStyle* /*strokeStyle*/) {
    if (!brush) return;
    auto* solidBrush = dynamic_cast<ID2D1SolidColorBrush*>(brush);
    if (!solidBrush) return;
    uint32_t color = ColorToPremul(solidBrush->GetColor());

    D2D1_POINT_2F tp0 = TransformPoint(p0);
    D2D1_POINT_2F tp1 = TransformPoint(p1);

    float dx = tp1.x - tp0.x;
    float dy = tp1.y - tp0.y;
    float lenSq = dx * dx + dy * dy;
    if (lenSq < 0.0001f) return;

    float scale = GetTransformScale();
    float effectiveStrokeWidth = strokeWidth * scale;
    float halfW = (std::max)(0.5f, effectiveStrokeWidth * 0.5f);
    float radius = halfW + 0.5f;
    float invLenSq = 1.0f / lenSq;

    // 粗粒度快速视锥裁剪
    float minX = (std::min)(tp0.x, tp1.x) - radius;
    float maxX = (std::max)(tp0.x, tp1.x) + radius;
    float minY = (std::min)(tp0.y, tp1.y) - radius;
    float maxY = (std::max)(tp0.y, tp1.y) + radius;
    if (maxX < m_currentClip.left || minX >= m_currentClip.right ||
        maxY < m_currentClip.top || minY >= m_currentClip.bottom) {
        return;
    }

    int clipLeft = static_cast<int>(m_currentClip.left);
    int clipRight = static_cast<int>(m_currentClip.right) - 1;
    int clipTop = static_cast<int>(m_currentClip.top);
    int clipBottom = static_cast<int>(m_currentClip.bottom) - 1;

    // 沿主轴步进，消除整个 AABB 的 O(W*H) 盲目像素遍历
    if (std::abs(dx) >= std::abs(dy)) {
        if (tp0.x > tp1.x) {
            std::swap(tp0, tp1);
            dx = -dx;
            dy = -dy;
        }
        int startX = (std::max)(clipLeft, static_cast<int>(std::floor(tp0.x - radius)));
        int endX = (std::min)(clipRight, static_cast<int>(std::ceil(tp1.x + radius)));
        float slope = dy / dx;

        for (int x = startX; x <= endX; ++x) {
            float px = static_cast<float>(x) + 0.5f;
            float midY = tp0.y + (px - tp0.x) * slope;
            int startY = (std::max)(clipTop, static_cast<int>(std::floor(midY - radius - 0.5f)));
            int endY = (std::min)(clipBottom, static_cast<int>(std::ceil(midY + radius + 0.5f)));

            for (int y = startY; y <= endY; ++y) {
                float py = static_cast<float>(y) + 0.5f;
                float t = std::clamp(((px - tp0.x) * dx + (py - tp0.y) * dy) * invLenSq, 0.0f, 1.0f);
                float projX = tp0.x + t * dx;
                float projY = tp0.y + t * dy;
                float distSq = (px - projX) * (px - projX) + (py - projY) * (py - projY);
                if (distSq < radius * radius) {
                    float dist = std::sqrt(distSq);
                    float d = dist - halfW;
                    if (d < 0.5f) {
                        float cov = std::clamp(0.5f - d, 0.0f, 1.0f);
                        BlendPixel(x, y, color, cov);
                    }
                }
            }
        }
    } else {
        if (tp0.y > tp1.y) {
            std::swap(tp0, tp1);
            dx = -dx;
            dy = -dy;
        }
        int startY = (std::max)(clipTop, static_cast<int>(std::floor(tp0.y - radius)));
        int endY = (std::min)(clipBottom, static_cast<int>(std::ceil(tp1.y + radius)));
        float invSlope = dx / dy;

        for (int y = startY; y <= endY; ++y) {
            float py = static_cast<float>(y) + 0.5f;
            float midX = tp0.x + (py - tp0.y) * invSlope;
            int startX = (std::max)(clipLeft, static_cast<int>(std::floor(midX - radius - 0.5f)));
            int endX = (std::min)(clipRight, static_cast<int>(std::ceil(midX + radius + 0.5f)));

            for (int x = startX; x <= endX; ++x) {
                float px = static_cast<float>(x) + 0.5f;
                float t = std::clamp(((px - tp0.x) * dx + (py - tp0.y) * dy) * invLenSq, 0.0f, 1.0f);
                float projX = tp0.x + t * dx;
                float projY = tp0.y + t * dy;
                float distSq = (px - projX) * (px - projX) + (py - projY) * (py - projY);
                if (distSq < radius * radius) {
                    float dist = std::sqrt(distSq);
                    float d = dist - halfW;
                    if (d < 0.5f) {
                        float cov = std::clamp(0.5f - d, 0.0f, 1.0f);
                        BlendPixel(x, y, color, cov);
                    }
                }
            }
        }
    }
}

// 矩形填充 (支持任意仿射变换/旋转，结合 SDF 次像素抗锯齿并严格遵从 Clip 限制)
void LinuxRenderTarget::FillRectangle(const D2D1_RECT_F& rect, ID2D1Brush* brush) {
    if (!brush) return;
    auto* solidBrush = dynamic_cast<ID2D1SolidColorBrush*>(brush);
    bool isSolid = (solidBrush != nullptr);
    uint32_t solidColor = isSolid ? ColorToPremul(solidBrush->GetColor()) : 0;
    bool isOpaque = isSolid && ((solidColor >> 24) == 255);

    float cx = (rect.left + rect.right) * 0.5f;
    float cy = (rect.top + rect.bottom) * 0.5f;
    float halfW = (rect.right - rect.left) * 0.5f;
    float halfH = (rect.bottom - rect.top) * 0.5f;

    // 变换矩形的 4 个顶点，计算其真实的屏幕包围盒 AABB
    D2D1_POINT_2F p0 = TransformPoint(D2D1::Point2F(rect.left, rect.top));
    D2D1_POINT_2F p1 = TransformPoint(D2D1::Point2F(rect.right, rect.top));
    D2D1_POINT_2F p2 = TransformPoint(D2D1::Point2F(rect.right, rect.bottom));
    D2D1_POINT_2F p3 = TransformPoint(D2D1::Point2F(rect.left, rect.bottom));

    float minX = (std::min)({ p0.x, p1.x, p2.x, p3.x });
    float maxX = (std::max)({ p0.x, p1.x, p2.x, p3.x });
    float minY = (std::min)({ p0.y, p1.y, p2.y, p3.y });
    float maxY = (std::max)({ p0.y, p1.y, p2.y, p3.y });

    int clipLeft = static_cast<int>(m_currentClip.left);
    int clipRight = static_cast<int>(m_currentClip.right) - 1;
    int clipTop = static_cast<int>(m_currentClip.top);
    int clipBottom = static_cast<int>(m_currentClip.bottom) - 1;

    int x0 = (std::max)(clipLeft, static_cast<int>(std::floor(minX - 1.0f)));
    int x1 = (std::min)(clipRight, static_cast<int>(std::ceil(maxX + 1.0f)));
    int y0 = (std::max)(clipTop, static_cast<int>(std::floor(minY - 1.0f)));
    int y1 = (std::min)(clipBottom, static_cast<int>(std::ceil(maxY + 1.0f)));
    if (x0 > x1 || y0 > y1) return;

    float scale = GetTransformScale();

    for (int y = y0; y <= y1; ++y) {
        float py = static_cast<float>(y) + 0.5f;
        uint32_t* rowPixels = &m_pixels[y * m_width];
        for (int x = x0; x <= x1; ++x) {
            float px = static_cast<float>(x) + 0.5f;
            D2D1_POINT_2F localPt = InverseTransformPoint(D2D1::Point2F(px, py));
            float qx = std::abs(localPt.x - cx) - halfW;
            float qy = std::abs(localPt.y - cy) - halfH;
            float dLocal = (std::min)((std::max)(qx, qy), 0.0f) + std::hypot((std::max)(qx, 0.0f), (std::max)(qy, 0.0f));
            float d = dLocal * scale;
            if (d < 0.5f) {
                float cov = std::clamp(0.5f - d, 0.0f, 1.0f);
                if (cov >= 1.0f && isOpaque) {
                    rowPixels[x] = solidColor;
                } else {
                    uint32_t pixelColor = isSolid ? solidColor : ColorToPremul(SampleBrushColor(brush, localPt.x, localPt.y));
                    BlendPixel(x, y, pixelColor, cov);
                }
            }
        }
    }
}

// 矩形描边
void LinuxRenderTarget::DrawRectangle(const D2D1_RECT_F& rect, ID2D1Brush* brush, float strokeWidth, ID2D1StrokeStyle* strokeStyle) {
    DrawLine(D2D1::Point2F(rect.left, rect.top), D2D1::Point2F(rect.right, rect.top), brush, strokeWidth, strokeStyle);
    DrawLine(D2D1::Point2F(rect.right, rect.top), D2D1::Point2F(rect.right, rect.bottom), brush, strokeWidth, strokeStyle);
    DrawLine(D2D1::Point2F(rect.right, rect.bottom), D2D1::Point2F(rect.left, rect.bottom), brush, strokeWidth, strokeStyle);
    DrawLine(D2D1::Point2F(rect.left, rect.bottom), D2D1::Point2F(rect.left, rect.top), brush, strokeWidth, strokeStyle);
}

// SDF 算法实现的抗锯齿圆角矩形渲染
static inline float sdRoundedBox(float px, float py, float bx, float by, float r) {
    float qx = std::abs(px) - bx + r;
    float qy = std::abs(py) - by + r;
    return std::min(std::max(qx, qy), 0.0f) + std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) - r;
}

void LinuxRenderTarget::FillRoundedRectangle(const D2D1_ROUNDED_RECT& roundedRect, ID2D1Brush* brush) {
    if (!brush) return;
    auto* solidBrush = dynamic_cast<ID2D1SolidColorBrush*>(brush);
    bool isSolid = (solidBrush != nullptr);
    uint32_t solidColor = isSolid ? ColorToPremul(solidBrush->GetColor()) : 0;
    bool isOpaque = isSolid && ((solidColor >> 24) == 255);

    const auto& rc = roundedRect.rect;
    float cx = (rc.left + rc.right) * 0.5f;
    float cy = (rc.top + rc.bottom) * 0.5f;
    float bx = (rc.right - rc.left) * 0.5f;
    float by = (rc.bottom - rc.top) * 0.5f;
    float r = (std::min)({ roundedRect.radiusX, roundedRect.radiusY, bx, by });

    D2D1_POINT_2F p0 = TransformPoint(D2D1::Point2F(rc.left, rc.top));
    D2D1_POINT_2F p1 = TransformPoint(D2D1::Point2F(rc.right, rc.top));
    D2D1_POINT_2F p2 = TransformPoint(D2D1::Point2F(rc.right, rc.bottom));
    D2D1_POINT_2F p3 = TransformPoint(D2D1::Point2F(rc.left, rc.bottom));

    float minX = (std::min)({ p0.x, p1.x, p2.x, p3.x });
    float maxX = (std::max)({ p0.x, p1.x, p2.x, p3.x });
    float minY = (std::min)({ p0.y, p1.y, p2.y, p3.y });
    float maxY = (std::max)({ p0.y, p1.y, p2.y, p3.y });

    int clipLeft = static_cast<int>(m_currentClip.left);
    int clipRight = static_cast<int>(m_currentClip.right) - 1;
    int clipTop = static_cast<int>(m_currentClip.top);
    int clipBottom = static_cast<int>(m_currentClip.bottom) - 1;

    int x0 = (std::max)(clipLeft, static_cast<int>(std::floor(minX - 1.5f)));
    int x1 = (std::min)(clipRight, static_cast<int>(std::ceil(maxX + 1.5f)));
    int y0 = (std::max)(clipTop, static_cast<int>(std::floor(minY - 1.5f)));
    int y1 = (std::min)(clipBottom, static_cast<int>(std::ceil(maxY + 1.5f)));
    if (x0 > x1 || y0 > y1) return;

    float scale = GetTransformScale();

    for (int y = y0; y <= y1; ++y) {
        float py = static_cast<float>(y) + 0.5f;
        uint32_t* rowPixels = &m_pixels[y * m_width];
        for (int x = x0; x <= x1; ++x) {
            float px = static_cast<float>(x) + 0.5f;
            D2D1_POINT_2F localPt = InverseTransformPoint(D2D1::Point2F(px, py));
            float dLocal = sdRoundedBox(localPt.x - cx, localPt.y - cy, bx, by, r);
            float d = dLocal * scale;
            if (d < 0.5f) {
                float cov = std::clamp(0.5f - d, 0.0f, 1.0f);
                if (cov >= 1.0f && isOpaque) {
                    rowPixels[x] = solidColor;
                } else {
                    uint32_t pixelColor = isSolid ? solidColor : ColorToPremul(SampleBrushColor(brush, localPt.x, localPt.y));
                    BlendPixel(x, y, pixelColor, cov);
                }
            }
        }
    }
}

void LinuxRenderTarget::DrawRoundedRectangle(const D2D1_ROUNDED_RECT& roundedRect, ID2D1Brush* brush, float strokeWidth, ID2D1StrokeStyle* /*strokeStyle*/) {
    if (!brush) return;
    auto* solidBrush = dynamic_cast<ID2D1SolidColorBrush*>(brush);
    if (!solidBrush) return;
    uint32_t color = ColorToPremul(solidBrush->GetColor());

    const auto& rc = roundedRect.rect;
    float cx = (rc.left + rc.right) * 0.5f;
    float cy = (rc.top + rc.bottom) * 0.5f;
    float bx = (rc.right - rc.left) * 0.5f;
    float by = (rc.bottom - rc.top) * 0.5f;
    float r = (std::min)({ roundedRect.radiusX, roundedRect.radiusY, bx, by });

    float scale = GetTransformScale();
    float halfW = (std::max)(0.5f, strokeWidth * 0.5f * scale);

    D2D1_POINT_2F p0 = TransformPoint(D2D1::Point2F(rc.left, rc.top));
    D2D1_POINT_2F p1 = TransformPoint(D2D1::Point2F(rc.right, rc.top));
    D2D1_POINT_2F p2 = TransformPoint(D2D1::Point2F(rc.right, rc.bottom));
    D2D1_POINT_2F p3 = TransformPoint(D2D1::Point2F(rc.left, rc.bottom));

    float minX = (std::min)({ p0.x, p1.x, p2.x, p3.x }) - halfW;
    float maxX = (std::max)({ p0.x, p1.x, p2.x, p3.x }) + halfW;
    float minY = (std::min)({ p0.y, p1.y, p2.y, p3.y }) - halfW;
    float maxY = (std::max)({ p0.y, p1.y, p2.y, p3.y }) + halfW;

    int clipLeft = static_cast<int>(m_currentClip.left);
    int clipRight = static_cast<int>(m_currentClip.right) - 1;
    int clipTop = static_cast<int>(m_currentClip.top);
    int clipBottom = static_cast<int>(m_currentClip.bottom) - 1;

    int x0 = (std::max)(clipLeft, static_cast<int>(std::floor(minX - 1.5f)));
    int x1 = (std::min)(clipRight, static_cast<int>(std::ceil(maxX + 1.5f)));
    int y0 = (std::max)(clipTop, static_cast<int>(std::floor(minY - 1.5f)));
    int y1 = (std::min)(clipBottom, static_cast<int>(std::ceil(maxY + 1.5f)));
    if (x0 > x1 || y0 > y1) return;

    for (int y = y0; y <= y1; ++y) {
        float py = static_cast<float>(y) + 0.5f;
        for (int x = x0; x <= x1; ++x) {
            float px = static_cast<float>(x) + 0.5f;
            D2D1_POINT_2F localPt = InverseTransformPoint(D2D1::Point2F(px, py));
            float dLocal = sdRoundedBox(localPt.x - cx, localPt.y - cy, bx, by, r);
            float d = std::abs(dLocal * scale) - halfW;
            if (d < 0.5f) {
                float cov = std::clamp(0.5f - d, 0.0f, 1.0f);
                BlendPixel(x, y, color, cov);
            }
        }
    }
}

// 椭圆 / 圆形填充 (支持旋转、倾斜与逆变换精准光栅化)
void LinuxRenderTarget::FillEllipse(const D2D1_ELLIPSE& ellipse, ID2D1Brush* brush) {
    if (!brush) return;
    auto* solidBrush = dynamic_cast<ID2D1SolidColorBrush*>(brush);
    bool isSolid = (solidBrush != nullptr);
    uint32_t solidColor = isSolid ? ColorToPremul(solidBrush->GetColor()) : 0;
    bool isOpaque = isSolid && ((solidColor >> 24) == 255);

    D2D1_POINT_2F cp = TransformPoint(ellipse.point);
    float scale = GetTransformScale();

    float halfW = std::sqrt((ellipse.radiusX * m_transform._11) * (ellipse.radiusX * m_transform._11) +
                            (ellipse.radiusY * m_transform._21) * (ellipse.radiusY * m_transform._21));
    float halfH = std::sqrt((ellipse.radiusX * m_transform._12) * (ellipse.radiusX * m_transform._12) +
                            (ellipse.radiusY * m_transform._22) * (ellipse.radiusY * m_transform._22));

    int clipLeft = static_cast<int>(m_currentClip.left);
    int clipRight = static_cast<int>(m_currentClip.right) - 1;
    int clipTop = static_cast<int>(m_currentClip.top);
    int clipBottom = static_cast<int>(m_currentClip.bottom) - 1;

    int x0 = (std::max)(clipLeft, static_cast<int>(std::floor(cp.x - halfW - 1.5f)));
    int x1 = (std::min)(clipRight, static_cast<int>(std::ceil(cp.x + halfW + 1.5f)));
    int y0 = (std::max)(clipTop, static_cast<int>(std::floor(cp.y - halfH - 1.5f)));
    int y1 = (std::min)(clipBottom, static_cast<int>(std::ceil(cp.y + halfH + 1.5f)));
    if (x0 > x1 || y0 > y1) return;

    float invRx = 1.0f / (std::max)(0.001f, ellipse.radiusX);
    float invRy = 1.0f / (std::max)(0.001f, ellipse.radiusY);
    float minR = (std::min)(ellipse.radiusX, ellipse.radiusY);

    for (int y = y0; y <= y1; ++y) {
        float py = static_cast<float>(y) + 0.5f;
        uint32_t* rowPixels = &m_pixels[y * m_width];
        for (int x = x0; x <= x1; ++x) {
            float px = static_cast<float>(x) + 0.5f;
            D2D1_POINT_2F localPt = InverseTransformPoint(D2D1::Point2F(px, py));
            float lx = (localPt.x - ellipse.point.x) * invRx;
            float ly = (localPt.y - ellipse.point.y) * invRy;
            float distNorm = std::hypot(lx, ly) - 1.0f;
            float pixelDist = distNorm * minR * scale;
            if (pixelDist < 0.5f) {
                float cov = std::clamp(0.5f - pixelDist, 0.0f, 1.0f);
                if (cov >= 1.0f && isOpaque) {
                    rowPixels[x] = solidColor;
                } else {
                    uint32_t pixelColor = isSolid ? solidColor : ColorToPremul(SampleBrushColor(brush, localPt.x, localPt.y));
                    BlendPixel(x, y, pixelColor, cov);
                }
            }
        }
    }
}

// 椭圆 / 圆形描边 (支持任意旋转变换与逆变换描边)
void LinuxRenderTarget::DrawEllipse(const D2D1_ELLIPSE& ellipse, ID2D1Brush* brush, float strokeWidth, ID2D1StrokeStyle* /*strokeStyle*/) {
    if (!brush) return;
    auto* solidBrush = dynamic_cast<ID2D1SolidColorBrush*>(brush);
    if (!solidBrush) return;
    uint32_t color = ColorToPremul(solidBrush->GetColor());

    D2D1_POINT_2F cp = TransformPoint(ellipse.point);
    float scale = GetTransformScale();
    float halfW = (std::max)(0.5f, strokeWidth * 0.5f * scale);

    float spanW = std::sqrt((ellipse.radiusX * m_transform._11) * (ellipse.radiusX * m_transform._11) +
                            (ellipse.radiusY * m_transform._21) * (ellipse.radiusY * m_transform._21)) + halfW;
    float spanH = std::sqrt((ellipse.radiusX * m_transform._12) * (ellipse.radiusX * m_transform._12) +
                            (ellipse.radiusY * m_transform._22) * (ellipse.radiusY * m_transform._22)) + halfW;

    int clipLeft = static_cast<int>(m_currentClip.left);
    int clipRight = static_cast<int>(m_currentClip.right) - 1;
    int clipTop = static_cast<int>(m_currentClip.top);
    int clipBottom = static_cast<int>(m_currentClip.bottom) - 1;

    int x0 = (std::max)(clipLeft, static_cast<int>(std::floor(cp.x - spanW - 1.5f)));
    int x1 = (std::min)(clipRight, static_cast<int>(std::ceil(cp.x + spanW + 1.5f)));
    int y0 = (std::max)(clipTop, static_cast<int>(std::floor(cp.y - spanH - 1.5f)));
    int y1 = (std::min)(clipBottom, static_cast<int>(std::ceil(cp.y + spanH + 1.5f)));
    if (x0 > x1 || y0 > y1) return;

    float invRx = 1.0f / (std::max)(0.001f, ellipse.radiusX);
    float invRy = 1.0f / (std::max)(0.001f, ellipse.radiusY);
    float minR = (std::min)(ellipse.radiusX, ellipse.radiusY);

    for (int y = y0; y <= y1; ++y) {
        float py = static_cast<float>(y) + 0.5f;
        for (int x = x0; x <= x1; ++x) {
            float px = static_cast<float>(x) + 0.5f;
            D2D1_POINT_2F localPt = InverseTransformPoint(D2D1::Point2F(px, py));
            float lx = (localPt.x - ellipse.point.x) * invRx;
            float ly = (localPt.y - ellipse.point.y) * invRy;
            float distNorm = std::abs(std::hypot(lx, ly) - 1.0f);
            float pixelDist = distNorm * minR * scale;
            float d = pixelDist - halfW;
            if (d < 0.5f) {
                float cov = std::clamp(0.5f - d, 0.0f, 1.0f);
                BlendPixel(x, y, color, cov);
            }
        }
    }
}

// 复杂几何体描边 (PathGeometry，连续抗锯齿线段光栅化)
void LinuxRenderTarget::DrawGeometry(ID2D1Geometry* geometry, ID2D1Brush* brush, float strokeWidth, ID2D1StrokeStyle* strokeStyle) {
    auto* path = dynamic_cast<LinuxPathGeometry*>(geometry);
    if (!path || !brush) return;

    for (const auto& fig : path->figures) {
        for (size_t i = 1; i < fig.points.size(); ++i) {
            DrawLine(fig.points[i - 1], fig.points[i], brush, strokeWidth, strokeStyle);
        }
    }
}

// 复杂多边形填充 (扫描线填充 - 缓冲区复用与快速行填充优化)
void LinuxRenderTarget::FillGeometry(ID2D1Geometry* geometry, ID2D1Brush* brush, ID2D1Brush* /*opacityBrush*/) {
    auto* path = dynamic_cast<LinuxPathGeometry*>(geometry);
    if (!path || !brush) return;
    auto* solidBrush = dynamic_cast<ID2D1SolidColorBrush*>(brush);
    bool isSolid = (solidBrush != nullptr);
    uint32_t solidColor = isSolid ? ColorToPremul(solidBrush->GetColor()) : 0;
    bool isOpaque = isSolid && ((solidColor >> 24) == 255);

    int clipLeft = static_cast<int>(m_currentClip.left);
    int clipRight = static_cast<int>(m_currentClip.right) - 1;
    int clipTop = static_cast<int>(m_currentClip.top);
    int clipBottom = static_cast<int>(m_currentClip.bottom) - 1;

    std::vector<float> nodeX;
    nodeX.reserve(32);

    for (const auto& fig : path->figures) {
        if (fig.points.size() < 3) continue;

        std::vector<D2D1_POINT_2F> tPts;
        tPts.reserve(fig.points.size());
        float minY = 1e9f, maxY = -1e9f;
        for (const auto& p : fig.points) {
            D2D1_POINT_2F tp = TransformPoint(p);
            tPts.push_back(tp);
            minY = (std::min)(minY, tp.y);
            maxY = (std::max)(maxY, tp.y);
        }

        int y0 = (std::max)(clipTop, static_cast<int>(std::floor(minY)));
        int y1 = (std::min)(clipBottom, static_cast<int>(std::ceil(maxY)));

        for (int y = y0; y <= y1; ++y) {
            float scanY = static_cast<float>(y) + 0.5f;
            nodeX.clear();

            for (size_t i = 0, j = tPts.size() - 1; i < tPts.size(); j = i++) {
                if ((tPts[i].y < scanY && tPts[j].y >= scanY) || (tPts[j].y < scanY && tPts[i].y >= scanY)) {
                    float dy = tPts[j].y - tPts[i].y;
                    if (std::abs(dy) > 1e-5f) {
                        float x = tPts[i].x + (scanY - tPts[i].y) / dy * (tPts[j].x - tPts[i].x);
                        nodeX.push_back(x);
                    }
                }
            }

            if (nodeX.size() < 2) continue;
            std::sort(nodeX.begin(), nodeX.end());

            uint32_t* rowPixels = &m_pixels[y * m_width];
            for (size_t k = 0; k + 1 < nodeX.size(); k += 2) {
                int startX = (std::max)(clipLeft, static_cast<int>(std::floor(nodeX[k])));
                int endX   = (std::min)(clipRight, static_cast<int>(std::ceil(nodeX[k + 1])));
                if (startX > endX) continue;

                if (isSolid && isOpaque) {
                    std::fill(rowPixels + startX, rowPixels + endX + 1, solidColor);
                } else if (isSolid) {
                    for (int x = startX; x <= endX; ++x) {
                        BlendPremul(rowPixels[x], solidColor);
                    }
                } else {
                    for (int x = startX; x <= endX; ++x) {
                        float px = static_cast<float>(x) + 0.5f;
                        D2D1_POINT_2F localPt = InverseTransformPoint(D2D1::Point2F(px, scanY));
                        uint32_t pixelColor = ColorToPremul(SampleBrushColor(brush, localPt.x, localPt.y));
                        BlendPremul(rowPixels[x], pixelColor);
                    }
                }
            }
        }
    }
}

// 高清文本绘制 (FreeType2)
void LinuxRenderTarget::DrawText(
    const wchar_t* string,
    uint32_t stringLength,
    IDWriteTextFormat* textFormat,
    const D2D1_RECT_F& layoutRect,
    ID2D1Brush* defaultFillBrush,
    D2D1_DRAW_TEXT_OPTIONS /*options*/
) {
    if (!string || stringLength == 0 || !textFormat || !defaultFillBrush) return;
    D2D1_COLOR_F textColor = GetBrushColor(defaultFillBrush);

    float scaleX = std::sqrt(m_transform._11 * m_transform._11 + m_transform._12 * m_transform._12);
    float scaleY = std::sqrt(m_transform._21 * m_transform._21 + m_transform._22 * m_transform._22);
    float avgScale = (scaleX + scaleY) * 0.5f;
    if (avgScale <= 0.001f) avgScale = 1.0f;
    if (scaleX <= 0.001f) scaleX = 1.0f;
    if (scaleY <= 0.001f) scaleY = 1.0f;

    D2D1_POINT_2F vecX = { m_transform._11 / scaleX, m_transform._12 / scaleX };
    D2D1_POINT_2F vecY = { m_transform._21 / scaleY, m_transform._22 / scaleY };
    bool isAxisAligned = (std::abs(vecX.y) < 1e-4f && std::abs(vecY.x) < 1e-4f && vecX.x > 0.0f && vecY.y > 0.0f);

    float effectiveSize = (std::max)(1.0f, textFormat->fontSize * avgScale);

    FT_Face primaryFace = FreeTypeFontEngine::Instance().GetFace(textFormat->fontFamily, effectiveSize, textFormat->weight);
    FT_Face cjkFace = FreeTypeFontEngine::Instance().GetCjkFallbackFace(effectiveSize, textFormat->weight);
    if (!primaryFace && !cjkFace) return;

    auto getGlyph = [&](wchar_t ch, FT_Face& outFace) -> FT_UInt {
        if (primaryFace) {
            FT_UInt idx = FT_Get_Char_Index(primaryFace, ch);
            if (idx != 0) {
                outFace = primaryFace;
                return idx;
            }
        }
        if (cjkFace) {
            FT_UInt idx = FT_Get_Char_Index(cjkFace, ch);
            if (idx != 0) {
                outFace = cjkFace;
                return idx;
            }
        }
        outFace = primaryFace ? primaryFace : cjkFace;
        return 0;
    };

    auto isZeroWidthOrModifier = [](wchar_t ch) -> bool {
        if (ch >= 0xFE00 && ch <= 0xFE0F) return true; // Variation Selectors
        if (ch == 0x200B || ch == 0x200C || ch == 0x200D || ch == 0xFEFF) return true; // Zero-width spaces & joiners
        if (ch < 0x20 && ch != L'\t' && ch != L'\n' && ch != L'\r') return true;
        return false;
    };

    // 1. 测量全部字符在局部坐标系下的排版尺寸
    float totalW = 0.0f;
    for (uint32_t i = 0; i < stringLength; ++i) {
        FT_Face glyphFace = nullptr;
        FT_UInt glyphIdx = getGlyph(string[i], glyphFace);
        if (glyphFace && glyphIdx != 0) {
            if (FT_Load_Glyph(glyphFace, glyphIdx, FT_LOAD_DEFAULT) == 0) {
                totalW += static_cast<float>(glyphFace->glyph->advance.x >> 6) / avgScale;
            }
        } else if (!isZeroWidthOrModifier(string[i])) {
            totalW += textFormat->fontSize * 0.6f;
        }
    }

    // 2. 根据 layoutRect 对齐方式在局部坐标系下计算排版位置
    float boxW = layoutRect.right - layoutRect.left;
    float boxH = layoutRect.bottom - layoutRect.top;

    float localStartX = layoutRect.left;
    if (textFormat->textAlignment == DWRITE_TEXT_ALIGNMENT_CENTER) {
        localStartX += (boxW - totalW) * 0.5f;
    } else if (textFormat->textAlignment == DWRITE_TEXT_ALIGNMENT_TRAILING) {
        localStartX += boxW - totalW;
    }

    int asc1 = primaryFace ? (primaryFace->size->metrics.ascender >> 6) : 0;
    int asc2 = cjkFace ? (cjkFace->size->metrics.ascender >> 6) : 0;
    float maxAscender = static_cast<float>((std::max)(asc1, asc2)) / avgScale;
    if (maxAscender <= 0.001f) maxAscender = textFormat->fontSize * 0.8f;

    float localBaselineY = layoutRect.top + maxAscender;
    if (textFormat->paragraphAlignment == DWRITE_PARAGRAPH_ALIGNMENT_CENTER) {
        int h1 = primaryFace ? (primaryFace->size->metrics.height >> 6) : 0;
        int h2 = cjkFace ? (cjkFace->size->metrics.height >> 6) : 0;
        float maxHeight = static_cast<float>((std::max)(h1, h2)) / avgScale;
        if (maxHeight <= 0.001f) maxHeight = textFormat->fontSize;
        localBaselineY = layoutRect.top + (boxH - maxHeight) * 0.5f + maxAscender;
    }

    // 3. 逐字元渲染 (严格响应仿射矩阵与 m_currentClip 剪裁约束)
    float penX = localStartX;
    for (uint32_t i = 0; i < stringLength; ++i) {
        FT_Face glyphFace = nullptr;
        FT_UInt glyphIdx = getGlyph(string[i], glyphFace);
        if (glyphFace && glyphIdx != 0 && FT_Load_Glyph(glyphFace, glyphIdx, FT_LOAD_RENDER) == 0) {
            FT_Bitmap& bmp = glyphFace->glyph->bitmap;
            D2D1_POINT_2F localPt = D2D1::Point2F(penX, localBaselineY);
            D2D1_POINT_2F devicePt = TransformPoint(localPt);

            float bLeft = static_cast<float>(glyphFace->glyph->bitmap_left);
            float bTop  = static_cast<float>(glyphFace->glyph->bitmap_top);

            for (unsigned int row = 0; row < bmp.rows; ++row) {
                for (unsigned int col = 0; col < bmp.width; ++col) {
                    unsigned char cov = bmp.buffer[row * bmp.pitch + col];
                    if (cov == 0) continue;

                    int dstX = 0;
                    int dstY = 0;
                    if (isAxisAligned) {
                        dstX = static_cast<int>(std::round(devicePt.x + (bLeft + col)));
                        dstY = static_cast<int>(std::round(devicePt.y + (row - bTop)));
                    } else {
                        float offsetX = bLeft + col;
                        float offsetY = static_cast<float>(row) - bTop;
                        float px = devicePt.x + offsetX * vecX.x + offsetY * vecY.x;
                        float py = devicePt.y + offsetX * vecX.y + offsetY * vecY.y;
                        dstX = static_cast<int>(std::round(px));
                        dstY = static_cast<int>(std::round(py));
                    }

                    if (dstY < 0 || dstY >= m_height) continue;
                    if (dstY < m_currentClip.top || dstY >= m_currentClip.bottom) continue;
                    if (dstX < 0 || dstX >= m_width) continue;
                    if (dstX < m_currentClip.left || dstX >= m_currentClip.right) continue;

                    float alpha = static_cast<float>(cov) / 255.0f;
                    uint32_t c = ColorToPremul(textColor, alpha);
                    BlendPremul(m_pixels[dstY * m_width + dstX], c);
                }
            }
            penX += static_cast<float>(glyphFace->glyph->advance.x >> 6) / avgScale;
        } else {
            if (!isZeroWidthOrModifier(string[i])) {
                // 缺失字形回退 (仅对可见正文字符呈现 Tofu 替换矩形，自动抑制控制符与变体选择符)
                float tfW = textFormat->fontSize * 0.45f;
                float tfH = textFormat->fontSize * 0.7f;
                float tfX = penX + textFormat->fontSize * 0.075f;
                float tfY = localBaselineY - tfH;

                D2D1_RECT_F tfRc = D2D1::RectF(tfX, tfY, tfX + tfW, tfY + tfH);
                LinuxBrush tofuBrush(textColor);
                DrawRectangle(tfRc, &tofuBrush, 1.0f);
                penX += textFormat->fontSize * 0.6f;
            }
        }
    }
}

void LinuxRenderTarget::CreateGradientStopCollection(const D2D1_GRADIENT_STOP* gradientStops, uint32_t gradientStopsCount, ID2D1GradientStopCollection** ppGradientStopCollection) {
    if (!ppGradientStopCollection) return;
    *ppGradientStopCollection = new LinuxGradientStopCollection(gradientStops, gradientStopsCount);
}

void LinuxRenderTarget::CreateLinearGradientBrush(const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES& linearGradientBrushProperties, ID2D1GradientStopCollection* gradientStopCollection, ID2D1LinearGradientBrush** ppLinearGradientBrush) {
    if (!ppLinearGradientBrush) return;
    *ppLinearGradientBrush = new LinuxLinearGradientBrush(linearGradientBrushProperties, gradientStopCollection);
}

void LinuxRenderTarget::CreateRadialGradientBrush(const D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES& radialGradientBrushProperties, ID2D1GradientStopCollection* gradientStopCollection, ID2D1RadialGradientBrush** ppRadialGradientBrush) {
    if (!ppRadialGradientBrush) return;
    *ppRadialGradientBrush = new LinuxRadialGradientBrush(radialGradientBrushProperties, gradientStopCollection);
}
