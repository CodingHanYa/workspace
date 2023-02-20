
cmake_minimum_required(VERSION 3.2.5)

include_directories(../)
set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -g -O2 -pthread")

add_executable( demo1 demo1.cpp)
add_executable( demo2 demo2.cpp)