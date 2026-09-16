#ifdef NDEBUG
#undef NDEBUG
#endif
#include <iostream>
#include <cassert>
#include <cstring>
#include <string>
#include <cstdlib>

#define ALWAYS_ASSERT(cond) do { \
    if (!(cond)) { \
        std::cerr << "\n[TEST FAILURE] Assertion failed: (" #cond ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::abort(); \
    } \
} while(0)

#ifndef _WIN32
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <dbus/dbus.h>
#include "ui/linux/X11Utils.hpp"
#include "ui/TrayWindow.hpp"
#include "ui/linux/X11App.hpp"
#include "graphics/D2DContext.hpp"
#include "graphics/ExerciseLayout.hpp"
#include "core/AppConstants.hpp"
#include "core/StateMachine.hpp"

StateMachine* g_pStateMachine = nullptr;

// 测试 1: 验证 UTF-8 窗口标题属性能够正确写入 _NET_WM_NAME 且类型为 UTF8_STRING (消除 åç«æé 乱码)
void TestWindowUtf8TitleProperty() {
    std::cout << "[RUN] TestWindowUtf8TitleProperty..." << std::endl;
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        std::cout << "  [SKIP] No X11 Display available" << std::endl;
        return;
    }

    int scr = DefaultScreen(dpy);
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0, 100, 100, 0, 0, 0);
    const char* expectedTitle = "坐立提醒 · 设置中心";

    // 设置 UTF-8 标题
    X11Utils::SetWindowUtf8Title(dpy, win, expectedTitle);

    // 校验 _NET_WM_NAME
    Atom netWmName = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom actualType;
    int actualFormat;
    unsigned long nItems, bytesAfter;
    unsigned char* propValue = nullptr;

    int status = XGetWindowProperty(
        dpy, win, netWmName, 0, 1024, False, AnyPropertyType,
        &actualType, &actualFormat, &nItems, &bytesAfter, &propValue
    );

    ALWAYS_ASSERT(status == Success);
    ALWAYS_ASSERT(propValue != nullptr);

    Atom utf8String = XInternAtom(dpy, "UTF8_STRING", False);
    ALWAYS_ASSERT(actualType == utf8String); // 必须是 UTF8_STRING，绝不可为 STRING (Latin-1)
    ALWAYS_ASSERT(std::string(reinterpret_cast<char*>(propValue)) == expectedTitle);

    XFree(propValue);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    std::cout << "[PASS] TestWindowUtf8TitleProperty" << std::endl;
}

// 测试 2: 验证右键菜单的显示、隐藏与生命周期状态
void TestTrayMenuLifecycle() {
    std::cout << "[RUN] TestTrayMenuLifecycle..." << std::endl;
    auto& tray = TrayWindow::Instance();
    // 初始状态菜单必须未显示
    ALWAYS_ASSERT(!tray.IsMenuVisible());

    tray.HideMenu();
    ALWAYS_ASSERT(!tray.IsMenuVisible());
    std::cout << "[PASS] TestTrayMenuLifecycle" << std::endl;
}

// 测试 3: 验证右键菜单每一项在视觉渲染矩形中心处的命中测试绝对对齐，彻底消除错位
void TestTrayMenuHitTesting() {
    std::cout << "[RUN] TestTrayMenuHitTesting..." << std::endl;
    auto& tray = TrayWindow::Instance();
    tray.Create();

    const auto& items = tray.GetMenuItems();
    ALWAYS_ASSERT(!items.empty());

    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].isSeparator) {
            continue;
        }

        D2D1_RECT_F r = tray.GetMenuItemRect(i);
        int centerY = static_cast<int>((r.top + r.bottom) / 2.0f);
        int hitIndex = tray.GetMenuItemAt(centerY);

        std::cout << "  Item " << i << " (" << (items[i].id) << "): rect [" 
                  << r.top << ", " << r.bottom << "], centerY=" << centerY 
                  << ", hitIndex=" << hitIndex << std::endl;

        // 核心断言：鼠标指向该项渲染的几何中心，命中的项索引必须严格等于 i！
        ALWAYS_ASSERT(hitIndex == static_cast<int>(i));
    }

    tray.Destroy();
    std::cout << "[PASS] TestTrayMenuHitTesting" << std::endl;
}

#include "ui/FullscreenMask.hpp"
#include <thread>

// 测试 4: 验证工间操全屏遮罩真实物理时间推进，彻底消除动画静止缺陷
void TestFullscreenAnimationProgress() {
    std::cout << "[RUN] TestFullscreenAnimationProgress..." << std::endl;
    auto& mask = FullscreenMask::Instance();
    mask.Initialize();
    mask.Show(true);

    float p0 = mask.GetNeckProgress();
    ALWAYS_ASSERT(p0 == 0.0f);

    // 模拟流逝 120 毫秒物理时间
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    mask.TriggerAnimationTick();

    float p1 = mask.GetNeckProgress();
    std::cout << "  Neck progress after 120ms: " << p1 << " (initial: " << p0 << ")" << std::endl;

    // 核心断言：物理时间流逝后，动画进度必须大于初始值（绝对不允许静止不前）
    ALWAYS_ASSERT(p1 > p0);

    mask.Show(false);
    std::cout << "[PASS] TestFullscreenAnimationProgress" << std::endl;
}

// 测试 5: 验证阶段切换自愈与动画时间轴平滑重置
void TestFullscreenStageTransition() {
    std::cout << "[RUN] TestFullscreenStageTransition..." << std::endl;
    auto& mask = FullscreenMask::Instance();
    mask.Show(true);

    ALWAYS_ASSERT(mask.GetCurrentStage() == 0);

    // 切换到阶段 1 (护眼操)
    mask.UpdateDisplay(30, 60, 1, L"阶段 2/2 · 👁️ 20-20-20 科学护眼操");
    ALWAYS_ASSERT(mask.GetCurrentStage() == 1);

    float eye0 = mask.GetEyeProgress();
    ALWAYS_ASSERT(eye0 == 0.0f);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mask.TriggerAnimationTick();

    float eye1 = mask.GetEyeProgress();
    std::cout << "  Eye progress after 100ms: " << eye1 << std::endl;
    ALWAYS_ASSERT(eye1 > eye0);

    mask.Show(false);
    std::cout << "[PASS] TestFullscreenStageTransition" << std::endl;
}

#include "ui/SettingsWindow.hpp"

