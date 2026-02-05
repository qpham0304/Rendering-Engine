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
# Shader compilation
find_program(GLSLC_EXECUTABLE glslc REQUIRED)

set(SPIRV_BINARY_FILES "")

foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_FILE ${SHADER} NAME)
    set(SPIRV_OUTPUT "${SHADER}.spv")

    add_custom_command(
        OUTPUT ${SPIRV_OUTPUT}
        # Use --target-env=vulkan explicitly
        # -x glsl tells the compiler to treat it as GLSL regardless of extension
        COMMAND ${GLSLC_EXECUTABLE} -x glsl --target-env=vulkan1.1 ${SHADER} -o ${SPIRV_OUTPUT}
        DEPENDS ${SHADER}
        MAIN_DEPENDENCY ${SHADER}
        COMMENT "Compiling shader ${SHADER_FILE} for Vulkan"
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
