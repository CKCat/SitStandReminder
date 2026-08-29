cmake_minimum_required(VERSION 3.20)
project(SitStandReminder VERSION 1.0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Unicode 支持
add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
    add_compile_options(
        /W4
        /utf-8
        /permissive-
        /EHsc
    )
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Oi /Gy /GL")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")
endif()

include_directories(src resources ${CMAKE_CURRENT_BINARY_DIR}/generated)

# 自动生成版本控制文件 (Single Source of Truth from project VERSION)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/Version.hpp
    @ONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/app.rc
    @ONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/app.manifest.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/app.manifest
    @ONLY
)

# 核心源码
set(CORE_SOURCES
    src/core/ConfigManager.cpp
    src/core/StateMachine.cpp
)

set(PLATFORM_SOURCES
    src/platform/ThemeManager.cpp
    src/platform/KeyboardHook.cpp
)

set(GRAPHICS_SOURCES
    src/graphics/D2DContext.cpp
    src/graphics/NeckExerciseRenderer.cpp
    src/graphics/EyeExerciseRenderer.cpp
    src/graphics/MascotRenderer.cpp
    src/graphics/DynamicTrayIcon.cpp
)

set(UI_SOURCES
    src/ui/TrayWindow.cpp
    src/ui/FloatingWindow.cpp
    src/ui/FullscreenMask.cpp
    src/ui/SettingsWindow.cpp
)

# 单元测试子目录
enable_testing()
add_subdirectory(tests)

# 主可执行程序
add_executable(SitStandReminder WIN32
    src/main.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/app.rc
    ${CORE_SOURCES}
    ${PLATFORM_SOURCES}
    ${GRAPHICS_SOURCES}
    ${UI_SOURCES}
)

target_link_libraries(SitStandReminder PRIVATE
    d2d1
    dwrite
    dxgi
    d3d11
    dwmapi
    uxtheme
    winmm
    comctl32
    shlwapi
    shcore
    wtsapi32
)

# 启用现代 Common Controls 视觉样式与 Per-Monitor DPI 清单
if(MSVC)
    target_link_options(SitStandReminder PRIVATE
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${CMAKE_CURRENT_BINARY_DIR}/generated/app.manifest"
    )
endif()

