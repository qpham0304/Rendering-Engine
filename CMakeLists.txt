cmake_minimum_required(VERSION 3.5...4.0)

project(MyGraphicsEngine LANGUAGES CXX)

# for glad C 
enable_language(C)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH "x64")
else()
    set(ARCH "Win32")
endif()


if(ARCH STREQUAL "x64")
    set(CMAKE_CXX_STANDARD 20)
else()
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)


file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)


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

add_executable(
    ${PROJECT_NAME}
    ${ENGINE_SOURCES}
    ${IMGUI_SOURCES}
)


set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/${PROJECT_NAME}/${ARCH}/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/${PROJECT_NAME}/${ARCH}/$<CONFIG>"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/${PROJECT_NAME}/${ARCH}/$<CONFIG>"
)

add_subdirectory(Libraries/include/glad)
add_subdirectory(Libraries/include/spdlog)
add_subdirectory(Libraries/include/glfw)
add_subdirectory(Libraries/include/nlohmann_json)
add_subdirectory(Libraries/include/assimp)

add_library(vulkan-1 STATIC IMPORTED)
set_target_properties(vulkan-1 PROPERTIES
    IMPORTED_LOCATION "$ENV{VULKAN_SDK}/Lib/vulkan-1.lib"
    INTERFACE_INCLUDE_DIRECTORIES "$ENV{VULKAN_SDK}/Include"
)


target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/entt
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/FileWatch
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/Libraries/include/ImGuizmo
)


target_link_libraries(${PROJECT_NAME} PRIVATE
    assimp
    vulkan-1
    opengl32
    glad
    spdlog
    nlohmann_json
    glfw
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32
    _CONSOLE
    IMGUI_IMPL_OPENGL_LOADER_GLAD
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# If using dynamic GLFW (glfw3.dll)
# target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_DLL)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:/W3 /SDL /MP /Zc:__cplusplus>
    $<$<CONFIG:Release>:/W3 /O2 /GL /Oi /fp:precise /SDL /MP /Zc:__cplusplus>
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/dll"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/Textures"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/Textures
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/src
)

foreach(SRC ${ENGINE_SOURCES})
    file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src" "${SRC}")
    get_filename_component(DIR "${REL_PATH}" PATH)
    string(REPLACE "/" "\\" DIR_GROUP "${DIR}")
    if(DIR_GROUP STREQUAL "")
        set(DIR_GROUP "Source")
    else()
        set(DIR_GROUP "Source\\${DIR_GROUP}")
    endif()
    source_group("${DIR_GROUP}" FILES "${SRC}")
endforeach()
