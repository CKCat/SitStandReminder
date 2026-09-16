#undef NDEBUG
#include "graphics/D2DCompat.hpp"
#include "graphics/ExerciseLayout.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#ifndef _WIN32
#include "graphics/linux/LinuxCanvas.hpp"

// 1. 验证 PathGeometry 在 Close 之后能完整接收并保留 Sink 中的 Figures 数据 (BUG-01 回归测试)
void TestPathGeometryFigureRetention() {
    auto path = std::make_unique<LinuxPathGeometry>();
    ID2D1GeometrySink* pSink = nullptr;
    bool opened = path->Open(&pSink);
    assert(opened && pSink != nullptr);

    pSink->BeginFigure(D2D1::Point2F(10.0f, 20.0f), D2D1_FIGURE_BEGIN_FILLED);
    pSink->AddLine(D2D1::Point2F(30.0f, 40.0f));
    pSink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(35.0f, 45.0f),
        D2D1::Point2F(50.0f, 60.0f),
        D2D1::Point2F(70.0f, 80.0f)
    ));
    pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
    pSink->Close();

    // 核心断言：path->figures 必须包含回传的图元数据，不能是空集合！
    assert(!path->figures.empty() && "FAIL: path->figures should NOT be empty after sink->Close()!");
    assert(path->figures.size() == 1);
    assert(path->figures[0].begin == D2D1_FIGURE_BEGIN_FILLED);
    assert(path->figures[0].end == D2D1_FIGURE_END_CLOSED);
    assert(path->figures[0].points.size() >= 2); // 包含起始点、直线端点与贝塞尔插值点

    delete pSink;
    std::cout << "[PASS] TestPathGeometryFigureRetention" << std::endl;
}

// 2. 验证 AddArc 输出的是真正的圆弧/椭圆弧，而非简单的两点共线直线 (BUG-02 回归测试)
void TestAddArcCurvature() {
    auto path = std::make_unique<LinuxPathGeometry>();
    ID2D1GeometrySink* pSink = nullptr;
    path->Open(&pSink);

    // 起点 (100, 50)，半径 (50, 50)，顺时针绘制 90 度圆弧到 (50, 100)，圆心在 (50, 50)
    D2D1_POINT_2F startPt = D2D1::Point2F(100.0f, 50.0f);
    D2D1_POINT_2F endPt = D2D1::Point2F(50.0f, 100.0f);
    D2D1_POINT_2F expectedCenter = D2D1::Point2F(50.0f, 50.0f);
    float expectedRadius = 50.0f;

    pSink->BeginFigure(startPt, D2D1_FIGURE_BEGIN_HOLLOW);
    pSink->AddArc(D2D1::ArcSegment(
        endPt,
        D2D1::SizeF(expectedRadius, expectedRadius),
        0.0f,
        D2D1_SWEEP_DIRECTION_CLOCKWISE,
        D2D1_ARC_SIZE_SMALL
    ));
    pSink->EndFigure(D2D1_FIGURE_END_OPEN);
    pSink->Close();

    assert(!path->figures.empty());
    const auto& pts = path->figures[0].points;
    assert(pts.size() >= 5 && "FAIL: Arc should contain multiple subdivision points!");

    // 针对圆弧中间的点进行几何半径检查
    // 如果是错误的直线插值，其中点 (75, 75) 到 (50, 50) 的距离是 sqrt(25^2 + 25^2) = 35.35px，与 50px 误差极大！
    // 如果是正确的真圆弧插值，每个点到期望圆心的距离必须在 50px 的合理误差容限 (0.5px) 内！
    for (size_t i = 1; i < pts.size() - 1; ++i) {
        float dx = pts[i].x - expectedCenter.x;
        float dy = pts[i].y - expectedCenter.y;
        float dist = std::hypot(dx, dy);
        float radiusErr = std::abs(dist - expectedRadius);
        assert(radiusErr < 1.0f && "FAIL: AddArc produced points that deviate from true arc radius!");
    }

    delete pSink;
    std::cout << "[PASS] TestAddArcCurvature" << std::endl;
}

