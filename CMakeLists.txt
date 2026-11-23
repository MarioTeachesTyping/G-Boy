cmake_minimum_required(VERSION 3.20)
project(gboy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(GBOY_BUILD_TESTS "Build unit tests" ON)

# --- Core emulator library (platform-free) ---
add_library(gboy_core STATIC
    src/core/gba.cpp
    src/core/cartridge/cartridge.cpp
    src/core/memory/bus.cpp
    src/core/cpu/arm7tdmi.cpp
    src/core/cpu/arm_isa.cpp
    src/core/ppu/ppu.cpp
)
target_include_directories(gboy_core PUBLIC src)

if(MSVC)
    target_compile_options(gboy_core PUBLIC /W4 /permissive-)
else()
    target_compile_options(gboy_core PUBLIC -Wall -Wextra -Wpedantic)
endif()

# --- CLI frontend ---
add_executable(gboy src/frontend/cli/main.cpp)
target_link_libraries(gboy PRIVATE gboy_core)

# --- Tests ---
if(GBOY_BUILD_TESTS)
    enable_testing()
    add_executable(gboy_tests
        tests/main.cpp
        tests/test_cpu_arm.cpp
        tests/test_cpu_arm_exceptions.cpp
        tests/test_cpu_arm_multiply.cpp
        tests/test_cpu_arm_transfer.cpp
        tests/test_system.cpp
    )
    target_link_libraries(gboy_tests PRIVATE gboy_core)
    add_test(NAME gboy_tests COMMAND gboy_tests)
endif()
