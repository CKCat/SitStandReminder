cmake_minimum_required(VERSION 3.20)

add_executable(UnitTests
    test_statemachine.cpp
    ../src/core/StateMachine.cpp
    ../src/core/ConfigManager.cpp
)

target_include_directories(UnitTests PRIVATE ../src ${CMAKE_BINARY_DIR}/generated)

if(WIN32)
    target_link_libraries(UnitTests PRIVATE
        shlwapi
        d2d1
    )
else()
    target_link_libraries(UnitTests PRIVATE
        pthread
    )

    add_executable(GraphicsTests
        test_graphics_pipeline.cpp
        ../src/graphics/linux/LinuxCanvas.cpp
    )

    target_include_directories(GraphicsTests PRIVATE
        ../src
        ${FREETYPE_INCLUDE_DIRS}
        ${FONTCONFIG_INCLUDE_DIRS}
    )

    target_link_libraries(GraphicsTests PRIVATE
        ${FREETYPE_LIBRARIES}
        ${FONTCONFIG_LIBRARIES}
        pthread
    )

    add_test(NAME GraphicsPipelineTests COMMAND GraphicsTests)

    add_executable(X11UIFixesTests
        test_x11_ui_fixes.cpp
        ../src/ui/linux/TrayWindowLinux.cpp
        ../src/ui/linux/FloatingWindowLinux.cpp
        ../src/ui/linux/SettingsWindowLinux.cpp
        ../src/ui/linux/X11App.cpp
        ../src/platform/ThemeManager.cpp
        ../src/core/ConfigManager.cpp
        ../src/core/StateMachine.cpp
        ../src/graphics/MascotRenderer.cpp
        ../src/graphics/NeckExerciseRenderer.cpp
        ../src/graphics/EyeExerciseRenderer.cpp
        ../src/ui/linux/FullscreenMaskLinux.cpp
        ../src/graphics/linux/D2DContextLinux.cpp
        ../src/graphics/linux/LinuxCanvas.cpp
        ../src/platform/SingleInstance.cpp
        ../src/ui/linux/StatusNotifierItemLinux.cpp
    )

    target_include_directories(X11UIFixesTests PRIVATE
        ../src
        ${CMAKE_BINARY_DIR}/generated
        ${FREETYPE_INCLUDE_DIRS}
        ${FONTCONFIG_INCLUDE_DIRS}
        ${DBUS_INCLUDE_DIRS}
        ${X11_INCLUDE_DIR}
    )

    target_link_libraries(X11UIFixesTests PRIVATE
        ${X11_LIBRARIES}
        ${FREETYPE_LIBRARIES}
        ${FONTCONFIG_LIBRARIES}
        ${DBUS_LIBRARIES}
        pthread
        dl
    )

    add_test(NAME X11UIFixesTests COMMAND X11UIFixesTests)

    add_executable(E2EVerify
        test_e2e_verify.cpp
        ../src/ui/linux/TrayWindowLinux.cpp
        ../src/ui/linux/FloatingWindowLinux.cpp
        ../src/ui/linux/SettingsWindowLinux.cpp
        ../src/ui/linux/FullscreenMaskLinux.cpp
        ../src/ui/linux/X11App.cpp
        ../src/platform/ThemeManager.cpp
        ../src/core/ConfigManager.cpp
        ../src/core/StateMachine.cpp
        ../src/graphics/MascotRenderer.cpp
        ../src/graphics/NeckExerciseRenderer.cpp
        ../src/graphics/EyeExerciseRenderer.cpp
        ../src/graphics/linux/D2DContextLinux.cpp
        ../src/graphics/linux/LinuxCanvas.cpp
        ../src/platform/SingleInstance.cpp
        ../src/ui/linux/StatusNotifierItemLinux.cpp
    )

    target_include_directories(E2EVerify PRIVATE
        ../src
        ${CMAKE_BINARY_DIR}/generated
        ${FREETYPE_INCLUDE_DIRS}
        ${FONTCONFIG_INCLUDE_DIRS}
        ${DBUS_INCLUDE_DIRS}
        ${X11_INCLUDE_DIR}
    )

    target_link_libraries(E2EVerify PRIVATE
        ${X11_LIBRARIES}
        ${FREETYPE_LIBRARIES}
        ${FONTCONFIG_LIBRARIES}
        ${DBUS_LIBRARIES}
        pthread
        dl
    )

    add_test(NAME E2EVerifyTests COMMAND E2EVerify)
    set_tests_properties(X11UIFixesTests E2EVerifyTests PROPERTIES RUN_SERIAL TRUE)


    add_executable(ExerciseBench
        test_exercise_bench.cpp
        ../src/graphics/NeckExerciseRenderer.cpp
        ../src/graphics/EyeExerciseRenderer.cpp
        ../src/graphics/linux/D2DContextLinux.cpp
        ../src/graphics/linux/LinuxCanvas.cpp
    )

    target_include_directories(ExerciseBench PRIVATE
        ../src
        ${CMAKE_BINARY_DIR}/generated
        ${FREETYPE_INCLUDE_DIRS}
        ${FONTCONFIG_INCLUDE_DIRS}
    )

    target_link_libraries(ExerciseBench PRIVATE
        ${FREETYPE_LIBRARIES}
        ${FONTCONFIG_LIBRARIES}
        pthread
    )
endif()


add_test(NAME StateMachineTests COMMAND UnitTests)

