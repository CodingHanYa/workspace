cmake_minimum_required(VERSION 3.5)

project(hipe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)      # c++11
include_directories(include)    # head
find_package(Threads REQUIRED)  # thread

# script
include(cmake/cc_funcs.cmake)

# If the compiler is GNU, I think I need "-Wall -Werror"
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Werror")
endif()

# default: release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# show compiler options
CC_SHOW_OPTION_DETAIL(${CMAKE_BUILD_TYPE})

add_subdirectory(benchmark)     # sub dir
add_subdirectory(demo)          # sub dir
add_subdirectory(interfaces)    # sub dir

