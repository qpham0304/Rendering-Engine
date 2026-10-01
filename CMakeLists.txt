cmake_minimum_required(VERSION 3.5...4.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(MyEngineWorkspace)

# dll build option but only use for window build will not be supported for cross platform
option(WK_BUILD_SHARED "Build engine modules as shared libraries (DLLs)" OFF)

if(WK_BUILD_SHARED)
    set(WK_LIB_TYPE SHARED)
else()
    set(WK_LIB_TYPE STATIC)
endif()

add_subdirectory(Engine)
add_subdirectory(Editor)
add_subdirectory(Sandbox)

target_compile_definitions(Sandbox PRIVATE 
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:RELEASE>
)
target_compile_definitions(Editor PRIVATE 
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:RELEASE>
)