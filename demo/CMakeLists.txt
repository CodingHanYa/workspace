
include_directories(../)

set(LIB "-lpthread")

add_executable( demo1 demo1.cpp)
target_link_libraries(demo1 ${LIB})

add_executable( demo2 demo2.cpp)
target_link_libraries(demo2 ${LIB})