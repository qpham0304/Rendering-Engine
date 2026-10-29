# shared GoogleTest setup, included by every project's tests/CMakeLists.txt
include_guard(GLOBAL)

# pinned release so every machine builds the same version
include(FetchContent)
set(FETCHCONTENT_BASE_DIR ${CMAKE_BINARY_DIR}/_deps)    # box3d caches FETCHCONTENT_BASE_DIR into the source tree, keep googletest in the build folder
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)    # match the engine's /MD runtime
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# wk_add_test_executable(<target> LABEL <label> [WORKING_DIRECTORY <dir>] [TIMEOUT <sec>] SOURCES <files...>)
# links the engine + gtest, gives the target the engine's compile flags and puts it in VS Test Explorer
function(wk_add_test_executable target)
    cmake_parse_arguments(ARG "" "LABEL;WORKING_DIRECTORY;TIMEOUT" "SOURCES" ${ARGN})
    if(NOT ARG_WORKING_DIRECTORY)
        set(ARG_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})    # memory_stats.json is written to the working dir at exit
    endif()
    if(NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT 60)
    endif()

    add_executable(${target} ${ARG_SOURCES})
    target_link_libraries(${target} PRIVATE MyGraphicsEngine GTest::gtest)
    target_compile_definitions(${target} PRIVATE
        NOMINMAX
        VK_NO_PROTOTYPES
        VMA_STATIC_VULKAN_FUNCTIONS=0
        VMA_DYNAMIC_VULKAN_FUNCTIONS=1
        IMGUI_IMPL_VULKAN_NO_PROTOTYPES
        IMGUI_DEFINE_MATH_OPERATORS
        IMGUI_IMPL_OPENGL_LOADER_GLAD
        $<$<CONFIG:Debug>:DEBUG>
        $<$<CONFIG:Release>:RELEASE>
    )
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/Engine/tests/support)    # TestHelpers.h
    target_precompile_headers(${target} PRIVATE ${CMAKE_SOURCE_DIR}/Engine/src/core/features/pch.h)
    if(MSVC)
        target_compile_options(${target} PRIVATE /bigobj /utf-8 /MP)
    endif()
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>"
        VS_DEBUGGER_WORKING_DIRECTORY "${ARG_WORKING_DIRECTORY}"
        FOLDER "Tests"
    )

    # dev build links the engine as a dll, the test exe needs it next to it to start (and to list its tests)
    if(WK_BUILD_SHARED)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS
            COMMENT "Copying engine dlls next to ${target}"
        )
    endif()

    gtest_discover_tests(${target}
        DISCOVERY_MODE PRE_TEST
        WORKING_DIRECTORY "${ARG_WORKING_DIRECTORY}"
        PROPERTIES LABELS "${ARG_LABEL}" TIMEOUT ${ARG_TIMEOUT}
    )
endfunction()
