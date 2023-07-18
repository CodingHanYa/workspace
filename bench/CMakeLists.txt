cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 11)

project(
	Nanobench
	VERSION 1.0
	LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
	nanobench
	GIT_REPOSITORY https://github.com/martinus/nanobench.git
	GIT_TAG v4.3.11
	GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(nanobench)

find_package(Threads REQUIRED)

file(GLOB BENCH_SOURCES "bench*.cc")

foreach(BENCH_SOURCE ${BENCH_SOURCES})
	get_filename_component(BENCH_NAME ${BENCH_SOURCE} NAME_WE)
	add_executable(${BENCH_NAME} ${BENCH_SOURCE})
	target_link_libraries(${BENCH_NAME} PRIVATE nanobench)
	target_link_libraries(${BENCH_NAME} PRIVATE Threads::Threads)
	target_include_directories(${BENCH_NAME} PRIVATE ../include)
endforeach()
