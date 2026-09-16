cmake_minimum_required(VERSION 3.20)
project(SitStandReminder VERSION 1.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    # Unicode 与 Windows 精简定义
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
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FREETYPE REQUIRED freetype2)
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
    pkg_check_modules(DBUS REQUIRED dbus-1)
    find_package(X11 REQUIRED)
endif()

include_directories(
    src
    resources
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

if(NOT WIN32)
    include_directories(
        ${FREETYPE_INCLUDE_DIRS}
        ${FONTCONFIG_INCLUDE_DIRS}
        ${DBUS_INCLUDE_DIRS}
        ${X11_INCLUDE_DIR}
    )
endif()

# 自动生成版本控制文件 (Single Source of Truth from project VERSION)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/Version.hpp
    @ONLY
)

if(WIN32)
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
endif()

# 核心源码 (平台无关)
set(CORE_SOURCES
    src/core/ConfigManager.cpp
    src/core/StateMachine.cpp
)

set(PLATFORM_SOURCES
    src/platform/ThemeManager.cpp
    src/platform/KeyboardHook.cpp
    src/platform/SingleInstance.cpp
)

if(WIN32)
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

    # Windows 主可执行程序
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

    if(MSVC)
        target_link_options(SitStandReminder PRIVATE
            "/MANIFEST:EMBED"
            "/MANIFESTINPUT:${CMAKE_CURRENT_BINARY_DIR}/generated/app.manifest"
        )
    endif()

else() # Linux / UNIX
    set(GRAPHICS_SOURCES
        src/graphics/NeckExerciseRenderer.cpp
        src/graphics/EyeExerciseRenderer.cpp
        src/graphics/MascotRenderer.cpp
        src/graphics/linux/LinuxCanvas.cpp
        src/graphics/linux/D2DContextLinux.cpp
    )

    set(UI_SOURCES
        src/ui/linux/X11App.cpp
        src/ui/linux/FloatingWindowLinux.cpp
        src/ui/linux/FullscreenMaskLinux.cpp
        src/ui/linux/TrayWindowLinux.cpp
        src/ui/linux/SettingsWindowLinux.cpp
        src/ui/linux/StatusNotifierItemLinux.cpp
    )

    # Linux 主可执行程序
    add_executable(SitStandReminder
        src/main.cpp
        ${CORE_SOURCES}
        ${PLATFORM_SOURCES}
        ${GRAPHICS_SOURCES}
        ${UI_SOURCES}
    )

    target_link_libraries(SitStandReminder PRIVATE
        ${X11_LIBRARIES}
        ${FREETYPE_LIBRARIES}
        ${FONTCONFIG_LIBRARIES}
        ${DBUS_LIBRARIES}
        pthread
        dl
    )

    # Linux 安装规则
    include(GNUInstallDirs)
    install(TARGETS SitStandReminder
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(FILES resources/SitStandReminder.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    install(FILES resources/sitstandreminder.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
    )
endif()

# 单元测试子目录
enable_testing()
add_subdirectory(tests)

# ---------------- CPack 打包配置 ----------------
set(CPACK_PACKAGE_NAME "SitStandReminder")
set(CPACK_PACKAGE_VENDOR "CKCat")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "科学颈部与视力舒缓工间操健康伴侣")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

if(NOT WIN32)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "CKCat <noreply@github.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME "SitStandReminder-Linux-x64.deb")
endif()

include(CPack)

