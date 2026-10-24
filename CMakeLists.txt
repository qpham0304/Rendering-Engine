cmake_minimum_required(VERSION 3.5...4.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(MyEngineWorkspace)

# OFF (dev, default): engine + Sandbox game build as dlls, the editor hot reloads the game, everything reads Sandbox/assets
# ON (export): standalone Sandbox exe with everything statically linked (mobile / apple need static),
#              assets are installed next to it by scripts/export-game.cmake, no editor
option(WK_EXPORT_GAME "Build the standalone statically linked game instead of the dll dev build" OFF)

if(WK_EXPORT_GAME)
    set(WK_BUILD_SHARED OFF)
    set(WK_LIB_TYPE STATIC)
else()
    set(WK_BUILD_SHARED ON)
    set(WK_LIB_TYPE SHARED)
endif()

add_subdirectory(Engine)
add_subdirectory(Sandbox)
if(NOT WK_EXPORT_GAME)
    add_subdirectory(Editor)
    target_compile_definitions(Editor PRIVATE
        $<$<CONFIG:Debug>:DEBUG>
        $<$<CONFIG:Release>:RELEASE>
    )
endif()

target_compile_definitions(SandboxGame PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:RELEASE>
)
target_compile_definitions(Sandbox PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:RELEASE>
)