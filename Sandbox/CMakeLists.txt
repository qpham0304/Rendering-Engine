cmake_minimum_required(VERSION 3.5...4.0)

project(Sandbox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

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

set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>")
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
)

set(SPV_OUTPUT_DIR "${OUTPUT_DIR}/assets/shaders/spv")
set(SPIRV_BINARY_FILES "")

foreach(SHADER_SOURCE ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${SHADER_SOURCE} NAME)
    set(SPIRV_OUT "${SPV_OUTPUT_DIR}/${FILE_NAME}.spv")

    add_custom_command(
        OUTPUT ${SPIRV_OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SPV_OUTPUT_DIR}
        COMMAND Vulkan::glslc ${SHADER_SOURCE} -o ${SPIRV_OUT}
        DEPENDS ${SHADER_SOURCE}
        COMMENT "Compiling shader: ${FILE_NAME}"
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV_OUT})
endforeach()

add_custom_target(${PROJECT_NAME}_Shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_Shaders)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            ${OUTPUT_DIR}/assets
    COMMENT "Syncing assets to build directory"
)