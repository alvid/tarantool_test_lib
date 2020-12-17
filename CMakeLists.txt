cmake_minimum_required(VERSION 3.17)
cmake_policy(SET CMP0076 NEW)   #prevent absolute source file paths

project(Lua_RTSIB VERSION 1.0.0)

set(LIBNAME rtsib)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	add_definitions(-D_DEBUG)
	set(ENABLE_DEBUG ON)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_SKIP_BUILD_RPATH true)
set(ENABLE_TESTING ON)

add_compile_options("-std=gnu++17")

include_directories("Interface")
include_directories("Sources")
include_directories("/usr/include/tarantool")

add_library(${LIBNAME} SHARED
        "Sources/library.cpp"
        "Sources/Dispatcher_fiber.cpp"
        "Sources/Dispatcher_fiber.hpp"
        "Sources/RTSIB_server.cpp"
        "Sources/RTSIB_server.hpp"
        )

if(${ENABLE_DEBUG})
    target_compile_options(${LIBNAME} PUBLIC -o0 -g3 -ggdb -gstabs+)
else()
    target_compile_options(${LIBNAME} PUBLIC -o2)
endif()

target_link_libraries(${LIBNAME} ${EXT_LIBRARY_FILES})
target_link_options(${LIBNAME} PUBLIC LINKER:--export-dynamic -ldl)
