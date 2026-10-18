# Builds the engine-independent simulation core (Source/DustToGlory/Sim) and its unit tests.
# Usage:  cmake -S Tests -B Tests/build && cmake --build Tests/build && ctest --test-dir Tests/build
cmake_minimum_required(VERSION 3.16)
project(DustToGloryCoreTests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SIM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../Source/DustToGlory/Sim/*.cpp)
file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(dtg_tests ${SIM_SOURCES} ${TEST_SOURCES})
target_include_directories(dtg_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Source/DustToGlory ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(dtg_tests PRIVATE DTG_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../Content/Data")

# Mirror Unreal's constraints: no exceptions, no RTTI, warnings as errors.
if(MSVC)
  target_compile_options(dtg_tests PRIVATE /W4 /WX /GR- /EHs-c- /D_HAS_EXCEPTIONS=0 /wd4100)
else()
  target_compile_options(dtg_tests PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror -fno-exceptions -fno-rtti)
endif()

enable_testing()
add_test(NAME core COMMAND dtg_tests)
