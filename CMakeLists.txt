cmake_minimum_required(VERSION 3.16)
project(workspace)

# Set library name
set(LIB_NAME "workspace")
set(LIB_HEADER "include/workspace/")

# Specify installation paths
if(UNIX)
    set(INSTALL_INCLUDE_DIR "include" CACHE PATH "Installation directory for headers")
elseif(WIN32)
    set(INSTALL_INCLUDE_DIR "include" CACHE PATH "Installation directory for headers")
endif()

if(UNIX AND NOT APPLE)   # unix
  set(INSTALL_PREFIX "/usr/local") 
  set(INSTALL_INCLUDEDIR "${INSTALL_PREFIX}/include")
elseif(APPLE)            # mac
  set(INSTALL_PREFIX "/usr/local")
  set(INSTALL_INCLUDEDIR "${INSTALL_PREFIX}/include")
elseif(WIN32)            # win
  set(INSTALL_PREFIX "C:/Program Files")
  set(INSTALL_INCLUDEDIR "${INSTALL_PREFIX}/workspace/include")
endif()


# Set library installation path
install(DIRECTORY ${LIB_HEADER} DESTINATION ${INSTALL_INCLUDEDIR}/${LIB_NAME})