# lua scripting as a static lib linked into the engine. export all only scans the engine's own objects,
# so the sol template instantiations are not exported and keep the dll under the 65535 export limit
file(GLOB_RECURSE LUA_SCRIPTING_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

# compiled as part of the engine: same includes and defines, and the engine's export macro so its classes resolve inside the dll
add_library(MyGraphicsEngineLua STATIC ${LUA_SCRIPTING_SOURCES})
target_include_directories(MyGraphicsEngineLua PRIVATE $<TARGET_PROPERTY:${PROJECT_NAME},INCLUDE_DIRECTORIES>)
target_compile_definitions(MyGraphicsEngineLua PRIVATE
    $<TARGET_PROPERTY:${PROJECT_NAME},COMPILE_DEFINITIONS>
    $<$<BOOL:${WK_BUILD_SHARED}>:${PROJECT_NAME}_EXPORTS>
)
target_precompile_headers(MyGraphicsEngineLua PRIVATE ${PROJECT_SOURCE_DIR}/src/core/features/pch.h)

if(MSVC)
    target_compile_options(MyGraphicsEngineLua PRIVATE /bigobj /utf-8 /MP)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE MyGraphicsEngineLua)
