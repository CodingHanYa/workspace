cmake_minimum_required(VERSION 3.2.5)

set(CMAKE_CXX_STANDARD 11 Required)
project(test_interface LANGUAGES CXX)

IF(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
ENDIF()

find_package(Threads REQUIRED)

# target: test_steady_pond_interface
add_executable(test_steady_pond_interface test_steady_pond_interface.cpp)
target_include_directories(test_steady_pond_interface PUBLIC ../)
target_link_libraries(test_steady_pond_interface PUBLIC Threads::Threads)

# target: test_dynamic_pond_interface
add_executable(test_dynamic_pond_interface test_dynamic_pond_interface.cpp)
target_include_directories(test_dynamic_pond_interface PUBLIC ../)
target_link_libraries(test_dynamic_pond_interface PUBLIC Threads::Threads)
