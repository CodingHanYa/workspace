cmake_minimum_required(VERSION 3.5)

project(benchmark)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Werror")

message(STATUS "--------------------------------------------------------")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: [${CMAKE_BUILD_TYPE}]")

message(STATUS "C++ flags: (debug)          configuration: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "C++ flags: (release)        configuration: ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "C++ flags: (relwithdebinfo) configuration: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message(STATUS "C++ flags: (minsizerel     configuration: ${CMAKE_CXX_FLAGS_MINSIZEREL}")
message(STATUS "Usage: cmake -D CMAKE_BUILD_TYPE=debug|release|...")
message(STATUS "--------------------------------------------------------")

include_directories(../include)
add_executable( test_empty_task       test_empty_task.cpp)
add_executable( test_speedup          test_speedup.cpp)
add_executable( compare_batch_submit  compare_batch_submit.cpp)
add_executable( compare_submit        compare_submit.cpp)
add_executable( compare_other_task    compare_other_task.cpp)