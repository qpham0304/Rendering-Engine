cmake_minimum_required(VERSION 3.5...4.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(MyEngineWorkspace)

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