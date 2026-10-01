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
add_subdirectory(extern/vma)
add_subdirectory(extern/volk)
add_subdirectory(extern/vk-bootstrap)

find_package(Vulkan REQUIRED)


add_compile_definitions(${PROJECT_NAME} PUBLIC 
    VK_NO_PROTOTYPES
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    IMGUI_DEFINE_MATH_OPERATORS
    IMGUI_IMPL_OPENGL_LOADER_GLAD
)

target_precompile_headers(MyGraphicsEngine PRIVATE src/core/features/pch.h)

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
    ${CMAKE_CURRENT_BINARY_DIR}/extern/volk
    $ENV{VULKAN_SDK}/Include

    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)


# Link dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC
    assimp
    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    volk::volk
    GPUOpen::VulkanMemoryAllocator
    glad
    spdlog
    nlohmann_json
    glfw
    opengl32
    lua::lib
    box3d::box3d
)


if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj /utf-8 /MP)
endif()