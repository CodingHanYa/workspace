
# 显示编译器选项细节(optain)
function(CC_SHOW_OPTION_DETAIL BUILD_TYPE)
    message(STATUS "----------------- compiler options ---------------------")
    message(STATUS "Build type: [${BUILD_TYPE}]")
    message(STATUS "C++ flags: (debug)          configuration: ${CMAKE_CXX_FLAGS_DEBUG}")
    message(STATUS "C++ flags: (release)        configuration: ${CMAKE_CXX_FLAGS_RELEASE}")
    message(STATUS "C++ flags: (relwithdebinfo) configuration: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    message(STATUS "C++ flags: (minsizerel)     configuration: ${CMAKE_CXX_FLAGS_MINSIZEREL}")
    message(STATUS "Usage: cmake -D CMAKE_BUILD_TYPE=debug|release|...")
    message(STATUS "--------------------------------------------------------")   
endfunction()

# 批量 add_executable （找到所有.cc/.cpp/.C文件并调用add_executale)
# 可执行文件名: 相对文件名+文件名，并将"/"替换为"_". 其中相对文件名相对于CMakeLists.txt所在文件夹
# 统一添加链接库如: CC_ADD_EXECUTABLES(LINK_LIBRARIES Threads::Threads)
function(CC_ADD_EXECUTABLES)
    cmake_parse_arguments(ARG "" "LINK_LIBRARIES" "" ${ARGN})
    file(GLOB_RECURSE exes "*.cpp" "*.cc" "*.C")
    foreach(v ${exes})
        # get relative path
        file(RELATIVE_PATH relative_path ${CMAKE_CURRENT_SOURCE_DIR} ${v})
        # remove file extension from relative path
        string(REGEX REPLACE ".cpp|.cc|.C" "" target_name ${relative_path})
        # replace "/" with "_" in relative path
        string(REPLACE "/" "_" target_name ${target_name})
        # add the executable target
        add_executable(${target_name} ${v})
        if(ARG_LINK_LIBRARIES)
            target_link_libraries(${target_name} ${ARG_LINK_LIBRARIES})
        endif()

    endforeach()
endfunction()
# learn from chatGPT