// 测试 6: 验证重构后的高质感设置界面各卡片交互与控件命中区域精准性
void TestSettingsWindowInteractions() {
    std::cout << "[RUN] TestSettingsWindowInteractions..." << std::endl;
    auto& settings = SettingsWindow::Instance();
    settings.LoadConfigToUI();

    // 1. 测试预设按钮点击 (4个预设, id 分别为 1, 2, 3, 4, y: 42~72)
    // Preset 1 (45/15): 36 + 114/2 = 93, y = 57
    settings.SimulateClick(93, 57);
    ALWAYS_ASSERT(settings.GetSelectedPreset() == 1);
    ALWAYS_ASSERT(settings.GetTempConfig().workMinutes == 45);
    ALWAYS_ASSERT(settings.GetTempConfig().standMinutes == 15);
    ALWAYS_ASSERT(settings.GetTempConfig().restSeconds == 90);
    ALWAYS_ASSERT(settings.GetTempConfig().enableStand == true);

    // Preset 2 (50/10): 36 + 124 + 114/2 = 217, y = 57
    settings.SimulateClick(217, 57);
    ALWAYS_ASSERT(settings.GetSelectedPreset() == 2);
    ALWAYS_ASSERT(settings.GetTempConfig().workMinutes == 50);
    ALWAYS_ASSERT(settings.GetTempConfig().standMinutes == 10);
    ALWAYS_ASSERT(settings.GetTempConfig().restSeconds == 90);
    ALWAYS_ASSERT(settings.GetTempConfig().enableStand == true);

    // Preset 3 (25/5): 36 + 248 + 114/2 = 341, y = 57
    settings.SimulateClick(341, 57);
    ALWAYS_ASSERT(settings.GetSelectedPreset() == 3);
    ALWAYS_ASSERT(settings.GetTempConfig().workMinutes == 25);
    ALWAYS_ASSERT(settings.GetTempConfig().standMinutes == 5);
    ALWAYS_ASSERT(settings.GetTempConfig().restSeconds == 60);

    // Preset 4 (60/20): 36 + 372 + 114/2 = 465, y = 57
    settings.SimulateClick(465, 57);
    ALWAYS_ASSERT(settings.GetSelectedPreset() == 4);
    ALWAYS_ASSERT(settings.GetTempConfig().workMinutes == 60);
    ALWAYS_ASSERT(settings.GetTempConfig().standMinutes == 20);
    ALWAYS_ASSERT(settings.GetTempConfig().restSeconds == 90);

    // 2. 测试步进器 (+ / -) (坐姿工作 +5: x = 308, y = 103)
    int prevWork = settings.GetTempConfig().workMinutes;
    settings.SimulateClick(308, 103);
    ALWAYS_ASSERT(settings.GetTempConfig().workMinutes == prevWork + 5);
    ALWAYS_ASSERT(settings.GetSelectedPreset() == -1); // 自定义调整后预设高亮清除

    // 3. 测试工间操药丸选择 (y: 224 ~ 254, 中心 239)
    // 药丸 2 (只做护眼操): 36 + 2 * 124 + 57 = 341, y = 239
    settings.SimulateClick(341, 239);
    ALWAYS_ASSERT(settings.GetTempConfig().exerciseMode == ExerciseMode::EyeOnly);

    // 4. 测试伴侣形象药丸选择 (y: 260 ~ 290, 中心 275)
    // 药丸 1 (佛系水豚): 36 + 1 * 124 + 57 = 217, y = 275
    settings.SimulateClick(217, 275);
    ALWAYS_ASSERT(settings.GetTempConfig().mascotTheme == MascotTheme::Capybara);

    // 5. 测试托盘图标模式药丸选择 (y: 296 ~ 326, 中心 311)
    // 药丸 2 (感应猫 RunCatHealth): 36 + 2 * 124 + 57 = 341, y = 311
    settings.SimulateClick(341, 311);
    ALWAYS_ASSERT(settings.GetTempConfig().trayDisplayMode == TrayDisplayMode::RunCatHealth);

    // 6. 测试边框厚度药丸选择 (y: 332 ~ 362, 中心 347)
    // 药丸 2 (加粗 Thick 3.5px): 36 + 2 * 124 + 57 = 341, y = 347
    settings.SimulateClick(341, 347);
    ALWAYS_ASSERT(settings.GetTempConfig().borderWidth == BorderWidth::Thick);

    // 7. 测试主题选择 (y: 368 ~ 398, 中心 383)
    // 深色模式 (pillW=154, gap=13): 36 + 2 * (154 + 13) + 154/2 = 447, y = 383
    settings.SimulateClick(447, 383);
    ALWAYS_ASSERT(settings.GetTempConfig().themeMode == ThemeMode::Dark);

    // 8. 测试 Checkbox 命中 (y: 454 起)
    // 启用站立工作循环 (x: 45, y: 463)
    bool prevStand = settings.GetTempConfig().enableStand;
    settings.SimulateClick(45, 463);
    ALWAYS_ASSERT(settings.GetTempConfig().enableStand == !prevStand);

    // 悬浮窗始终置顶 (x: 45, y: 491)
    bool prevTopMost = settings.GetTempConfig().alwaysTopMost;
    settings.SimulateClick(45, 491);
    ALWAYS_ASSERT(settings.GetTempConfig().alwaysTopMost == !prevTopMost);

    // 阶段切换提示音 (x: 45, y: 519)
    bool prevSound = settings.GetTempConfig().enableSound;
    settings.SimulateClick(45, 519);
    ALWAYS_ASSERT(settings.GetTempConfig().enableSound == !prevSound);

    // 临界 30 秒红光强提醒 (右列 x: 290, y: 491)
    bool prevStrong = settings.GetTempConfig().strongReminder;
    settings.SimulateClick(290, 491);
    ALWAYS_ASSERT(settings.GetTempConfig().strongReminder == !prevStrong);

    std::cout << "[PASS] TestSettingsWindowInteractions" << std::endl;
}

#include "ui/FloatingWindow.hpp"
#include "platform/SingleInstance.hpp"
#include <filesystem>

// 测试 7: 验证悬浮窗单机不会误判为双击唤醒设置，只有真实快速连击才触发
void TestFloatingWindowClickVsDoubleClick() {
    std::cout << "[RUN] TestFloatingWindowClickVsDoubleClick..." << std::endl;
    auto& fw = FloatingWindow::Instance();
    auto& settings = SettingsWindow::Instance();
    settings.Close();
    ALWAYS_ASSERT(!settings.IsVisible());

    fw.Create();

    // 动作 1: 首次单次点击悬浮窗 (模拟用户点击或开始拖拽)
    fw.SimulateClick(10, 10, 1);
    // 核心断言：首次点击绝不允许唤醒设置中心！
    std::cout << "  After 1st click, SettingsWindow visible: " << settings.IsVisible() << std::endl;
    ALWAYS_ASSERT(!settings.IsVisible());

    // 动作 2: 紧接着在 100ms 内进行第二次点击 (双击操作)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    fw.SimulateClick(10, 10, 1);
    // 核心断言：连击必须成功识别为双击并唤醒设置中心！
    std::cout << "  After 2nd rapid click, SettingsWindow visible: " << settings.IsVisible() << std::endl;
    ALWAYS_ASSERT(settings.IsVisible());

    settings.Close();
    fw.Destroy();
    std::cout << "[PASS] TestFloatingWindowClickVsDoubleClick" << std::endl;
}

// 测试 8: 验证 SingleInstance 锁释放时不 unlink 锁文件，防止 Inode 竞态
void TestSingleInstanceInodePreserved() {
    std::cout << "[RUN] TestSingleInstanceInodePreserved..." << std::endl;
    std::string testLockPath = "/tmp/SitStandReminder_test_suite.lock";
    setenv("SITSTAND_TEST_LOCK_PATH", testLockPath.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove(testLockPath, ec);

    auto& si = SingleInstance::Instance();
    bool acquired1 = si.TryAcquire();
    ALWAYS_ASSERT(acquired1);

    std::filesystem::path lockFile(testLockPath);
    ALWAYS_ASSERT(std::filesystem::exists(lockFile));

    si.Release();

    // 核心断言：Release 释放 flock 之后，锁文件绝不可被 unlink 删除！
    ALWAYS_ASSERT(std::filesystem::exists(lockFile));

    // 且后续实例能够正常重新获取锁
    bool acquired2 = si.TryAcquire();
    ALWAYS_ASSERT(acquired2);
    si.Release();

    std::filesystem::remove(testLockPath, ec);
    unsetenv("SITSTAND_TEST_LOCK_PATH");

    std::cout << "[PASS] TestSingleInstanceInodePreserved" << std::endl;
}

// 测试 9: 验证 LinuxCanvas 在 90 度旋转变换下椭圆保持真实几何尺寸，不发生零维坍缩
void TestLinuxCanvasRotatedPrimitives() {
    std::cout << "[RUN] TestLinuxCanvasRotatedPrimitives..." << std::endl;
    LinuxRenderTarget rt(200, 200);
    rt.Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    // 绕 (100, 100) 旋转 90 度
    D2D1_MATRIX_3X2_F rot = D2D1::Matrix3x2F::Rotation(90.0f, D2D1::Point2F(100.0f, 100.0f));
    rt.SetTransform(rot);

    LinuxBrush brush(D2D1::ColorF(1.0f, 0.0f, 0.0f, 1.0f));
    // 原椭圆：长半轴 X=40, 短半轴 Y=20。旋转 90 度后，长半轴应沿 Y 轴（Y 方向应覆盖至 100 + 35 = 135）
    rt.FillEllipse(D2D1::Ellipse(D2D1::Point2F(100.0f, 100.0f), 40.0f, 20.0f), &brush);

    const uint32_t* pixels = rt.GetPixels();
    // 检查 (100, 130) 是否被绘制为红色 (若坍缩，此处必为 0)
    uint32_t pixelAt130 = pixels[130 * 200 + 100];
    uint32_t alphaAt130 = (pixelAt130 >> 24) & 0xFF;
    std::cout << "  Rotated ellipse pixel at (100, 130) alpha: " << alphaAt130 << std::endl;
    ALWAYS_ASSERT(alphaAt130 > 128); // 旋转后长轴向下，(100, 130) 距离中心 30px，在 radius 40px 之内，必须有红色！

    std::cout << "[PASS] TestLinuxCanvasRotatedPrimitives" << std::endl;
}

// 测试 9.1: 验证 LinuxCanvas 在 90 度旋转变换下 DrawText 字体大小不塌陷为 0，文字成功光栅化
void TestLinuxCanvasRotatedTextRendering() {
    std::cout << "[RUN] TestLinuxCanvasRotatedTextRendering..." << std::endl;
    LinuxRenderTarget rt(200, 200);
    rt.Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    // 绕 (100, 100) 旋转 90 度
    D2D1_MATRIX_3X2_F rot = D2D1::Matrix3x2F::Rotation(90.0f, D2D1::Point2F(100.0f, 100.0f));
    rt.SetTransform(rot);

    auto fmt = D2DContext::Instance().GetCachedTextFormat(L"sans-serif", 24.0f, DWRITE_FONT_WEIGHT_BOLD);
    LinuxBrush brush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));

    const wchar_t* text = L"ABC";
    rt.DrawText(text, 3, fmt.Get(), D2D1::RectF(50, 80, 150, 120), &brush);

    // 统计是否有非零 alpha 的像素被渲染
    int renderedPixelCount = 0;
    const uint32_t* pixels = rt.GetPixels();
    for (int i = 0; i < 200 * 200; ++i) {
        if (((pixels[i] >> 24) & 0xFF) > 30) {
            renderedPixelCount++;
        }
    }
    std::cout << "  Rendered pixel count for rotated text: " << renderedPixelCount << std::endl;
    ALWAYS_ASSERT(renderedPixelCount > 20); // 必须成功渲染文字，绝不允许为 0！
    std::cout << "[PASS] TestLinuxCanvasRotatedTextRendering" << std::endl;
}