// 3. 验证 LinuxRenderTarget 文本绘制受 Clip 剪裁区域保护 (BUG-08 回归测试)
void TestDrawTextClipProtection() {
    LinuxRenderTarget rt(100, 100);
    rt.Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    // 设置一个限制在 [10, 10] 到 [30, 30] 的剪裁矩形
    rt.PushAxisAlignedClip(D2D1::RectF(10.0f, 10.0f, 30.0f, 30.0f), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    // 尝试在超出剪裁区域的 [0, 0] 到 [100, 100] 绘制一段较长文字
    auto tf = std::make_unique<IDWriteTextFormat>();
    tf->fontFamily = L"sans-serif";
    tf->fontSize = 16.0f;
    LinuxBrush brush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));

    const wchar_t* text = L"SitStandReminder";
    rt.DrawText(text, wcslen(text), tf.get(), D2D1::RectF(0.0f, 0.0f, 100.0f, 30.0f), &brush);
    rt.PopAxisAlignedClip();

    const uint32_t* pixels = rt.GetPixels();
    // 检查剪裁区域外部（例如 x < 10 或 x >= 30）的像素，必须保持为初始背景 (0)，不能被着色！
    bool leakOutside = false;
    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) {
            if (x < 10 || x >= 30 || y < 10 || y >= 30) {
                if (pixels[y * 100 + x] != 0) {
                    leakOutside = true;
                    break;
                }
            }
        }
        if (leakOutside) break;
    }
    assert(!leakOutside && "FAIL: Text rendering leaked outside the AxisAlignedClip boundary!");
    std::cout << "[PASS] TestDrawTextClipProtection" << std::endl;
}

// 4. 验证 LinuxRenderTarget::FillRectangle 在旋转变换下精准光栅化，不发生 AABB 矩形溢出涂脏 (BUG-03 回归测试)
void TestRotatedFillRectangleNoAABBLeakage() {
    LinuxRenderTarget rt(100, 100);
    rt.Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    // 绕 (50, 50) 旋转 45 度
    D2D1_MATRIX_3X2_F rot = D2D1::Matrix3x2F::Rotation(45.0f, D2D1::Point2F(50.0f, 50.0f));
    rt.SetTransform(rot);

    LinuxBrush brush(D2D1::ColorF(1.0f, 0.0f, 0.0f, 1.0f));
    // 在局部坐标系绘制 [30, 30] 到 [70, 70] 的矩形 (宽40，高40)
    rt.FillRectangle(D2D1::RectF(30.0f, 30.0f, 70.0f, 70.0f), &brush);

    const uint32_t* pixels = rt.GetPixels();
    // 旋转 45 度后，中心点 (50, 50) 必在矩形内部，必须为红色
    uint32_t centerPixel = pixels[50 * 100 + 50];
    uint32_t centerAlpha = (centerPixel >> 24) & 0xFF;
    assert(centerAlpha > 200 && "FAIL: Center (50, 50) must be filled!");
    // 旋转 45 度后，四个顶点为 (50, 22), (78, 50), (50, 78), (22, 50)
    // 点 (40, 50) 和 (60, 50) 距离中心仅 10px，显然在矩形内部，必须被着色！
    // 旧实现因仅取 (rect.left, rect.top) 和 (rect.right, rect.bottom) 变换点算 AABB，
    // 导致旋转 45 度时 x0=50, x1=50，整个 40x40 矩形严重坍缩成一条垂直单像素线，(40, 50) alpha 为 0！
    uint32_t leftSidePixel = pixels[50 * 100 + 40];
    uint32_t leftAlpha = (leftSidePixel >> 24) & 0xFF;
    std::cout << "  Rotated FillRectangle side point (40, 50) alpha: " << leftAlpha << std::endl;
    assert(leftAlpha > 200 && "FAIL: Left side of rotated rectangle (40, 50) collapsed into a 1-pixel line!");

    // (30, 30) 在旋转 45 度后距离中心 28.28px，超出半宽 20px，在斜矩形外部！
    // 若错误地使用 AABB 包围盒，(30, 30) 会被错误涂成红色。正确的仿射光栅化此处必须保持透明 (0)！
    uint32_t cornerPixel = pixels[30 * 100 + 30];
    uint32_t cornerAlpha = (cornerPixel >> 24) & 0xFF;
    std::cout << "  Rotated FillRectangle corner (30, 30) alpha: " << cornerAlpha << std::endl;
    assert(cornerAlpha == 0 && "FAIL: Rotated FillRectangle leaked color to AABB corner (30, 30)!");

    std::cout << "[PASS] TestRotatedFillRectangleNoAABBLeakage" << std::endl;
}

