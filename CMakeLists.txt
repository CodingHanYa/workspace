cmake_minimum_required(VERSION 3.2.5)

project(Hipe)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "-g -Wall")
set(CMAKE_CXX_FLAGS_DEBUG  "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

add_subdirectory(benchmark)
add_subdirectory(demo)
add_subdirectory(interfaces)
add_subdirectory(stability)