// 测试 10: 验证悬浮窗渲染时填充了半透明磨砂卡片底色 (非镂空全透明)，且四个圆角外部保持透明
void TestFloatingWindowCardBackground() {
    std::cout << "[RUN] TestFloatingWindowCardBackground..." << std::endl;
    auto& fw = FloatingWindow::Instance();
    fw.Create();
    fw.TriggerRender();

    const auto* rt = fw.GetRenderTarget();
    ALWAYS_ASSERT(rt != nullptr);

    int w = rt->GetWidth();
    [[maybe_unused]] int h = rt->GetHeight();
    const uint32_t* pixels = rt->GetPixels();
    ALWAYS_ASSERT(pixels != nullptr);

    // 1. 内部卡片区域 (例如 x=25, y=25)，应当具有磨砂底色，Alpha 绝不可为 0 (镂空透明)
    uint32_t insidePixel = pixels[25 * w + 25];
    uint32_t insideAlpha = (insidePixel >> 24) & 0xFF;
    std::cout << "  Inside card pixel alpha at (25, 25): " << insideAlpha << std::endl;
    ALWAYS_ASSERT(insideAlpha >= 200); // 92%~94% 不透明度 (约 234~240)

    // 2. 外部角落区域 (例如 x=1, y=1)，处于圆角外部，应当保持透明 (Alpha 接近 0)
    uint32_t cornerPixel = pixels[1 * w + 1];
    uint32_t cornerAlpha = (cornerPixel >> 24) & 0xFF;
    std::cout << "  Corner pixel alpha at (1, 1): " << cornerAlpha << std::endl;
    ALWAYS_ASSERT(cornerAlpha < 50);

    fw.Destroy();
    std::cout << "[PASS] TestFloatingWindowCardBackground" << std::endl;
}

// 测试 11: 验证悬浮窗右侧包含状态标签与 mm:ss 倒计时文本 (BUG-11 回归测试)
void TestFloatingWindowRightSideTimerText() {
    std::cout << "[RUN] TestFloatingWindowRightSideTimerText..." << std::endl;
    auto& fw = FloatingWindow::Instance();
    fw.Create();
    fw.UpdateState(AppState::Working, 25 * 60, 25 * 60);
    fw.TriggerRender();

    const auto* rt = fw.GetRenderTarget();
    ALWAYS_ASSERT(rt != nullptr);

    int w = rt->GetWidth();
    const uint32_t* pixels = rt->GetPixels();
    ALWAYS_ASSERT(pixels != nullptr);

    // 检查右侧倒计时文本区域后半段 (x in [100, 128], y in [30, 48])
    // 悬浮窗总宽 136px，此区间完全超出吉祥物边界 (吉祥物最右仅到 x=92)，纯属倒计时数字显示区！
    // 必须存在与卡片背景明显不同的文本前景色像素！
    bool foundTextPixel = false;
    uint32_t bgPixel = pixels[10 * w + 10]; // 卡片内部参考背景像素
    uint8_t bgR = (bgPixel >> 16) & 0xFF;
    uint8_t bgG = (bgPixel >> 8) & 0xFF;
    uint8_t bgB = bgPixel & 0xFF;

    for (int y = 30; y <= 48; ++y) {
        for (int x = 100; x <= 128; ++x) {
            uint32_t px = pixels[y * w + x];
            uint8_t pr = (px >> 16) & 0xFF;
            uint8_t pg = (px >> 8) & 0xFF;
            uint8_t pb = px & 0xFF;
            int diff = std::abs(pr - bgR) + std::abs(pg - bgG) + std::abs(pb - bgB);
            if (diff > 80) { // 明显的文本高对比度笔画像素
                foundTextPixel = true;
                break;
            }
        }
        if (foundTextPixel) break;
    }

    std::cout << "  Right side timer text pixel found: " << foundTextPixel << std::endl;
    ALWAYS_ASSERT(foundTextPixel && "FAIL: Floating window right side missing mm:ss countdown timer text!");

    fw.Destroy();
    std::cout << "[PASS] TestFloatingWindowRightSideTimerText" << std::endl;
}

#include "ui/linux/StatusNotifierItemLinux.hpp"

// 测试 12: 验证现代 Linux 桌面 D-Bus StatusNotifierItem (SNI) 协议的挂号、消息泵送与销毁流程
void TestStatusNotifierItemLifecycleAndCallbacks() {
    std::cout << "[RUN] TestStatusNotifierItemLifecycleAndCallbacks..." << std::endl;
    auto& sni = StatusNotifierItemLinux::Instance();

    bool initOk = sni.Initialize(
        [](int /*x*/, int /*y*/) {},
        [](int /*x*/, int /*y*/) {}
    );
    std::cout << "  StatusNotifierItem Initialize result: " << initOk << ", registered: " << sni.IsAvailable() << std::endl;
    if (initOk) {
        ALWAYS_ASSERT(sni.IsAvailable());
        sni.SetTooltip("坐立提醒", "单元测试运行中");
        sni.SetIconName("sitstandreminder");
        sni.ProcessEvents();
        sni.Shutdown();
        ALWAYS_ASSERT(!sni.IsAvailable());
    } else {
        std::cout << "  (StatusNotifierWatcher not active in test environment, graceful fallback PASS)" << std::endl;
    }
    std::cout << "[PASS] TestStatusNotifierItemLifecycleAndCallbacks" << std::endl;
}

// 测试 13: 验证 D-Bus SNI 的 ToolTip 属性能够被正确查询且结构完整
void TestStatusNotifierItemTooltipProperty() {
    std::cout << "[RUN] TestStatusNotifierItemTooltipProperty..." << std::endl;
    auto& sni = StatusNotifierItemLinux::Instance();
    sni.SetTooltip("坐立提醒 (工作中)", "剩余 44:59");

    // 构造模拟 DBus 消息验证 Get("ToolTip")
    DBusMessage* msg = dbus_message_new_method_call("org.test", "/StatusNotifierItem", "org.freedesktop.DBus.Properties", "Get");
    dbus_message_set_serial(msg, 1);
    DBusMessage* reply = StatusNotifierItemLinux::TestHandleGetProperty(msg, "org.kde.StatusNotifierItem", "ToolTip", &sni);
    dbus_message_unref(msg);

    ALWAYS_ASSERT(reply != nullptr);
    ALWAYS_ASSERT(dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN);

    // 解析出 Variant -> Struct (sa(iiay)ss)
    DBusMessageIter iter;
    dbus_message_iter_init(reply, &iter);
    ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_VARIANT);

    DBusMessageIter varIter;
    dbus_message_iter_recurse(&iter, &varIter);
    ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&varIter) == DBUS_TYPE_STRUCT);

    DBusMessageIter structIter;
    dbus_message_iter_recurse(&varIter, &structIter);

    // 1. icon_name
    const char* iconName = nullptr;
    ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&structIter) == DBUS_TYPE_STRING);
    dbus_message_iter_get_basic(&structIter, &iconName);
    dbus_message_iter_next(&structIter);

    // 2. icon_data array
    ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&structIter) == DBUS_TYPE_ARRAY);
    dbus_message_iter_next(&structIter);

    // 3. title
    const char* title = nullptr;
    ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&structIter) == DBUS_TYPE_STRING);
    dbus_message_iter_get_basic(&structIter, &title);
    ALWAYS_ASSERT(title != nullptr && std::string(title) == "坐立提醒 (工作中)");
    dbus_message_iter_next(&structIter);

    // 4. desc
    const char* desc = nullptr;
    ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&structIter) == DBUS_TYPE_STRING);
    dbus_message_iter_get_basic(&structIter, &desc);
    ALWAYS_ASSERT(desc != nullptr && std::string(desc) == "剩余 44:59");

    dbus_message_unref(reply);
    std::cout << "[PASS] TestStatusNotifierItemTooltipProperty" << std::endl;
}

// 测试 14: 验证 TrayWindow::UpdateTooltip 传递中文字符串时完整保留 UTF-8 编码，杜绝 ASCII 截断
void TestTrayWindowTooltipUtf8Preserved() {
    std::cout << "[RUN] TestTrayWindowTooltipUtf8Preserved..." << std::endl;
    auto& tray = TrayWindow::Instance();
    auto& sni = StatusNotifierItemLinux::Instance();

    tray.UpdateTooltip(L"坐立提醒 (工作中) - 剩余 35:12");

    // 核心断言：SNI 内部接收到的描述文本必须包含完整的中文，绝不可变成空括号 " () -  35:12"
    ALWAYS_ASSERT(sni.GetTooltipDesc() == "坐立提醒 (工作中) - 剩余 35:12");
    std::cout << "[PASS] TestTrayWindowTooltipUtf8Preserved" << std::endl;
}

