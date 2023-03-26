
cmake_minimum_required(VERSION 3.2.5)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-pthread")

project(demos)
include(../include)
add_executable(demo1 demo1.cpp)
add_executable(demo2 demo2.cpp)


