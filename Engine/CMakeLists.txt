cmake_minimum_required(VERSION 3.5...4.0)

project(MyGraphicsEngine LANGUAGES CXX)

# Detect architecture
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH "x64")
else()
    set(ARCH "Win32")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Engine sources
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
)

# ImGui + ImGuizmo sources
set(IMGUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/imgui/backends/imgui_impl_opengl3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/imgui/backends/imgui_impl_vulkan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/ImGuizmo/ImGuizmo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/ImGuizmo/GraphEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/ImGuizmo/ImCurveEdit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/ImGuizmo/ImGradient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/ImGuizmo/ImSequencer.cpp
)

# Build as STATIC library
add_library(${PROJECT_NAME} STATIC
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
    ${IMGUI_SOURCES}
)

# Output folders (optional)
set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/${ARCH}/$<CONFIG>"
)

# Dependencies
add_subdirectory(Libraries/include/glad)
add_subdirectory(Libraries/include/spdlog)
add_subdirectory(Libraries/include/glfw)
add_subdirectory(Libraries/include/external/json)
add_subdirectory(Libraries/include/assimp)

add_library(vulkan-1 STATIC IMPORTED)
set_target_properties(vulkan-1 PROPERTIES
    IMPORTED_LOCATION "$ENV{VULKAN_SDK}/Lib/vulkan-1.lib"
    INTERFACE_INCLUDE_DIRECTORIES "$ENV{VULKAN_SDK}/Include"
)

# Include directories to propagate to Sandbox
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/entt
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/FileWatch
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/ImGuizmo
    $ENV{VULKAN_SDK}/Include
)

# Link dependencies (publicly)
target_link_libraries(${PROJECT_NAME} PUBLIC
    assimp
    vulkan-1
    glad
    spdlog
    nlohmann_json
    glfw
    opengl32
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
    IMGUI_IMPL_OPENGL_LOADER_GLAD
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
