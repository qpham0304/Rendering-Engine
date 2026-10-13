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

# option to build as static library
option(BUILD_AS_SHARED "Build as shared library" OFF)   # libraries are built static by default
option(USE_MSVC "Build as shared library" ON)           # msvc is used by default

if(WIN32)
    message(STATUS "Configuring for Windows...")
    if(BUILD_AS_SHARED) # NOTE: shared library build not properly configured yet
        if(USE_MSVC)
            message(STATUS "Configuring for shared library using msvc...")
            execute_process(COMMAND cmake --preset windows-msvc-shared) 
        else()
            message(STATUS "Configuring for shared library using ninja...")
            execute_process(COMMAND cmake --preset windows-ninja-shared)
        endif()
    else()
        if(USE_MSVC)
            message(STATUS "Configuring for static library using msvc...")
            execute_process(COMMAND cmake --preset windows-msvc)
        else()  # NOTE: ninja build is not properly configured yet
            message(STATUS "Configuring for static library using ninja...")
            execute_process(COMMAND cmake --preset windows-ninja)
        endif()
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