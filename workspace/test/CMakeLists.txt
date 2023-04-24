cmake_minimum_required(VERSION 3.0)

project(test)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")


include_directories(../include)
find_package(Threads REQUIRED)

add_executable(test_workbranch test_workbranch.cc)
target_link_libraries(test_workbranch PRIVATE Threads::Threads)

add_executable(test_workspace test_workspace.cc)
target_link_libraries(test_workspace PRIVATE Threads::Threads)

add_executable(test_exception test_exception.cc)
target_link_libraries(test_exception PRIVATE Threads::Threads)

add_executable(test_supervisor test_supervisor.cc)
target_link_libraries(test_supervisor PRIVATE Threads::Threads)
