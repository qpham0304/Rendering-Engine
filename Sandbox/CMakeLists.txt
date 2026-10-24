cmake_minimum_required(VERSION 3.5...4.0)

project(Sandbox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

# game code only, runtime/ holds the exe entry point
file(GLOB_RECURSE SANDBOX_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

file(GLOB_RECURSE SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.rgen
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.rmiss
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.rchit
)

# shared includes, a change in one of them has to recompile every shader
file(GLOB_RECURSE SHADER_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/common/*.glsl
)

set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin/${BUILD_PLATFORM}/$<CONFIG>")
set(GAME_TARGET ${PROJECT_NAME}Game)

# dll the editor hot reloads in the dev build, a static lib in the export
add_library(${GAME_TARGET} ${WK_LIB_TYPE} ${SANDBOX_SOURCES})
target_link_libraries(${GAME_TARGET} PUBLIC MyGraphicsEngine)
target_include_directories(${GAME_TARGET} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

include(GenerateExportHeader)
generate_export_header(${GAME_TARGET}
    BASE_NAME SANDBOX
    EXPORT_FILE_NAME include/Sandbox/SandboxAPI.h
)

set_target_properties(${GAME_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
)

# embed only the pdb file name, the editor loads a copy of dll + pdb so the originals stay free to relink
if(MSVC AND NOT WK_EXPORT_GAME)
    target_link_options(${GAME_TARGET} PRIVATE /PDBALTPATH:%_PDB%)
endif()

# exported runtime, the installed game
add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/runtime/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${GAME_TARGET})

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
)

# standalone game reads assets next to the exe, scripts/export-game.cmake installs this component
if(WK_EXPORT_GAME)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION . COMPONENT game)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION . COMPONENT game
        PATTERN "*.vert" EXCLUDE
        PATTERN "*.frag" EXCLUDE
        PATTERN "*.comp" EXCLUDE
        PATTERN "*.rgen" EXCLUDE
        PATTERN "*.rmiss" EXCLUDE
        PATTERN "*.rchit" EXCLUDE
        PATTERN "*.glsl" EXCLUDE
        PATTERN "common" EXCLUDE
        PATTERN "compile.bat" EXCLUDE
    )
else()
    # dev build reads the source assets in place, nothing gets copied
    target_compile_definitions(${PROJECT_NAME} PRIVATE WK_WORKING_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying engine dlls next to ${PROJECT_NAME}"
    )
endif()

set(SPV_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/spv")
set(SPIRV_BINARY_FILES "")

foreach(SHADER_SOURCE ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${SHADER_SOURCE} NAME)
    set(SPIRV_OUT "${SPV_OUTPUT_DIR}/${FILE_NAME}.spv")

    add_custom_command(
        OUTPUT ${SPIRV_OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_OUTPUT_DIR}
        COMMAND Vulkan::glslc --target-env=vulkan1.2 ${SHADER_SOURCE} -o ${SPIRV_OUT}
        DEPENDS ${SHADER_SOURCE} ${SHADER_INCLUDES}
        COMMENT "Compiling shader: ${FILE_NAME}"
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV_OUT})
endforeach()

add_custom_target(${PROJECT_NAME}_Shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(${GAME_TARGET} ${PROJECT_NAME}_Shaders)