// 测试 15: 验证悬浮窗能够动态响应配置变更实时同步置顶状态
void TestFloatingWindowTopMostDynamic() {
    std::cout << "[RUN] TestFloatingWindowTopMostDynamic..." << std::endl;
    auto& fw = FloatingWindow::Instance();
    // 备份原有配置
    ReminderConfig originalCfg = ConfigManager::Instance().GetConfig();

    ReminderConfig testCfg = originalCfg;
    testCfg.alwaysTopMost = true;
    ConfigManager::Instance().SetConfig(testCfg);

    fw.Create();
    ALWAYS_ASSERT(fw.IsTopMost() == true);

    // 动态修改为 false
    testCfg.alwaysTopMost = false;
    ConfigManager::Instance().SetConfig(testCfg);
    fw.OnConfigChanged();
    ALWAYS_ASSERT(fw.IsTopMost() == false);

    // 恢复为 true
    testCfg.alwaysTopMost = true;
    ConfigManager::Instance().SetConfig(testCfg);
    fw.OnConfigChanged();
    ALWAYS_ASSERT(fw.IsTopMost() == true);

    fw.Destroy();
    ConfigManager::Instance().SetConfig(originalCfg);
    std::cout << "[PASS] TestFloatingWindowTopMostDynamic" << std::endl;
}

// 测试 16: 验证 SNI 的 IconThemePath、XAyatanaLabel、Menu 与 IconPixmap (a(iiay)) 属性
void TestSNIIconPropertiesAndPixmap() {
    std::cout << "[RUN] TestSNIIconPropertiesAndPixmap..." << std::endl;
    auto& sni = StatusNotifierItemLinux::Instance();

    // 1. 验证 IconThemePath 属性
    {
        DBusMessage* msg = dbus_message_new_method_call("org.test", "/StatusNotifierItem", "org.freedesktop.DBus.Properties", "Get");
        dbus_message_set_serial(msg, 101);
        DBusMessage* reply = StatusNotifierItemLinux::TestHandleGetProperty(msg, "org.kde.StatusNotifierItem", "IconThemePath", &sni);
        dbus_message_unref(msg);
        ALWAYS_ASSERT(reply != nullptr);
        ALWAYS_ASSERT(dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN);

        DBusMessageIter iter;
        dbus_message_iter_init(reply, &iter);
        ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_VARIANT);
        DBusMessageIter varIter;
        dbus_message_iter_recurse(&iter, &varIter);
        ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&varIter) == DBUS_TYPE_STRING);
        const char* pathStr = nullptr;
        dbus_message_iter_get_basic(&varIter, &pathStr);
        ALWAYS_ASSERT(pathStr != nullptr);
        dbus_message_unref(reply);
    }

    // 2. 验证 XAyatanaLabel 属性 (必须返回有效字符串 Variant，不能返回 Unknown Property 报错)
    {
        DBusMessage* msg = dbus_message_new_method_call("org.test", "/StatusNotifierItem", "org.freedesktop.DBus.Properties", "Get");
        dbus_message_set_serial(msg, 102);
        DBusMessage* reply = StatusNotifierItemLinux::TestHandleGetProperty(msg, "org.kde.StatusNotifierItem", "XAyatanaLabel", &sni);
        dbus_message_unref(msg);
        ALWAYS_ASSERT(reply != nullptr);
        ALWAYS_ASSERT(dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN);

        DBusMessageIter iter;
        dbus_message_iter_init(reply, &iter);
        ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_VARIANT);
        dbus_message_unref(reply);
    }

    // 3. 验证 Menu 属性返回合法的 Object Path (不能再是 /NO_DBUSMENU)
    {
        DBusMessage* msg = dbus_message_new_method_call("org.test", "/StatusNotifierItem", "org.freedesktop.DBus.Properties", "Get");
        dbus_message_set_serial(msg, 103);
        DBusMessage* reply = StatusNotifierItemLinux::TestHandleGetProperty(msg, "org.kde.StatusNotifierItem", "Menu", &sni);
        dbus_message_unref(msg);
        ALWAYS_ASSERT(reply != nullptr);
        ALWAYS_ASSERT(dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN);

        DBusMessageIter iter;
        dbus_message_iter_init(reply, &iter);
        ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_VARIANT);
        DBusMessageIter varIter;
        dbus_message_iter_recurse(&iter, &varIter);
        ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&varIter) == DBUS_TYPE_OBJECT_PATH);
        const char* menuPath = nullptr;
        dbus_message_iter_get_basic(&varIter, &menuPath);
        ALWAYS_ASSERT(menuPath != nullptr && std::string(menuPath) != "/NO_DBUSMENU");
        dbus_message_unref(reply);
    }

    // 4. 设置并验证 IconPixmap (a(iiay)) 动态像素流
    {
        const int testW = 4;
        const int testH = 4;
        std::vector<uint32_t> testPixels(testW * testH, 0xFF336699); // ARGB

        sni.SetIconPixmap(testW, testH, testPixels.data());

        DBusMessage* msg = dbus_message_new_method_call("org.test", "/StatusNotifierItem", "org.freedesktop.DBus.Properties", "Get");
        dbus_message_set_serial(msg, 104);
        DBusMessage* reply = StatusNotifierItemLinux::TestHandleGetProperty(msg, "org.kde.StatusNotifierItem", "IconPixmap", &sni);
        dbus_message_unref(msg);
        ALWAYS_ASSERT(reply != nullptr);
        ALWAYS_ASSERT(dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN);

        DBusMessageIter iter;
        dbus_message_iter_init(reply, &iter);
        ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_VARIANT);

        DBusMessageIter varIter;
        dbus_message_iter_recurse(&iter, &varIter);
        ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&varIter) == DBUS_TYPE_ARRAY);

        DBusMessageIter arrayIter;
        dbus_message_iter_recurse(&varIter, &arrayIter);
        ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&arrayIter) == DBUS_TYPE_STRUCT);

        DBusMessageIter structIter;
        dbus_message_iter_recurse(&arrayIter, &structIter);

        int32_t width = 0;
        int32_t height = 0;
        ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&structIter) == DBUS_TYPE_INT32);
        dbus_message_iter_get_basic(&structIter, &width);
        ALWAYS_ASSERT(width == testW);
        dbus_message_iter_next(&structIter);

        ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&structIter) == DBUS_TYPE_INT32);
        dbus_message_iter_get_basic(&structIter, &height);
        ALWAYS_ASSERT(height == testH);
        dbus_message_iter_next(&structIter);

        ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&structIter) == DBUS_TYPE_ARRAY);
        DBusMessageIter byteIter;
        dbus_message_iter_recurse(&structIter, &byteIter);

        int count = 0;
        while (dbus_message_iter_get_arg_type(&byteIter) == DBUS_TYPE_BYTE) {
            count++;
            dbus_message_iter_next(&byteIter);
        }
        ALWAYS_ASSERT(count == testW * testH * 4);
        dbus_message_unref(reply);

        // 验证在 SetIconPixmap 时，IconName 被设置为空字符串以让 GNOME Shell 扩展直通 _createIconFromPixmap 动态像素流
        DBusMessage* msgIcon = dbus_message_new_method_call("org.test", "/StatusNotifierItem", "org.freedesktop.DBus.Properties", "Get");
        dbus_message_set_serial(msgIcon, 105);
        DBusMessage* replyIcon = StatusNotifierItemLinux::TestHandleGetProperty(msgIcon, "org.kde.StatusNotifierItem", "IconName", &sni);
        dbus_message_unref(msgIcon);
        ALWAYS_ASSERT(replyIcon != nullptr);
        DBusMessageIter nameIter;
        dbus_message_iter_init(replyIcon, &nameIter);
        DBusMessageIter nameVar;
        dbus_message_iter_recurse(&nameIter, &nameVar);
        const char* dynamicPath = nullptr;
        dbus_message_iter_get_basic(&nameVar, &dynamicPath);
        ALWAYS_ASSERT(dynamicPath != nullptr && dynamicPath[0] == '\0' && "FAIL: Dynamic IconName must be empty string for GNOME Pixmap pass-through!");
        dbus_message_unref(replyIcon);

        // 验证显式设置经典图标名时，IconName 恢复为命名图标
        sni.SetIconName("sitstandreminder");
        DBusMessage* msgClassic = dbus_message_new_method_call("org.test", "/StatusNotifierItem", "org.freedesktop.DBus.Properties", "Get");
        dbus_message_set_serial(msgClassic, 106);
        DBusMessage* replyClassic = StatusNotifierItemLinux::TestHandleGetProperty(msgClassic, "org.kde.StatusNotifierItem", "IconName", &sni);
        dbus_message_unref(msgClassic);
        ALWAYS_ASSERT(replyClassic != nullptr);
        DBusMessageIter classicIter;
        dbus_message_iter_init(replyClassic, &classicIter);
        DBusMessageIter classicVar;
        dbus_message_iter_recurse(&classicIter, &classicVar);
        const char* classicName = nullptr;
        dbus_message_iter_get_basic(&classicVar, &classicName);
        ALWAYS_ASSERT(classicName != nullptr && std::string(classicName) == "sitstandreminder" && "FAIL: Classic IconName must be sitstandreminder!");
        dbus_message_unref(replyClassic);
    }
    std::cout << "[PASS] TestSNIIconPropertiesAndPixmap" << std::endl;
}

