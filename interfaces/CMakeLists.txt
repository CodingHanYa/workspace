cmake_minimum_required(VERSION 3.2.5)

include_directories(../)
set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -g -O2 -pthread")

add_executable( test_steady_pond_interface test_steady_pond_interface.cpp)
add_executable( test_dynamic_pond_interface test_dynamic_pond_interface.cpp)