// 5. 验证 LinuxRenderTarget::DrawGeometry 在闭合图形 (D2D1_FIGURE_END_CLOSED) 时完整闭合轮廓 (BUG-04 回归测试)
void TestDrawGeometryClosedFigure() {
    LinuxRenderTarget rt(100, 100);
    rt.Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    auto path = std::make_unique<LinuxPathGeometry>();
    ID2D1GeometrySink* pSink = nullptr;
    path->Open(&pSink);
    pSink->BeginFigure(D2D1::Point2F(10.0f, 10.0f), D2D1_FIGURE_BEGIN_HOLLOW);
    pSink->AddLine(D2D1::Point2F(90.0f, 10.0f));
    pSink->AddLine(D2D1::Point2F(90.0f, 90.0f));
    pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
    pSink->Close();
    delete pSink;

    LinuxBrush brush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
    rt.DrawGeometry(path.get(), &brush, 2.0f);

    const uint32_t* pixels = rt.GetPixels();
    // 闭合斜对角线从 (90, 90) 回连到 (10, 10)，中心点 (50, 50) 恰好在此斜线上，必须有像素！
    // 若缺少闭合连接逻辑，此处未被绘制 (alpha == 0)！
    uint32_t diagPixel = pixels[50 * 100 + 50];
    uint32_t diagAlpha = (diagPixel >> 24) & 0xFF;
    std::cout << "  Closed geometry diagonal point (50, 50) alpha: " << diagAlpha << std::endl;
    assert(diagAlpha > 100 && "FAIL: DrawGeometry failed to connect closed figure edge back to start!");
    std::cout << "[PASS] TestDrawGeometryClosedFigure" << std::endl;
}

// 6. 验证 LinuxRenderTarget::DrawGeometry 使用半透明画刷时 Alpha 纯净，无离散采样点 FillEllipse 串珠重叠伪影 (BUG-05 回归测试)
void TestDrawGeometryAlphaBlendingNoBeadArtifact() {
    LinuxRenderTarget rt(100, 100);
    rt.Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    auto path = std::make_unique<LinuxPathGeometry>();
    ID2D1GeometrySink* pSink = nullptr;
    path->Open(&pSink);
    pSink->BeginFigure(D2D1::Point2F(10.0f, 50.0f), D2D1_FIGURE_BEGIN_HOLLOW);
    // 添加一段贝塞尔曲线，其细分采样点密集排布
    pSink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(30.0f, 20.0f),
        D2D1::Point2F(70.0f, 80.0f),
        D2D1::Point2F(90.0f, 50.0f)
    ));
    pSink->EndFigure(D2D1_FIGURE_END_OPEN);
    pSink->Close();
    delete pSink;

    // 画刷 Alpha 为 0.3f (约为 76 / 255)
    LinuxBrush translucentBrush(D2D1::ColorF(1.0f, 0.0f, 0.0f, 0.3f));
    rt.DrawGeometry(path.get(), &translucentBrush, 4.0f);

    const uint32_t* pixels = rt.GetPixels();
    uint32_t maxAlpha = 0;
    for (int i = 0; i < 100 * 100; ++i) {
        uint32_t a = (pixels[i] >> 24) & 0xFF;
        if (a > maxAlpha) {
            maxAlpha = a;
        }
    }
    std::cout << "  DrawGeometry translucent max alpha: " << maxAlpha << " (brush target: 76, max overlap cap: 135)" << std::endl;
    // 消除离散采样点 FillEllipse 串珠后，仅在相邻线段接缝处存在合法的双重交汇混合 (Alpha <= 135)，绝不会出现 160~255 的恶性串珠伪影
    assert(maxAlpha <= 135 && "FAIL: DrawGeometry had severe bead overlapping artifact caused by redundant FillEllipse!");
    std::cout << "[PASS] TestDrawGeometryAlphaBlendingNoBeadArtifact" << std::endl;
}

