
set(CMAKE_CXX_STANDARD 17)
include_directories(../)

set(LIB "-lpthread")


add_executable( test_empty_task test_empty_task.cpp)
target_link_libraries(test_empty_task ${LIB})

add_executable( test_speedup test_speedup.cpp)
target_link_libraries(test_speedup ${LIB})

add_executable( compare_batch_submit compare_batch_submit.cpp)
target_link_libraries(compare_batch_submit ${LIB})

add_executable( compare_submit compare_submit.cpp)
target_link_libraries(compare_submit ${LIB})

add_executable( compare_other_task compare_other_task.cpp)
target_link_libraries(compare_other_task ${LIB})