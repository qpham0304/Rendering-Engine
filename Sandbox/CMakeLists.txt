cmake_minimum_required(VERSION 3.5...4.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Sandbox LANGUAGES CXX)

file(GLOB_RECURSE SANDBOX_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

file(GLOB_RECURSE SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/*.comp
)

add_executable(${PROJECT_NAME} ${SANDBOX_SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE MyGraphicsEngine)

# Shader compilation
find_program(GLSLC_EXECUTABLE glslc REQUIRED)

set(SPIRV_BINARY_FILES "")

foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_FILE ${SHADER} NAME)
    get_filename_component(SHADER_EXT ${SHADER} LAST_EXT) # Get .vert, .frag, or .comp
    
    # Map extensions to glslc stage flags
    if(SHADER_EXT STREQUAL ".vert")
        set(STAGE_FLAG "-fshader-stage=vertex")
    elseif(SHADER_EXT STREQUAL ".frag")
        set(STAGE_FLAG "-fshader-stage=fragment")
    elseif(SHADER_EXT STREQUAL ".comp")
        set(STAGE_FLAG "-fshader-stage=compute")
    else()
        set(STAGE_FLAG "") # Fallback to glslc default detection
    endif()

    set(SPIRV_OUTPUT "${SHADER}.spv")

    add_custom_command(
        OUTPUT ${SPIRV_OUTPUT}
        # Add ${STAGE_FLAG} to the command
        COMMAND ${GLSLC_EXECUTABLE} ${STAGE_FLAG} -x glsl --target-env=vulkan1.1 ${SHADER} -o ${SPIRV_OUTPUT}
        DEPENDS ${SHADER}
        MAIN_DEPENDENCY ${SHADER}
        COMMENT "Compiling ${SHADER_FILE} (${SHADER_EXT}) for Vulkan"
        VERBATIM
    )

    list(APPEND SPIRV_BINARY_FILES ${SPIRV_OUTPUT})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(${PROJECT_NAME} Shaders)

# Output location
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>"
)

# Copy assets to build folder
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)