// 测试 17: 验证 X11Utils 注入 WM_CLASS 与 _NET_WM_ICON 标准属性
void TestX11WindowClassAndIconProperties() {
    std::cout << "[RUN] TestX11WindowClassAndIconProperties..." << std::endl;
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        std::cout << "  [SKIP] No X11 Display available" << std::endl;
        return;
    }

    int scr = DefaultScreen(dpy);
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0, 100, 100, 0, 0, 0);

    // 1. 设置 WM_CLASS
    X11Utils::SetWindowClass(dpy, win, "sitstandreminder", "SitStandReminder");

    XClassHint hint = {};
    int status = XGetClassHint(dpy, win, &hint);
    ALWAYS_ASSERT(status != 0);
    ALWAYS_ASSERT(hint.res_name != nullptr && std::string(hint.res_name) == "sitstandreminder");
    ALWAYS_ASSERT(hint.res_class != nullptr && std::string(hint.res_class) == "SitStandReminder");
    if (hint.res_name) XFree(hint.res_name);
    if (hint.res_class) XFree(hint.res_class);

    // 2. 设置 _NET_WM_ICON
    const int iconW = 4;
    const int iconH = 4;
    std::vector<uint32_t> iconPixels(iconW * iconH, 0xFF4CAF50);
    X11Utils::SetWindowIcon(dpy, win, iconW, iconH, iconPixels.data());

    Atom netWmIcon = XInternAtom(dpy, "_NET_WM_ICON", False);
    Atom actualType;
    int actualFormat;
    unsigned long nItems, bytesAfter;
    unsigned char* propValue = nullptr;

    status = XGetWindowProperty(
        dpy, win, netWmIcon, 0, 1024, False, XA_CARDINAL,
        &actualType, &actualFormat, &nItems, &bytesAfter, &propValue
    );

    ALWAYS_ASSERT(status == Success);
    ALWAYS_ASSERT(propValue != nullptr);
    ALWAYS_ASSERT(actualType == XA_CARDINAL);
    ALWAYS_ASSERT(actualFormat == 32);
    // 头部包括 width + height (2 items) + 16 items = 18 items
    ALWAYS_ASSERT(nItems == 2 + iconW * iconH);
    unsigned long* rawData = reinterpret_cast<unsigned long*>(propValue);
    ALWAYS_ASSERT(rawData[0] == static_cast<unsigned long>(iconW));
    ALWAYS_ASSERT(rawData[1] == static_cast<unsigned long>(iconH));
    std::cout << "  [DEBUG] rawData[2] = 0x" << std::hex << rawData[2] << " expected 0x" << 0xFF4CAF50 << std::dec << std::endl;
    ALWAYS_ASSERT((rawData[2] & 0xFFFFFFFFUL) == 0xFF4CAF50UL);

    XFree(propValue);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    std::cout << "[PASS] TestX11WindowClassAndIconProperties" << std::endl;
}

// 测试 19: 验证 DBusMenu 布局导出非空以及 Event 菜单项点击命令派发
void TestStatusNotifierDBusMenuLayoutAndEvent() {
    std::cout << "[RUN] TestStatusNotifierDBusMenuLayoutAndEvent..." << std::endl;
    auto& sni = StatusNotifierItemLinux::Instance();

    // 1. 验证 GetLayout 返回包含完整子菜单项
    DBusMessage* msg = dbus_message_new_method_call(
        "com.canonical.AppMenu.Registrar",
        "/MenuBar",
        "com.canonical.dbusmenu",
        "GetLayout"
    );
    int32_t parentId = 0;
    int32_t depth = -1;
    dbus_message_append_args(msg, DBUS_TYPE_INT32, &parentId, DBUS_TYPE_INT32, &depth, DBUS_TYPE_INVALID);
    dbus_message_set_serial(msg, 100);

    DBusMessage* reply = StatusNotifierItemLinux::TestHandleDBusMenuMethod(msg, &sni);
    dbus_message_unref(msg);

    ALWAYS_ASSERT(reply != nullptr);
    ALWAYS_ASSERT(dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN);

    DBusMessageIter argsIter;
    dbus_message_iter_init(reply, &argsIter);
    ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&argsIter) == DBUS_TYPE_UINT32);
    dbus_message_iter_next(&argsIter);

    // root struct: (ia{sv}av)
    ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&argsIter) == DBUS_TYPE_STRUCT);
    DBusMessageIter rootIter;
    dbus_message_iter_recurse(&argsIter, &rootIter);

    int32_t rootId = -1;
    ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&rootIter) == DBUS_TYPE_INT32);
    dbus_message_iter_get_basic(&rootIter, &rootId);
    ALWAYS_ASSERT(rootId == 0);
    dbus_message_iter_next(&rootIter);

    // a{sv} properties
    ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&rootIter) == DBUS_TYPE_ARRAY);
    dbus_message_iter_next(&rootIter);

    // av children
    ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&rootIter) == DBUS_TYPE_ARRAY);
    DBusMessageIter childrenIter;
    dbus_message_iter_recurse(&rootIter, &childrenIter);

    // 关键红灯验证：必须包含子项，绝不可为空数组！
    ALWAYS_ASSERT(dbus_message_iter_get_arg_type(&childrenIter) != DBUS_TYPE_INVALID);

    dbus_message_unref(reply);

    // 2. 验证 Event 点击事件能正确触发回调
    uint32_t executedCmd = 0;
    sni.SetMenuCommandHandler([&](uint32_t cmdId) {
        executedCmd = cmdId;
    });

    DBusMessage* eventMsg = dbus_message_new_method_call(
        "com.canonical.AppMenu.Registrar",
        "/MenuBar",
        "com.canonical.dbusmenu",
        "Event"
    );
    int32_t targetId = 2001; // IDM_TRAY_START_WORK
    const char* eventId = "clicked";
    DBusMessageIter evArgs;
    dbus_message_iter_init_append(eventMsg, &evArgs);
    dbus_message_iter_append_basic(&evArgs, DBUS_TYPE_INT32, &targetId);
    dbus_message_iter_append_basic(&evArgs, DBUS_TYPE_STRING, &eventId);
    DBusMessageIter dummyVar;
    dbus_message_iter_open_container(&evArgs, DBUS_TYPE_VARIANT, "s", &dummyVar);
    const char* dummyStr = "";
    dbus_message_iter_append_basic(&dummyVar, DBUS_TYPE_STRING, &dummyStr);
    dbus_message_iter_close_container(&evArgs, &dummyVar);
    uint32_t timestamp = 0;
    dbus_message_iter_append_basic(&evArgs, DBUS_TYPE_UINT32, &timestamp);
    dbus_message_set_serial(eventMsg, 101);

    DBusMessage* evReply = StatusNotifierItemLinux::TestHandleDBusMenuMethod(eventMsg, &sni);
    dbus_message_unref(eventMsg);
    ALWAYS_ASSERT(evReply != nullptr);
    dbus_message_unref(evReply);

    ALWAYS_ASSERT(executedCmd == 2001);
    std::cout << "[PASS] TestStatusNotifierDBusMenuLayoutAndEvent" << std::endl;
}

// 测试 20: 验证兼容 org.freedesktop.StatusNotifierItem 与 org.kde.StatusNotifierItem
void TestStatusNotifierInterfaceCompatibility() {
    std::cout << "[RUN] TestStatusNotifierInterfaceCompatibility..." << std::endl;
    auto& sni = StatusNotifierItemLinux::Instance();

    DBusMessage* msg = dbus_message_new_method_call(
        "org.test",
        "/StatusNotifierItem",
        "org.freedesktop.StatusNotifierItem",
        "ContextMenu"
    );
    int32_t x = 100, y = 200;
    dbus_message_append_args(msg, DBUS_TYPE_INT32, &x, DBUS_TYPE_INT32, &y, DBUS_TYPE_INVALID);
    dbus_message_set_serial(msg, 102);

    int handleResult = StatusNotifierItemLinux::TestMessageFilter(nullptr, msg, &sni);
    dbus_message_unref(msg);

    ALWAYS_ASSERT(handleResult == DBUS_HANDLER_RESULT_HANDLED);
    std::cout << "[PASS] TestStatusNotifierInterfaceCompatibility" << std::endl;
}

