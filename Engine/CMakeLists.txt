cmake_minimum_required(VERSION 3.5...4.0)

project(MyGraphicsEngine LANGUAGES CXX)

# Detect architecture
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH "x64")
else()
    set(ARCH "Win32")
endif()

if(MSVC)
    add_compile_options(/bigobj)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Engine sources
file(GLOB_RECURSE ENGINE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
)

set(IMGUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui/backends/imgui_impl_opengl3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui/backends/imgui_impl_vulkan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui/misc/cpp/imgui_stdlib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/ImGuizmo/src/ImGuizmo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/ImGuizmo/src/GraphEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/ImGuizmo/src/ImCurveEdit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/ImGuizmo/src/ImGradient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/ImGuizmo/src/ImSequencer.cpp
)

add_library(${PROJECT_NAME} ${WK_LIB_TYPE}
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
    ${IMGUI_SOURCES}
)

include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME}
    EXPORT_FILE_NAME include/Engine/EngineCoreAPI.h
)

set(PCH_HEADERS
    <string>
    <sstream>
    <iostream>
    <algorithm>
    <memory>
    <unordered_map>
    <unordered_set>
    <set>
    <future>
    <mutex>
    <shared_mutex>
    <thread>
    <atomic>
    <cstring>
    <fstream>
    <stdexcept>
    <vector>
    <array>
    <span>
    <cstdint>
    <limits>
    <optional>
    <variant>
    <any>
    <functional>
    <random>
    <cstdlib>
    <cassert>
    <typeindex>
    <typeinfo>
    <glm/glm.hpp>
    <imgui.h>
    <vulkan/vulkan.h>
    <nlohmann/json.hpp>
)

if(WIN32)
    # Use definitions to keep windows.h from breaking things
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    list(APPEND PCH_HEADERS <windows.h>)
endif()

target_precompile_headers(${PROJECT_NAME} PRIVATE ${PCH_HEADERS})

# Output folders
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/${ARCH}/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/${ARCH}/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/${ARCH}/$<CONFIG>"
)
add_definitions(-DNOMINMAX)

# Dependencies
add_subdirectory(extern/single/glad)
add_subdirectory(extern/spdlog)
add_subdirectory(extern/glfw)
add_subdirectory(extern/nlohmann)
add_subdirectory(extern/assimp)
add_subdirectory(extern/lua)
add_subdirectory(extern/box3d)

find_package(Vulkan REQUIRED)


# Include directories to propagate to Sandbox
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    SYSTEM
    ${CMAKE_CURRENT_SOURCE_DIR}/extern
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/single
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/single/entt
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/single/FileWatch
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/single/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/single/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/single/sol
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui/misc/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/ImGuizmo/src
    ${CMAKE_CURRENT_BINARY_DIR}/extern/lua/include
    $ENV{VULKAN_SDK}/Include

    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)


# Link dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC
    assimp
    Vulkan::Vulkan
    glad
    spdlog
    nlohmann_json
    glfw
    opengl32
    lua::lib
    box3d::box3d
)

target_compile_definitions(${PROJECT_NAME} PUBLIC 
    IMGUI_IMPL_OPENGL_LOADER_GLAD
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj /utf-8 /MP)
endif()