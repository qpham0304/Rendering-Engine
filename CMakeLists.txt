cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------
# Project Setup
# ---------------------------------------------------------
project(MyGraphicsEngine LANGUAGES CXX)

# Future-proof:
# When you later split engine into a library:
#   change add_executable(...) -> add_library(...)
#   move main.cpp into a separate project
#
# Everything else will still work.
# ---------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------
# Source File Collection
# ---------------------------------------------------------
# Recursively collect all source files in src/
file(GLOB_RECURSE ENGINE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

# NOTE: main.cpp is included in that search.
# If you later separate it, remove it from ENGINE_SOURCES
# and create a new project for the app/game.

# ---------------------------------------------------------
# Engine Executable (change to add_library later)
# add_library(MyGraphicsEngine STATIC ${ENGINE_SOURCES})
# ---------------------------------------------------------
add_executable(${PROJECT_NAME}
    ${ENGINE_SOURCES}
)

# ---------------------------------------------------------
# Include Directories
# ---------------------------------------------------------
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    $ENV{VULKAN_SDK}/include
)

# ---------------------------------------------------------
# Library Paths
# ---------------------------------------------------------
link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/lib
    $ENV{VULKAN_SDK}/lib
)

# ---------------------------------------------------------
# Link Libraries
# ---------------------------------------------------------
# Static libs you said exist:
#   assimp-vc143-mtd.lib
#   glfw3.lib
#   spdlog.lib
#
# Vulkan loader:
#   vulkan-1.lib
#
# Windows system libs:
#   opengl32.lib
# ---------------------------------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE
    assimp-vc143-mtd
    glfw3
    spdlog
    vulkan-1
    opengl32
)

# ---------------------------------------------------------
# Optional: ImGui, ImGuizmo, Glad, stb, ENTt, glm, etc.
# Since these are header-only or compiled inside your repo,
# no linking is required beyond including their headers.
# ---------------------------------------------------------

# ---------------------------------------------------------
# Debug / Release Configuration
# ---------------------------------------------------------
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    WIN32
    _CONSOLE
)

# Visual Studio filters (helps in the VS Solution)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source" FILES ${ENGINE_SOURCES})
