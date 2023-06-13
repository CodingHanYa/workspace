cmake_minimum_required(VERSION 3.0)

project(example)

set(CMAKE_CXX_STANDARD 11)

include_directories(../include)
find_package(Threads REQUIRED)

add_executable(e1 e1.cc)
target_link_libraries(e1 PRIVATE Threads::Threads)

add_executable(e2 e2.cc)
target_link_libraries(e2 PRIVATE Threads::Threads)

add_executable(e3 e3.cc)
target_link_libraries(e3 PRIVATE Threads::Threads)

add_executable(e4 e4.cc)
target_link_libraries(e4 PRIVATE Threads::Threads)

add_executable(e5 e5.cc)
target_link_libraries(e5 PRIVATE Threads::Threads)

add_executable(e6 e6.cc)
target_link_libraries(e6 PRIVATE Threads::Threads)
