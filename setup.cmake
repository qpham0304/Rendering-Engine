message(STATUS "Configuring CMake Presets...")

find_package(Git REQUIRED)

if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    message(STATUS "Checking git submodules...")
    
    # Check the status of submodules
    execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule status
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_SUBMOD_STATUS
        RESULT_VARIABLE GIT_SUBMOD_STATUS_RESULT
    )
    
    # initialize if submodules are uninitialized (marked with '-')
    if(GIT_SUBMOD_STATUS_RESULT EQUAL "0" AND GIT_SUBMOD_STATUS MATCHES "^\\s*-")
        message(STATUS "Uninitialized submodules detected. Updating...")
        
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE GIT_SUBMOD_RESULT
            ERROR_VARIABLE GIT_SUBMOD_ERROR
        )
        
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "Git submodule update failed:\n${GIT_SUBMOD_ERROR}")
        else()
            message(STATUS "Submodules updated successfully.")
        endif()
    else()
        message(STATUS "Submodules are already initialized. Skipping update.")
    endif()
endif()

option(USE_MSVC "Use the Visual Studio 2022 generator instead of Ninja" ON)   # msvc is used by default

function(configure_preset preset)
    message(STATUS "Configuring preset ${preset}...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} --preset ${preset}
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        RESULT_VARIABLE PRESET_RESULT
    )
    if(NOT PRESET_RESULT EQUAL "0")
        message(FATAL_ERROR "Configuring preset ${preset} failed")
    endif()
endfunction()

# dev build only: engine + Sandbox game as dlls, the editor hot reloads the game
# the standalone static game has its own build dir, configured by scripts/export-game.cmake when exporting
if(WIN32)
    message(STATUS "Configuring for Windows...")
    if(USE_MSVC)
        configure_preset(windows-msvc)
    else()  # NOTE: ninja build is not properly configured yet
        configure_preset(windows-ninja)
    endif()
elseif(APPLE)
    message(STATUS "Configuring for macOS...")
    # execute_process(COMMAND cmake --preset macos-default)
elseif(UNIX)
    message(STATUS "Configuring for Linux...")
    # execute_process(COMMAND cmake --preset linux-default)
else()
    message(FATAL_ERROR "Unsupported operating system!")
endif()

message(STATUS "Setup complete!")