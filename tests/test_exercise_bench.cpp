#include <iostream>
#include <chrono>
#include "graphics/NeckExerciseRenderer.hpp"
#include "graphics/EyeExerciseRenderer.hpp"
#include "graphics/linux/LinuxCanvas.hpp"
#include "graphics/D2DContext.hpp"

int main() {
    D2DContext::Instance().Initialize();
    int w = 2560;
    int h = 1440;
    std::cout << "Creating LinuxRenderTarget " << w << "x" << h << "..." << std::endl;
    LinuxRenderTarget rt(w, h);
    D2D1_RECT_F bounds = D2D1::RectF(0, 0, static_cast<float>(w), static_cast<float>(h));

    NeckExerciseRenderer neck;
    neck.SetTotalDuration(60.0f);

    EyeExerciseRenderer eye;
    eye.SetTotalDuration(60.0f);

    std::cout << "Benchmarking NeckExerciseRenderer::Render..." << std::endl;
    for (int i = 0; i < 5; ++i) {
        std::cout << "  Updating neck..." << std::endl;
        neck.Update(0.033f);
        std::cout << "  Neck updated." << std::endl;
        auto t0 = std::chrono::steady_clock::now();
        std::cout << "  Clearing..." << std::endl;
        rt.Clear(D2D1::ColorF(0.05f, 0.06f, 0.08f, 0.98f));
        std::cout << "  Rendering neck..." << std::endl;
        neck.Render(&rt, bounds, 1.0f);
        std::cout << "  Neck rendered." << std::endl;
        auto t1 = std::chrono::steady_clock::now();
        std::cout << "Neck Frame " << i << " took " 
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() 
                  << " ms" << std::endl;
    }

    std::cout << "Benchmarking EyeExerciseRenderer::Render..." << std::endl;
    for (int i = 0; i < 5; ++i) {
        eye.Update(0.033f);
        auto t0 = std::chrono::steady_clock::now();
        rt.Clear(D2D1::ColorF(0.05f, 0.06f, 0.08f, 0.98f));
        eye.Render(&rt, bounds, 1.0f);
        auto t1 = std::chrono::steady_clock::now();
        std::cout << "Eye Frame " << i << " took " 
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() 
                  << " ms" << std::endl;
    }
    return 0;
}
