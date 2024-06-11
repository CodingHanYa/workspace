cmake_minimum_required(VERSION 3.0)

project(benchmark)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Release)

include_directories(../include)
find_package(Threads REQUIRED)

file(GLOB BENCH_SOURCES "bench*.cc")

foreach(BENCH_SOURCE ${BENCH_SOURCES})
	get_filename_component(BENCH_NAME ${BENCH_SOURCE} NAME_WE)
	add_executable(${BENCH_NAME} ${BENCH_SOURCE})
	target_link_libraries(${BENCH_NAME} PRIVATE Threads::Threads)
	target_include_directories(${BENCH_NAME} PRIVATE ../include)
endforeach()
