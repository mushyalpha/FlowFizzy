cmake_minimum_required(VERSION 3.16)
project(AquaFlow VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Include paths ─────────────────────────────────────────────────────────────
include_directories(include)   # PinConfig.h, IHardwareDevice.h
include_directories(src)       # hardware/, state/, monitor/, utils/ sub-paths

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(filling_machine
    src/main.cpp
    src/hardware/UltrasonicSensor.cpp
    src/hardware/PumpController.cpp
    src/hardware/FlowMeter.cpp
    src/state/FillingController.cpp
    src/monitor/Monitor.cpp
    src/utils/Timer.cpp
    src/utils/Logger.cpp
)

# All three hardware drivers now use libgpiod (officially supported on Raspberry Pi OS).
# lgpio has been removed — it was a hobbyist library not officially supported.
target_link_libraries(filling_machine
    gpiod
    pthread
)

# ── Google Test (fetched automatically) ──────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# ── Unit test executable (GTest, no GPIO needed) ─────────────────────────────
add_executable(filling_tests
    tests/FlowMeterTest.cpp
    tests/FillingControllerTest.cpp
    tests/PumpControllerTest.cpp
    # Source files compiled again for the test binary (no lgpio/gpiod calls needed
    # because tests use mocks/stubs in tests/mock_include/)
    src/state/FillingController.cpp
    src/monitor/Monitor.cpp
    src/utils/Timer.cpp
    src/utils/Logger.cpp
)

target_include_directories(filling_tests PRIVATE
    include
    src
    tests
    tests/mock_include   # mock gpiod.hpp and lgpio.h for off-target testing
)

target_link_libraries(filling_tests GTest::gtest_main pthread)

include(GoogleTest)
gtest_discover_tests(filling_tests)

# ── Standalone integration test (real hardware, uses main()) ─────────────────
# These are NOT GTest — they are manual integration programs kept as separate targets
add_executable(ultrasonic_integration_test
    tests/UltrasonicSensorTest.cpp
    src/hardware/UltrasonicSensor.cpp
)
target_include_directories(ultrasonic_integration_test PRIVATE include src)
target_link_libraries(ultrasonic_integration_test gpiod pthread)

add_executable(flow_sensor_integration_test
    tests/FlowSensorTest.cpp
    src/hardware/FlowMeter.cpp
)
target_include_directories(flow_sensor_integration_test PRIVATE include src)
target_link_libraries(flow_sensor_integration_test gpiod pthread)
