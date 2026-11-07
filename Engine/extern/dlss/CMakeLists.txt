cmake_minimum_required(VERSION 3.5...4.0)
project(dlss)

# NVIDIA DLSS SDK (https://github.com/NVIDIA/DLSS), its files are not in git (license), only this CMakeLists is.
# without the SDK the target is empty and the engine builds without DLSS
add_library(dlss INTERFACE)

set(DLSS_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/Windows_x86_64)
if(WIN32 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/include/nvsdk_ngx_vk.h)
    target_include_directories(dlss INTERFACE include)
    target_link_libraries(dlss INTERFACE
        $<IF:$<CONFIG:Debug>,${DLSS_LIB_DIR}/x64/nvsdk_ngx_d_dbg.lib,${DLSS_LIB_DIR}/x64/nvsdk_ngx_d.lib>
    )
    # ngx loads nvngx_dlss.dll at runtime, from here as well as the exe folder
    target_compile_definitions(dlss INTERFACE WK_DLSS WK_DLSS_RUNTIME_DIR="${DLSS_LIB_DIR}/rel")
endif()
