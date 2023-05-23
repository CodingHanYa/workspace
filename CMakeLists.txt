cmake_minimum_required(VERSION 3.16)
project(workspace)

# Set library name
set(LIB_NAME   "workspace")
set(LIB_HEADER "include/workspace/")

# Set library installation path
install(DIRECTORY ${LIB_HEADER} DESTINATION ${LIB_NAME})