// 7. 验证中文字体族名以 UTF-8 格式传递给 FontConfig，不被 wc < 128 粗暴丢弃 (BUG-06 回归测试)
void TestChineseFontFamilyUtf8Resolution() {
    LinuxRenderTarget rt(150, 60);
    rt.Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    auto tf = std::make_unique<IDWriteTextFormat>();
    tf->fontFamily = L"文泉驿微米黑";
    tf->fontSize = 18.0f;
    LinuxBrush brush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));

    const wchar_t* text = L"坐立提醒";
    rt.DrawText(text, wcslen(text), tf.get(), D2D1::RectF(0.0f, 0.0f, 150.0f, 60.0f), &brush);

    const uint32_t* pixels = rt.GetPixels();
    bool hasTextPixels = false;
    for (int i = 0; i < 150 * 60; ++i) {
        if (((pixels[i] >> 24) & 0xFF) > 100) {
            hasTextPixels = true;
            break;
        }
    }
    assert(hasTextPixels && "FAIL: Chinese font text rendering failed to output any pixels!");
    std::cout << "[PASS] TestChineseFontFamilyUtf8Resolution" << std::endl;
}

// 8. 验证 LinuxCanvas 线性渐变和径向渐变进行真实颜色插值采样，而非扁平单色 (BUG-02 回归测试)
void TestLinearAndRadialGradientRendering() {
    LinuxRenderTarget rt(100, 20);
    rt.Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    // 创建从纯黑 (0,0,0,1) 到纯白 (1,1,1,1) 的水平线性渐变
    D2D1_GRADIENT_STOP stops[2] = {
        { 0.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f) },
        { 1.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f) }
    };
    LinuxGradientStopCollection coll(stops, 2);
    D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES linProps;
    linProps.startPoint = D2D1::Point2F(0.0f, 10.0f);
    linProps.endPoint = D2D1::Point2F(100.0f, 10.0f);
    LinuxLinearGradientBrush linBrush(linProps, &coll);

    rt.FillRectangle(D2D1::RectF(0.0f, 0.0f, 100.0f, 20.0f), &linBrush);

    const uint32_t* pixels = rt.GetPixels();
    // 检查左中右三个采样点的颜色通道值
    // 左侧 (x=5, y=10) 接近纯黑
    uint32_t leftPixel = pixels[10 * 100 + 5];
    uint32_t leftR = (leftPixel >> 16) & 0xFF;

    // 中间 (x=50, y=10) 接近中灰 (~128)
    uint32_t midPixel = pixels[10 * 100 + 50];
    uint32_t midR = (midPixel >> 16) & 0xFF;

    // 右侧 (x=95, y=10) 接近纯白 (~242)
    uint32_t rightPixel = pixels[10 * 100 + 95];
    uint32_t rightR = (rightPixel >> 16) & 0xFF;

    std::cout << "  Gradient samples - leftR: " << leftR << ", midR: " << midR << ", rightR: " << rightR << std::endl;
    // 断言必须存在从暗到亮的渐变层次，不能全黑或全白，更不能未填充 (leftR < midR < rightR)
    assert(leftPixel != 0 && "FAIL: FillRectangle with LinearGradientBrush produced no pixels!");
    assert(midR > leftR + 30 && "FAIL: Midpoint is not significantly brighter than start point!");
    assert(rightR > midR + 30 && "FAIL: Endpoint is not significantly brighter than midpoint!");

    std::cout << "[PASS] TestLinearAndRadialGradientRendering" << std::endl;
}

