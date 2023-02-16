include_directories(../)

set(LIB "-lpthread")

add_executable( test_steady_pond_interface test_steady_pond_interface.cpp)
target_link_libraries(test_steady_pond_interface ${LIB})

add_executable( test_dynamic_pond_interface test_dynamic_pond_interface.cpp)
target_link_libraries(test_dynamic_pond_interface ${LIB})