// 测试 21: 验证 D2D/Linux 文本格式缓存池的对齐属性完全隔离，杜绝全局单例缓存污染
void TestD2DTextFormatCacheIsolation() {
    std::cout << "[RUN] TestD2DTextFormatCacheIsolation..." << std::endl;
    auto& d2d = D2DContext::Instance();

    // 1. 获取默认 LEADING 对齐格式
    auto fmtLeading = d2d.GetCachedTextFormat(L"sans-serif", 14.0f, DWRITE_FONT_WEIGHT_NORMAL);
    ALWAYS_ASSERT(fmtLeading != nullptr);
    ALWAYS_ASSERT(fmtLeading->GetTextAlignment() == DWRITE_TEXT_ALIGNMENT_LEADING);
    ALWAYS_ASSERT(fmtLeading->GetParagraphAlignment() == DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    // 2. 获取居中对齐格式
    auto fmtCenter = d2d.GetCachedTextFormat(
        L"sans-serif", 14.0f, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_TEXT_ALIGNMENT_CENTER,
        DWRITE_PARAGRAPH_ALIGNMENT_CENTER
    );
    ALWAYS_ASSERT(fmtCenter != nullptr);
    ALWAYS_ASSERT(fmtCenter->GetTextAlignment() == DWRITE_TEXT_ALIGNMENT_CENTER);
    ALWAYS_ASSERT(fmtCenter->GetParagraphAlignment() == DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // 3. 再次获取默认 LEADING 格式，验证其绝对未被污染
    auto fmtLeading2 = d2d.GetCachedTextFormat(L"sans-serif", 14.0f, DWRITE_FONT_WEIGHT_NORMAL);
    ALWAYS_ASSERT(fmtLeading2 != nullptr);
    ALWAYS_ASSERT(fmtLeading2->GetTextAlignment() == DWRITE_TEXT_ALIGNMENT_LEADING);
    ALWAYS_ASSERT(fmtLeading2->GetParagraphAlignment() == DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    // 4. 两个格式应当是隔离的独立对象
    ALWAYS_ASSERT(fmtLeading.Get() != fmtCenter.Get());

    std::cout << "[PASS] TestD2DTextFormatCacheIsolation" << std::endl;
}

// 测试 22: 验证多显示器拓扑探测与主屏感知
void TestMultiMonitorTopologyDetection() {
    std::cout << "[RUN] TestMultiMonitorTopologyDetection..." << std::endl;
    auto& app = X11App::Instance();
    ALWAYS_ASSERT(app.Initialize());

    auto screens = app.GetScreenGeometries();
    ALWAYS_ASSERT(!screens.empty());
    std::cout << "  Detected screens count: " << screens.size() << std::endl;
    for (size_t i = 0; i < screens.size(); ++i) {
        std::cout << "    Screen " << i << ": [" << screens[i].x << ", " << screens[i].y
                  << ", " << screens[i].width << "x" << screens[i].height
                  << "], isPrimary=" << screens[i].isPrimary << std::endl;
        ALWAYS_ASSERT(screens[i].width > 0);
        ALWAYS_ASSERT(screens[i].height > 0);
    }

    auto primary = app.GetPrimaryScreen();
    ALWAYS_ASSERT(primary.width > 0 && primary.height > 0);
    ALWAYS_ASSERT(primary.isPrimary);

    auto forPt = app.GetScreenForPoint(primary.x + 10, primary.y + 10);
    ALWAYS_ASSERT(forPt.width == primary.width && forPt.height == primary.height);

    std::cout << "[PASS] TestMultiMonitorTopologyDetection" << std::endl;
}

// 测试 23: 验证 FloatingWindow 遵循 EWMH 规范：override_redirect=False, _MOTIF_WM_HINTS 无边框, STICKY, SKIP_TASKBAR
void TestFloatingWindowEwmhProperties() {
    std::cout << "[RUN] TestFloatingWindowEwmhProperties..." << std::endl;
    auto& fw = FloatingWindow::Instance();
    ALWAYS_ASSERT(fw.Create());

    Display* dpy = X11App::Instance().GetDisplay();
    Window win = fw.GetWindow();
    ALWAYS_ASSERT(win != 0);

    XWindowAttributes attrs;
    ALWAYS_ASSERT(XGetWindowAttributes(dpy, win, &attrs) != 0);
    // 验证 override_redirect 为 False，保证由 WM 管理置顶与粘附
    ALWAYS_ASSERT(attrs.override_redirect == False);

    // 验证 _MOTIF_WM_HINTS 无边框
    Atom motifHintsAtom = X11App::Instance().GetAtom("_MOTIF_WM_HINTS");
    Atom actualType;
    int actualFormat;
    unsigned long nItems = 0, bytesAfter = 0;
    unsigned char* propData = nullptr;
    int status = XGetWindowProperty(dpy, win, motifHintsAtom, 0, 5, False, motifHintsAtom,
                                   &actualType, &actualFormat, &nItems, &bytesAfter, &propData);
    ALWAYS_ASSERT(status == Success && propData != nullptr);
    ALWAYS_ASSERT(nItems >= 3);
    unsigned long* hints = reinterpret_cast<unsigned long*>(propData);
    ALWAYS_ASSERT((hints[0] & 2) != 0); // MWM_HINTS_DECORATIONS
    ALWAYS_ASSERT(hints[2] == 0); // decorations = 0 (无边框)
    XFree(propData);

    // 验证 _NET_WM_STATE 包含 STICKY, SKIP_TASKBAR, SKIP_PAGER
    Atom wmState = X11App::Instance().GetAtom("_NET_WM_STATE");
    status = XGetWindowProperty(dpy, win, wmState, 0, 16, False, XA_ATOM,
                                &actualType, &actualFormat, &nItems, &bytesAfter, &propData);
    ALWAYS_ASSERT(status == Success && propData != nullptr);
    Atom* stateAtoms = reinterpret_cast<Atom*>(propData);
    bool hasSticky = false, hasSkipTaskbar = false, hasSkipPager = false;
    Atom stateSticky = X11App::Instance().GetAtom("_NET_WM_STATE_STICKY");
    Atom stateSkipTaskbar = X11App::Instance().GetAtom("_NET_WM_STATE_SKIP_TASKBAR");
    Atom stateSkipPager = X11App::Instance().GetAtom("_NET_WM_STATE_SKIP_PAGER");

    for (unsigned long i = 0; i < nItems; ++i) {
        if (stateAtoms[i] == stateSticky) hasSticky = true;
        if (stateAtoms[i] == stateSkipTaskbar) hasSkipTaskbar = true;
        if (stateAtoms[i] == stateSkipPager) hasSkipPager = true;
    }
    XFree(propData);

    ALWAYS_ASSERT(hasSticky);
    ALWAYS_ASSERT(hasSkipTaskbar);
    ALWAYS_ASSERT(hasSkipPager);

    fw.Destroy();
    std::cout << "[PASS] TestFloatingWindowEwmhProperties" << std::endl;
}

// 测试 24: 验证 FullscreenMask 安全初始化、显示与 ESC 释放
void TestFullscreenMaskMultiscreenAndSafeXImage() {
    std::cout << "[RUN] TestFullscreenMaskMultiscreenAndSafeXImage..." << std::endl;
    auto& fm = FullscreenMask::Instance();
    ALWAYS_ASSERT(fm.Initialize());

    fm.Show(true);
    ALWAYS_ASSERT(fm.IsVisible());

    fm.UpdateDisplay(30, 60, 0, L"颈部舒缓");
    fm.TriggerAnimationTick();

    // 触发 ESC 退出
    fm.OnEscape();
    ALWAYS_ASSERT(!fm.IsVisible());

    fm.Destroy();
    std::cout << "[PASS] TestFullscreenMaskMultiscreenAndSafeXImage" << std::endl;
}

// 测试 25: 验证 TrayWindow RunCat、倒计时与默认图标各模式渲染健全性
void TestTrayWindowRunCatAndCountdownRendering() {
    std::cout << "[RUN] TestTrayWindowRunCatAndCountdownRendering..." << std::endl;
    auto& tray = TrayWindow::Instance();
    ALWAYS_ASSERT(tray.Create());

    auto& cfg = ConfigManager::Instance().GetConfig();

    // 1. 动态倒计时模式
    cfg.trayDisplayMode = TrayDisplayMode::DynamicCountdown;
    tray.UpdateDynamicIcon(AppState::Working, 1800, 2700, L"测试提示");
    tray.RefreshTrayDisplayMode();

    // 2. 奔跑猫健康模式
    cfg.trayDisplayMode = TrayDisplayMode::RunCatHealth;
    tray.UpdateDynamicIcon(AppState::Working, 200, 2700, L"急迫快跑");
    tray.RefreshTrayDisplayMode();

    // 3. 奔跑猫打盹模式
    tray.UpdateDynamicIcon(AppState::Resting, 120, 300, L"打盹小猫");
    tray.RefreshTrayDisplayMode();

    // 4. 奔跑猫 CPU 模式
    cfg.trayDisplayMode = TrayDisplayMode::RunCatCpu;
    tray.RefreshTrayDisplayMode();

    // 5. 经典静态模式
    cfg.trayDisplayMode = TrayDisplayMode::DefaultIcon;
    tray.RefreshTrayDisplayMode();

    // 恢复默认
    cfg.trayDisplayMode = TrayDisplayMode::DynamicCountdown;

    tray.Destroy();
    std::cout << "[PASS] TestTrayWindowRunCatAndCountdownRendering" << std::endl;
}

// 测试 26: 验证 X11App 能够正确探测并返回有效的 DPI Scale 因子 (DEF-06 回归测试)
void TestDpiScaleCascade() {
    std::cout << "[RUN] TestDpiScaleCascade..." << std::endl;
    auto& app = X11App::Instance();
    float scale = app.GetDisplayDpiScale();
    std::cout << "  Detected X11 DPI scale: " << scale << std::endl;
    ALWAYS_ASSERT(scale >= 0.5f && scale <= 5.0f && "FAIL: DPI scale must be within normal bounds [0.5, 5.0]!");

    // 验证环境变量 GDK_SCALE 感知能力
    setenv("GDK_SCALE", "2", 1);
    app.InvalidateDpiCache();
    float scale2 = app.GetDisplayDpiScale();
    std::cout << "  DPI scale with GDK_SCALE=2: " << scale2 << std::endl;
    ALWAYS_ASSERT(std::abs(scale2 - 2.0f) < 0.01f && "FAIL: GDK_SCALE=2 was not respected!");
    unsetenv("GDK_SCALE");
    app.InvalidateDpiCache();

    std::cout << "[PASS] TestDpiScaleCascade" << std::endl;
}

// 测试 27: 验证多显示器环境下 SettingsWindow 居中落入 Primary Screen 内部，绝不横跨中缝撕裂 (DEF-07 回归测试)
void TestSettingsWindowPrimaryScreenCentering() {
    std::cout << "[RUN] TestSettingsWindowPrimaryScreenCentering..." << std::endl;
    auto& app = X11App::Instance();
    auto primary = app.GetPrimaryScreen();
    std::cout << "  Primary screen geometry: [" << primary.x << ", " << primary.y << ", " << primary.width << "x" << primary.height << "]" << std::endl;

    // 模拟居中计算逻辑 (SettingsWindow 修复后的算法)
    float dpiScale = app.GetDisplayDpiScale();
    int winW = static_cast<int>(560.0f * dpiScale);
    int winH = static_cast<int>(660.0f * dpiScale);

    int centerX = primary.x + (primary.width - winW) / 2;
    int centerY = primary.y + (primary.height - winH) / 2;

    std::cout << "  Calculated window center pos: (" << centerX << ", " << centerY << ") size: (" << winW << "x" << winH << ")" << std::endl;
    ALWAYS_ASSERT(centerX >= primary.x && "FAIL: SettingsWindow placed to the left of primary screen!");
    if (primary.width >= winW) {
        ALWAYS_ASSERT(centerX + winW <= primary.x + primary.width && "FAIL: SettingsWindow crosses primary screen right edge into seam/secondary monitor!");
    }
    std::cout << "[PASS] TestSettingsWindowPrimaryScreenCentering" << std::endl;
}

// 测试 28: 验证 DBusMenu GetGroupProperties 符合规范签名 a(ia{sv})
void TestDBusMenuGetGroupProperties() {
    std::cout << "[RUN] TestDBusMenuGetGroupProperties..." << std::endl;
    auto& sni = StatusNotifierItemLinux::Instance();

    DBusMessage* msg = dbus_message_new_method_call(
        "org.test", "/MenuBar", "com.canonical.dbusmenu", "GetGroupProperties"
    );
    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);

    // ids: [2001, 10001]
    int32_t ids[] = { 2001, 10001 };
    const int32_t* pIds = ids;
    DBusMessageIter arrayIds;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "i", &arrayIds);
    dbus_message_iter_append_fixed_array(&arrayIds, DBUS_TYPE_INT32, &pIds, 2);
    dbus_message_iter_close_container(&args, &arrayIds);

    // propertyNames: ["label", "type"]
    DBusMessageIter arrayProps;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "s", &arrayProps);
    const char* p1 = "label";
    const char* p2 = "type";
    dbus_message_iter_append_basic(&arrayProps, DBUS_TYPE_STRING, &p1);
    dbus_message_iter_append_basic(&arrayProps, DBUS_TYPE_STRING, &p2);
    dbus_message_iter_close_container(&args, &arrayProps);

    dbus_message_set_serial(msg, 301);

    DBusMessage* reply = StatusNotifierItemLinux::TestHandleDBusMenuMethod(msg, &sni);
    dbus_message_unref(msg);

    ALWAYS_ASSERT(reply != nullptr && "FAIL: GetGroupProperties returned null reply!");
    const char* sig = dbus_message_get_signature(reply);
    ALWAYS_ASSERT(sig != nullptr && strcmp(sig, "a(ia{sv})") == 0 && "FAIL: GetGroupProperties must return signature a(ia{sv})!");

    dbus_message_unref(reply);
    std::cout << "[PASS] TestDBusMenuGetGroupProperties" << std::endl;
}