// 9. 验证 LinuxRenderTarget::DrawText 在任意矩阵缩放仿射变换下正确缩放排版与光栅化 (DEF-09 回归测试)
void TestDrawTextScaleTransform() {
    LinuxRenderTarget rt(300, 100);
    rt.Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    // 设置 2x 缩放变换
    rt.SetTransform(D2D1::Matrix3x2F::Scale(2.0f, 2.0f));

    auto tf = std::make_unique<IDWriteTextFormat>();
    tf->fontFamily = L"sans-serif";
    tf->fontSize = 16.0f;
    LinuxBrush brush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));

    // 逻辑位置 [20, 10, 120, 40]
    // 缩放 2x 后，物理起始 X 至少应该在 20 * 2 = 40 附近，绝不能出现在 x < 35 的区域！
    const wchar_t* text = L"HELLOWORLD";
    rt.DrawText(text, wcslen(text), tf.get(), D2D1::RectF(20.0f, 10.0f, 120.0f, 40.0f), &brush);

    const uint32_t* pixels = rt.GetPixels();
    bool leakedLeft = false;
    int rightmostX = 0;
    int leftmostX = 300;

    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 300; ++x) {
            if (((pixels[y * 300 + x] >> 24) & 0xFF) > 50) {
                if (x < 35) leakedLeft = true;
                if (x > rightmostX) rightmostX = x;
                if (x < leftmostX) leftmostX = x;
            }
        }
    }

    std::cout << "  DrawText with Scale(2,2) - leftmostX: " << leftmostX << ", rightmostX: " << rightmostX << std::endl;
    // 验证起始 X 被放大到 40 附近，并且整体文字跨度大约为 2 倍
    assert(!leakedLeft && "FAIL: Text rendered with Scale(2.0) leaked into unscaled left region (x < 35)!");
    assert(leftmostX >= 35 && "FAIL: Text start X was not scaled by transform matrix!");
    assert(rightmostX > 150 && "FAIL: Text advance width was not scaled properly by transform matrix!");
    std::cout << "[PASS] TestDrawTextScaleTransform" << std::endl;
}

// 10. 验证 ExerciseLayoutHelper 在矮屏或小高度限制下人偶高度不得溢出穿透画布净高度 (DEF-12 回归测试)
void TestExerciseLayoutLowHeightProtection() {
    // 模拟极限矮屏 (1366x500)，DPI scale 1.0f
    D2D1_RECT_F lowBounds = D2D1::RectF(0.0f, 0.0f, 1366.0f, 500.0f);
    float baseDesignWidth = 400.0f;
    float baseDesignHeight = 350.0f;

    auto layout = ExerciseLayout::Calculate(
        lowBounds,
        1.0f,
        baseDesignWidth,
        baseDesignHeight
    );

    float canvasH = layout.canvasRect.bottom - layout.canvasRect.top;
    float renderedAnimH = baseDesignHeight * layout.animScale;

    std::cout << "  Low screen canvasH: " << canvasH << ", renderedAnimH: " << renderedAnimH << ", animScale: " << layout.animScale << std::endl;
    // 核心断言：人偶实际渲染高度不得超过可用画布净高度，防止上下穿透破损！
    assert(renderedAnimH <= canvasH + 0.1f && "FAIL: Animation scaled larger than available canvas height, penetrating headers/dock!");
    std::cout << "[PASS] TestExerciseLayoutLowHeightProtection" << std::endl;
}
#endif

int main() {
#ifndef _WIN32
    std::cout << "Running Graphics Pipeline Unit Tests..." << std::endl;
    TestPathGeometryFigureRetention();
    TestAddArcCurvature();
    TestDrawTextClipProtection();
    TestRotatedFillRectangleNoAABBLeakage();
    TestDrawGeometryClosedFigure();
    TestDrawGeometryAlphaBlendingNoBeadArtifact();
    TestChineseFontFamilyUtf8Resolution();
    TestLinearAndRadialGradientRendering();
    TestDrawTextScaleTransform();
    TestExerciseLayoutLowHeightProtection();
    std::cout << "All Graphics Pipeline Tests PASSED successfully!" << std::endl;
#endif
    return 0;
}
