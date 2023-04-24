cmake_minimum_required(VERSION 3.0)

project(benchmark)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Release)

include_directories(../include)
find_package(Threads REQUIRED)

add_executable(bench1 bench1.cc)
target_link_libraries(bench1 PRIVATE Threads::Threads)

add_executable(bench2 bench2.cc)
target_link_libraries(bench2 PRIVATE Threads::Threads)

add_executable(bench3 bench3.cc)
target_link_libraries(bench3 PRIVATE Threads::Threads)