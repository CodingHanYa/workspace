cmake_minimum_required(VERSION 3.22)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

project(HipeThreadPool VERSION 1.0.0 LANGUAGES CXX)

# add_subdirectory()

file (GLOB_RECURSE srcs CONFIGURE_DEPENDS src_path)
add_library(libname STATIC ${srcs})
target_include_directories(libname PUBLIC include_path)

find_package(Threads REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} Threads::Threads)

file(GLOB_RECURSE src ${PROJECT_SOURCE_DIR})

set(VERBOSE ON)
if (VERBOSE)
    message(STATUS "project name: ${PROJECT_NAME}")
    message(STATUS "project source dir: ${PROJECT_SOURCE_DIR}")
    message(STATUS "project binary dir: ${PROJECT_BINARY_DIR}")

    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        message(STATUS "system name: Linux")
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        message(STATUS "system name: Windows")
    else()
        message(STATUS "system name: Unknown")
    endif()

    if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
        message(STATUS "compiler : GNU")
    elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
        message(STATUS "compiler : Clang")
    elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "Microsoft")
        message(STATUS "compiler: Microsoft")
    else()
        message(STATUS "compiler: Unknown")
    endif()


endif()


set(CMAKE_INSTALL_PREFIX /usr/local)

include(FetchContent)

option(BUILD_DEMO "build demo files" ON)
option(ENABLE_TEST "build test files" ON)

if (BUILD_DEMO)
    message(STATUS "build demo files")
endif()

if(ENABLE_TEST)
    message(STATUS "build test files")
    add_subdirectory(tests)
endif()