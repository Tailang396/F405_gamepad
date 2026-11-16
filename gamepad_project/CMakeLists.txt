cmake_minimum_required(VERSION 4.4)
project(gamepad LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(direwolf2 direwolf2.c)
target_include_directories(direwolf2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(parser_demo EXCLUDE_FROM_ALL demo.c demo_delay.cpp)
target_link_libraries(parser_demo PRIVATE direwolf2)

add_executable(report_stdin EXCLUDE_FROM_ALL portable_monitor.c)
target_link_libraries(report_stdin PRIVATE direwolf2)

if(WIN32)
    add_executable(gamepad pc_monitor.cpp)
    target_link_libraries(gamepad PRIVATE dinput8 dxguid)
    add_executable(usb_enum EXCLUDE_FROM_ALL usb_enum.cpp)
    target_link_libraries(usb_enum PRIVATE setupapi)
    add_executable(pc_monitor EXCLUDE_FROM_ALL pc_monitor.cpp)
    target_link_libraries(pc_monitor PRIVATE dinput8 dxguid)
endif()

enable_testing()
add_executable(parser_test tests/parser_test.c)
target_link_libraries(parser_test PRIVATE direwolf2)
add_test(NAME parser_test COMMAND parser_test)

# The previous Windows-only program is preserved, but disabled by default.
option(BUILD_XINPUT_MONITOR "Build the original Windows XInput monitor" OFF)
if(BUILD_XINPUT_MONITOR)
    if(NOT WIN32)
        message(FATAL_ERROR "The XInput monitor requires Windows")
    endif()
    add_executable(xinput_monitor main.cpp)
    set_property(TARGET xinput_monitor PROPERTY CXX_STANDARD 20)
    target_link_libraries(xinput_monitor PRIVATE xinput9_1_0)
endif()
