cmake_minimum_required(VERSION 3.5...4.0)

project(Editor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

add_executable(${PROJECT_NAME} ${EDITOR_SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE MyGraphicsEngine)

set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin/${BUILD_PLATFORM}/$<CONFIG>")
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
)

# the editor always works on the game project's source assets (shaders included), edits and saves land there
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WK_WORKING_DIR="$<TARGET_PROPERTY:SandboxGame,SOURCE_DIR>"
)

# game dll is loaded at runtime so it can be rebuilt and hot reloaded while the editor runs, not linked
add_dependencies(${PROJECT_NAME} SandboxGame)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WK_GAME_MODULE_PATH="$<TARGET_FILE:SandboxGame>"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND_EXPAND_LISTS
    COMMENT "Copying engine dlls next to ${PROJECT_NAME}"
)
