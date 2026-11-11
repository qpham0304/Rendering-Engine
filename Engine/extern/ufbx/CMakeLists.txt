cmake_minimum_required(VERSION 3.5...4.0)
project(ufbx LANGUAGES C)

set(srcDir upstream)

add_library(ufbx STATIC ${srcDir}/ufbx.c)
add_library(ufbx::ufbx ALIAS ufbx)
target_include_directories(ufbx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${srcDir})
