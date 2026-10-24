# Exports Sandbox as a standalone game, engine + game statically linked into one executable
#
#   cmake -P scripts/export-game.cmake                       Release into export/<preset>
#   cmake -DCONFIG=Debug -P scripts/export-game.cmake
#   cmake -DOUTPUT_DIR=D:/MyGame -P scripts/export-game.cmake
#
# uses its own build dir (WK_EXPORT_GAME=ON preset), so the dev dll build is never touched.
# the output folder is the exe plus its runtime assets (compiled spv, no shader sources), ready to zip

cmake_minimum_required(VERSION 3.25)

get_filename_component(ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT CONFIG)
    set(CONFIG Release)
endif()

# one export preset per target platform, add linux / macos / ios / android
if(CMAKE_HOST_WIN32)
    set(PRESET windows-export)
    set(BUILD_DIR "${ROOT_DIR}/build/export-windows")
else()
    message(FATAL_ERROR "No export preset for this platform yet")
endif()

set(DEFAULT_OUTPUT_DIR "${ROOT_DIR}/export/${PRESET}")
if(NOT OUTPUT_DIR)
    set(OUTPUT_DIR "${DEFAULT_OUTPUT_DIR}")
endif()

function(run_step description)
    message(STATUS "${description}...")
    execute_process(
        COMMAND ${ARGN}
        WORKING_DIRECTORY ${ROOT_DIR}
        RESULT_VARIABLE STEP_RESULT
    )
    if(NOT STEP_RESULT EQUAL "0")
        message(FATAL_ERROR "${description} failed")
    endif()
endfunction()

run_step("Configuring ${PRESET}" ${CMAKE_COMMAND} --preset ${PRESET})
run_step("Building Sandbox (${CONFIG})" ${CMAKE_COMMAND} --build ${BUILD_DIR} --config ${CONFIG} --target Sandbox --parallel)

# only wipe the folder this script owns, a custom OUTPUT_DIR may hold other files
if(OUTPUT_DIR STREQUAL DEFAULT_OUTPUT_DIR)
    file(REMOVE_RECURSE "${OUTPUT_DIR}")
endif()

# component "game" only, the extern libraries come with their own install rules we don't want to ship
run_step("Installing game into ${OUTPUT_DIR}" ${CMAKE_COMMAND} --install ${BUILD_DIR} --config ${CONFIG} --component game --prefix ${OUTPUT_DIR})

message(STATUS "Exported game: ${OUTPUT_DIR}")