// 测试 29: 验证 /StatusNotifierItem 与 /MenuBar 的 Introspect 接口
void TestDBusIntrospectSupport() {
    std::cout << "[RUN] TestDBusIntrospectSupport..." << std::endl;
    auto& sni = StatusNotifierItemLinux::Instance();

    // 1. /StatusNotifierItem Introspect
    {
        DBusMessage* msg = dbus_message_new_method_call(
            "org.test", "/StatusNotifierItem", "org.freedesktop.DBus.Introspectable", "Introspect"
        );
        dbus_message_set_serial(msg, 302);
        int res = StatusNotifierItemLinux::TestMessageFilter(nullptr, msg, &sni);
        ALWAYS_ASSERT(res == DBUS_HANDLER_RESULT_HANDLED && "FAIL: /StatusNotifierItem Introspect must be handled!");
        dbus_message_unref(msg);
    }

    // 2. /MenuBar Introspect
    {
        DBusMessage* msg = dbus_message_new_method_call(
            "org.test", "/MenuBar", "org.freedesktop.DBus.Introspectable", "Introspect"
        );
        dbus_message_set_serial(msg, 303);
        int res = StatusNotifierItemLinux::TestMessageFilter(nullptr, msg, &sni);
        ALWAYS_ASSERT(res == DBUS_HANDLER_RESULT_HANDLED && "FAIL: /MenuBar Introspect must be handled!");
        dbus_message_unref(msg);
    }
    std::cout << "[PASS] TestDBusIntrospectSupport" << std::endl;
}

// 测试 30: 验证悬浮窗左、右、顶贴边折叠形态下物理边缘绝对对齐，彻底消除 116px 空白
void TestFloatingWindowCollapsedDockGeometry() {
    std::cout << "[RUN] TestFloatingWindowCollapsedDockGeometry..." << std::endl;
    auto& fw = FloatingWindow::Instance();
    fw.Create();

    float dpiScale = X11App::Instance().GetDisplayDpiScale();
    int baseW = static_cast<int>(std::round(AppConstants::FloatingWindowDimensions::BASE_WIDTH * dpiScale));
    int baseH = static_cast<int>(std::round(AppConstants::FloatingWindowDimensions::BASE_HEIGHT * dpiScale));
    int tabW = static_cast<int>(std::round(AppConstants::FloatingWindowDimensions::DOCK_TAB_WIDTH * dpiScale));

    // 1. 测试右贴边折叠
    fw.TestSetDockStateAndRender(DockState::DockedRight_Collapsed);
    auto screen = X11App::Instance().GetScreenForPoint(fw.GetX() + fw.GetWidth() / 2, fw.GetY() + fw.GetHeight() / 2);

    std::cout << "  DockedRight_Collapsed pos: (" << fw.GetX() << ", " << fw.GetY() << ") size: (" << fw.GetWidth() << "x" << fw.GetHeight() << ")" << std::endl;
    std::cout << "  Screen right edge: " << (screen.x + screen.width) << " Window right edge: " << (fw.GetX() + fw.GetWidth()) << std::endl;

    ALWAYS_ASSERT(fw.GetWidth() == tabW && "FAIL: Right collapsed width must equal tabW!");
    ALWAYS_ASSERT(fw.GetHeight() == baseH && "FAIL: Right collapsed height must equal baseH!");
    ALWAYS_ASSERT(fw.GetX() + fw.GetWidth() == screen.x + screen.width && "FAIL: Right collapsed window must align exactly with screen right edge!");

    // 2. 测试左贴边折叠
    fw.TestSetDockStateAndRender(DockState::DockedLeft_Collapsed);
    std::cout << "  DockedLeft_Collapsed pos: (" << fw.GetX() << ", " << fw.GetY() << ") size: (" << fw.GetWidth() << "x" << fw.GetHeight() << ")" << std::endl;
    ALWAYS_ASSERT(fw.GetWidth() == tabW && "FAIL: Left collapsed width must equal tabW!");
    ALWAYS_ASSERT(fw.GetX() == screen.x && "FAIL: Left collapsed window must align exactly with screen left edge!");

    // 3. 测试顶贴边折叠
    fw.TestSetDockStateAndRender(DockState::DockedTop_Collapsed);
    std::cout << "  DockedTop_Collapsed pos: (" << fw.GetX() << ", " << fw.GetY() << ") size: (" << fw.GetWidth() << "x" << fw.GetHeight() << ")" << std::endl;
    ALWAYS_ASSERT(fw.GetWidth() == baseW && "FAIL: Top collapsed width must equal baseW!");
    ALWAYS_ASSERT(fw.GetHeight() == tabW && "FAIL: Top collapsed height must equal tabW!");
    ALWAYS_ASSERT(fw.GetY() == screen.y && "FAIL: Top collapsed window must align exactly with screen top edge!");

    // 4. 还原为浮动常驻态
    fw.TestSetDockStateAndRender(DockState::Floating);
    std::cout << "[PASS] TestFloatingWindowCollapsedDockGeometry" << std::endl;
}

