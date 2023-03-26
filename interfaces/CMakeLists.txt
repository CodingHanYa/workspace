cmake_minimum_required(VERSION 3.5.1)

project(test_interface LANGUAGES CXX)

find_package(Threads REQUIRED) 
include_directories(../include)

add_executable(test_steady_pond_interface test_steady_pond_interface.cpp)
target_link_libraries(test_steady_pond_interface PUBLIC Threads::Threads)

add_executable(test_dynamic_pond_interface test_dynamic_pond_interface.cpp)
target_link_libraries(test_dynamic_pond_interface PUBLIC Threads::Threads)
