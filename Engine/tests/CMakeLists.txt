# EngineTests: fast tests with no window and no GPU. New .cpp files here are picked up on the next configure
include(${CMAKE_SOURCE_DIR}/cmake/WkTesting.cmake)

file(GLOB_RECURSE ENGINE_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

wk_add_test_executable(EngineTests
    LABEL unit
    SOURCES ${ENGINE_TEST_SOURCES}
)