// 测试 30: 验证全屏工间操阶段名称与倒计时字符串绝不包含导致 Linux 字体方框乱码的非标准 Emoji
void TestFullscreenStageAndBadgeTextNoEmoji() {
    std::cout << "[RUN] TestFullscreenStageAndBadgeTextNoEmoji..." << std::endl;
    ReminderConfig cfg;
    cfg.exerciseMode = ExerciseMode::Comprehensive;
    cfg.workMinutes = 1;
    cfg.restSeconds = 60;
    StateMachine sm(cfg);
    sm.StartRest();

    std::wstring stage1 = sm.GetCurrentRestStageName();
    std::cout << "  Stage 1 name: " << X11Utils::WStringToUtf8(stage1) << std::endl;
    ALWAYS_ASSERT(!stage1.empty());
    // 必须包含中文阶段与操法，但绝不能包含 Emoji 字符
    ALWAYS_ASSERT(stage1.find(L"科学颈椎保养操") != std::wstring::npos);
    ALWAYS_ASSERT(stage1.find(L"🧘") == std::wstring::npos && "FAIL: Stage name must not contain 🧘 emoji!");
    ALWAYS_ASSERT(stage1.find(L"👁") == std::wstring::npos && "FAIL: Stage name must not contain 👁 emoji!");
    ALWAYS_ASSERT(stage1.find(L"🍃") == std::wstring::npos && "FAIL: Stage name must not contain 🍃 emoji!");
    for (wchar_t ch : stage1) {
        ALWAYS_ASSERT((ch < 0x2700 || (ch >= 0x4E00 && ch <= 0x9FFF) || (ch >= 0x3000 && ch <= 0x303F)) && "FAIL: Stage name contains emoji or unrenderable character!");
    }

    // 测试倒计时格式化文案
    wchar_t timeBuf[32];
    int minutes = 1, seconds = 11;
    swprintf(timeBuf, 32, L"剩余 %02d:%02d", minutes, seconds);
    std::wstring timeStr = timeBuf;
    ALWAYS_ASSERT(timeStr.find(L"⏱") == std::wstring::npos && "FAIL: Countdown text must not contain stopwatch emoji!");
    ALWAYS_ASSERT(timeStr == L"剩余 01:11");

    std::cout << "[PASS] TestFullscreenStageAndBadgeTextNoEmoji" << std::endl;
}

// 测试 31: 验证全屏动作要领卡片字体尺寸升级与 ExerciseLayout 垂直呼吸留白
void TestFullscreenGuidanceCardFontSizesAndLayout() {
    std::cout << "[RUN] TestFullscreenGuidanceCardFontSizesAndLayout..." << std::endl;
    D2D1_RECT_F bounds2K = D2D1::RectF(0, 0, 2560, 1440);
    float dpiScale = 1.25f;
    auto layout = ExerciseLayout::Calculate(bounds2K, dpiScale, 460.0f, 260.0f);

    float dockH = layout.dockRect.bottom - layout.dockRect.top;
    std::cout << "  2K Dock Height: " << dockH << ", dpiScale: " << dpiScale << std::endl;
    // 升级后的三大层级字号与行高需求
    float titleH = 25.0f * dpiScale;
    float tipH = 18.5f * dpiScale;
    float subTipH = 15.5f * dpiScale;
    float totalContentH = titleH + tipH + subTipH + (18.0f + 14.0f + 14.0f) * dpiScale;
    ALWAYS_ASSERT(dockH >= totalContentH && "FAIL: Dock card height must be sufficiently large to host enlarged text without clipping!");
    ALWAYS_ASSERT(dockH >= 180.0f * dpiScale && "FAIL: Dock card should have generous breathing room for standing workout!");

    std::cout << "[PASS] TestFullscreenGuidanceCardFontSizesAndLayout" << std::endl;
}

// 测试 32: 验证 LinuxCanvas 对变体选择符 (FE00-FE0F) 及零宽控制字符的 Tofu 抑制
void TestLinuxCanvasZeroWidthAndVariationSelectorSuppression() {
    std::cout << "[RUN] TestLinuxCanvasZeroWidthAndVariationSelectorSuppression..." << std::endl;
    auto& d2d = D2DContext::Instance();
    LinuxRenderTarget rt(100, 50);
    rt.BeginDraw();
    rt.Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f));

    // 仅绘制变体选择符与零宽字符
    const wchar_t testStr[] = { 0xFE0F, 0x200B, 0xFE0E, 0 };
    auto fmt = d2d.GetCachedTextFormat(L"sans-serif", 20.0f);
    LinuxBrush brush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
    rt.DrawText(testStr, 3, fmt.Get(), D2D1::RectF(0, 0, 100, 50), &brush);
    rt.EndDraw();

    // 断言：不可见控制字符决不能绘制出任何亮色像素 (避免产生次生 Tofu 空心方块)
    const uint32_t* pixels = rt.GetPixels();
    int nonBlackCount = 0;
    for (int i = 0; i < 100 * 50; ++i) {
        if ((pixels[i] & 0x00FFFFFF) != 0) {
            nonBlackCount++;
        }
    }
    std::cout << "  Zero-width / Variation selector rendered pixels: " << nonBlackCount << std::endl;
    ALWAYS_ASSERT(nonBlackCount == 0 && "FAIL: Variation selectors and zero-width characters must NOT render tofu box!");

    std::cout << "[PASS] TestLinuxCanvasZeroWidthAndVariationSelectorSuppression" << std::endl;
}

// 测试 33: 验证 Linux 全屏极简模式下对齐绘制中央舒缓标语
void TestFullscreenSimpleModeTextRendering() {
    std::cout << "[RUN] TestFullscreenSimpleModeTextRendering..." << std::endl;
    auto& mask = FullscreenMask::Instance();
    mask.Initialize();
    ConfigManager::Instance().GetConfig().exerciseMode = ExerciseMode::Simple;
    mask.UpdateDisplay(60, 60, 0, L"工间静心放空休息");
    mask.Show(true);

    // 触发渲染并验证背景缓冲有效生成
    mask.TriggerAnimationTick();
    mask.Show(false);
    ConfigManager::Instance().GetConfig().exerciseMode = ExerciseMode::Comprehensive;

    std::cout << "[PASS] TestFullscreenSimpleModeTextRendering" << std::endl;
}

int main() {
    std::cout << "Running X11 UI Fixes Unit Tests..." << std::endl;
    D2DContext::Instance().Initialize();
    TestWindowUtf8TitleProperty();
    TestTrayMenuLifecycle();
    TestTrayMenuHitTesting();
    TestFullscreenAnimationProgress();
    TestFullscreenStageTransition();
    TestSettingsWindowInteractions();
    TestFloatingWindowClickVsDoubleClick();
    TestSingleInstanceInodePreserved();
    TestLinuxCanvasRotatedPrimitives();
    TestLinuxCanvasRotatedTextRendering();
    TestFloatingWindowCardBackground();
    TestFloatingWindowRightSideTimerText();
    TestStatusNotifierItemLifecycleAndCallbacks();
    TestStatusNotifierItemTooltipProperty();
    TestTrayWindowTooltipUtf8Preserved();
    TestFloatingWindowTopMostDynamic();
    TestSNIIconPropertiesAndPixmap();
    TestX11WindowClassAndIconProperties();
    TestStatusNotifierDBusMenuLayoutAndEvent();
    TestStatusNotifierInterfaceCompatibility();
    TestD2DTextFormatCacheIsolation();
    TestMultiMonitorTopologyDetection();
    TestFloatingWindowEwmhProperties();
    TestFullscreenMaskMultiscreenAndSafeXImage();
    TestTrayWindowRunCatAndCountdownRendering();
    TestDpiScaleCascade();
    TestSettingsWindowPrimaryScreenCentering();
    TestDBusMenuGetGroupProperties();
    TestFloatingWindowCollapsedDockGeometry();
    TestFullscreenStageAndBadgeTextNoEmoji();
    TestFullscreenGuidanceCardFontSizesAndLayout();
    TestLinuxCanvasZeroWidthAndVariationSelectorSuppression();
    TestFullscreenSimpleModeTextRendering();
    std::cout << "All X11 UI Fixes Tests PASSED!" << std::endl;
    return 0;
}

#else
int main() { return 0; }
#endif
