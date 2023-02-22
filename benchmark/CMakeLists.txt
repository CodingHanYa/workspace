cmake_minimum_required(VERSION 3.2.5)

include_directories(../)
set(CMAKE_CXX_FLAGS "-std=c++17 -Wall -g -O2 -pthread")

add_executable( test_empty_task       test_empty_task.cpp)
add_executable( test_speedup          test_speedup.cpp)
add_executable( compare_batch_submit  compare_batch_submit.cpp)
add_executable( compare_submit        compare_submit.cpp)
add_executable( compare_other_task    compare_other_task.cpp)