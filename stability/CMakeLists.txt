include_directories(../)

set(LIB "-lpthread")

add_executable( test_balance test_balance.cpp)
target_link_libraries(test_balance ${LIB})

add_executable( test_dynamic test_dynamic.cpp)
target_link_libraries(test_dynamic ${LIB})

add_executable( test_steady test_steady.cpp)
target_link_libraries(test_steady ${LIB})