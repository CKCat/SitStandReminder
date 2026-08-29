cmake_minimum_required(VERSION 3.20)

add_executable(UnitTests
    test_statemachine.cpp
    ../src/core/StateMachine.cpp
    ../src/core/ConfigManager.cpp
)

target_include_directories(UnitTests PRIVATE ../src)

target_link_libraries(UnitTests PRIVATE
    shlwapi
)

add_test(NAME StateMachineTests COMMAND UnitTests)

