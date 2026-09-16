#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <cassert>
#include "ui/TrayWindow.hpp"
#include "ui/FullscreenMask.hpp"
#include "ui/linux/X11App.hpp"
#include "graphics/D2DContext.hpp"
#include "core/ConfigManager.hpp"
#include "platform/ThemeManager.hpp"

StateMachine* g_pStateMachine = nullptr;

// 统计两个 BMP 文件中不同的像素数量
size_t DiffBmpPixels(const std::string& pathA, const std::string& pathB) {
    std::ifstream fa(pathA, std::ios::binary);
    std::ifstream fb(pathB, std::ios::binary);
    if (!fa.is_open() || !fb.is_open()) return 0;

    fa.seekg(0, std::ios::end);
    size_t sizeA = fa.tellg();
    fa.seekg(54, std::ios::beg);

    fb.seekg(0, std::ios::end);
    size_t sizeB = fb.tellg();
    fb.seekg(54, std::ios::beg);

    if (sizeA != sizeB || sizeA <= 54) return 0;

    std::vector<char> bufA(sizeA - 54);
    std::vector<char> bufB(sizeB - 54);
    fa.read(bufA.data(), bufA.size());
    fb.read(bufB.data(), bufB.size());

    size_t diffPixels = 0;
    size_t totalPixels = bufA.size() / 4;
    for (size_t i = 0; i < totalPixels; ++i) {
        if (bufA[i * 4] != bufB[i * 4] ||
            bufA[i * 4 + 1] != bufB[i * 4 + 1] ||
            bufA[i * 4 + 2] != bufB[i * 4 + 2]) {
            diffPixels++;
        }
    }
    return diffPixels;
}

int main() {
    std::cout << "[E2E] Initializing X11App & D2DContext..." << std::endl;
    auto& app = X11App::Instance();
    if (!app.Initialize()) {
        std::cout << "Cannot initialize X11App" << std::endl;
        return 1;
    }
    D2DContext::Instance().Initialize();
    ConfigManager::Instance().Load();
    ThemeManager::Instance().Refresh();

    std::cout << "[E2E] 1. Testing Tray Menu Hit-testing & Geometry..." << std::endl;
    auto& tray = TrayWindow::Instance();
    tray.Create();

    const auto& items = tray.GetMenuItems();
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].isSeparator) continue;
        auto rect = tray.GetMenuItemRect(i);
        int midY = static_cast<int>((rect.top + rect.bottom) / 2.0f);
        int hit = tray.GetMenuItemAt(midY);
        std::cout << "  Item " << i << " hit at midY=" << midY << ": " << hit << std::endl;
        assert(hit == static_cast<int>(i));
    }
    tray.Destroy();
    std::cout << "[E2E] Tray menu hit-testing: SUCCESS!" << std::endl;

    std::cout << "[E2E] 2. Testing Fullscreen Exercise Animation Progression..." << std::endl;
    setenv("DUMP_FULLSCREEN_PREFIX", "/tmp/e2e_fullscreen", 1);

    auto& mask = FullscreenMask::Instance();
    mask.Initialize();
    mask.UpdateDisplay(71, 120, 0, L"阶段 1/2 · 科学颈椎保养操");
    mask.Show(true);

    // 模拟前 3 帧渲染 (间隔 ~60ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    mask.TriggerAnimationTick();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    mask.TriggerAnimationTick();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    mask.TriggerAnimationTick();

    mask.Show(false);

    size_t diff01 = DiffBmpPixels("/tmp/e2e_fullscreen_frame0.bmp", "/tmp/e2e_fullscreen_frame1.bmp");
    size_t diff12 = DiffBmpPixels("/tmp/e2e_fullscreen_frame1.bmp", "/tmp/e2e_fullscreen_frame2.bmp");

    std::cout << "  Frame 0 vs Frame 1 diff pixels: " << diff01 << std::endl;
    std::cout << "  Frame 1 vs Frame 2 diff pixels: " << diff12 << std::endl;

    // 核心断言：连续帧之间必须存在显著的像素运动与变化，绝不可为 0！
    assert(diff01 > 100);
    assert(diff12 > 100);

    std::cout << "[E2E] Fullscreen animation dynamic progression: SUCCESS!" << std::endl;
    return 0;
}
