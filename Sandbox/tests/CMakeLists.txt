# SandboxSmokeTests: boots the real engine (window + Vulkan) on Sandbox/assets, runs a few frames, shuts down
include(${CMAKE_SOURCE_DIR}/cmake/WkTesting.cmake)

file(GLOB_RECURSE SANDBOX_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

set(SANDBOX_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
cmake_path(NORMAL_PATH SANDBOX_SOURCE_DIR)

wk_add_test_executable(SandboxSmokeTests
    LABEL smoke
    WORKING_DIRECTORY ${SANDBOX_SOURCE_DIR}
    TIMEOUT 120
    SOURCES ${SANDBOX_TEST_SOURCES}
)
target_compile_definitions(SandboxSmokeTests PRIVATE WK_SANDBOX_DIR="${SANDBOX_SOURCE_DIR}")
add_dependencies(SandboxSmokeTests Sandbox_Shaders)
