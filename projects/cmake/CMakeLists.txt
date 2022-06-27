cmake_minimum_required(VERSION 3.5)

set(CMAKE_VERBOSE_MAKEFILE ON)

# project name, use this name to create executable file 
project(netplus CXX C)

# SET(CMAKE_BUILD_TYPE "Release")
# build flags
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

# add source file for lib
aux_source_directory(../../3rd/c-ares/c-ares-1.18.1 PROGRAM_SOURCE)
aux_source_directory(../../src/ PROGRAM_SOURCE)
aux_source_directory(../../src/handler PROGRAM_SOURCE)
aux_source_directory(../../src/http PROGRAM_SOURCE)
aux_source_directory(../../src/logger PROGRAM_SOURCE)
aux_source_directory(../../src/os PROGRAM_SOURCE)
aux_source_directory(../../src/task PROGRAM_SOURCE)
aux_source_directory(../../src/thread_impl PROGRAM_SOURCE)

if (WIN32)
  aux_source_directory(../../3rd/stack_walker PROGRAM_SOURCE)
  aux_source_directory(../../3rd/getopt PROGRAM_SOURCE)
endif()

# create netplus.a
set(LIB_NAME netplus)
add_library(${LIB_NAME} STATIC ${PROGRAM_SOURCE})


if (NOT WIN32)
  TARGET_LINK_LIBRARIES(${LIB_NAME} dl)
  TARGET_LINK_LIBRARIES(${LIB_NAME} pthread)
endif ()

if (WIN32)
  MESSAGE(STATUS "windows now")
  add_definitions(
          -DWIN32
          -D_CRT_SECURE_NO_WARNINGS
          -DNOMINMAX
          -DWINDOWS
          -DNETP_ENABLE_DNS_RESOLVER
          -DNO_IPHLPAPI
          -DWIN32_LEAN_AND_MEAN
          -DCARES_STATICLIB
  )
  set(CMAKE_SYSTEM_VERSION 10.0.18362.0)
  target_include_directories(${LIB_NAME} 
    PUBLIC
      ../../3rd/stack_walker
      ../../3rd/getopt
	    ../../3rd/c-ares/c-ares-1.18.1/include/
      ../../include
  )
elseif (APPLE)
  MESSAGE(STATUS "apple now")
elseif (UNIX)
  MESSAGE(STATUS "unix-like now")
else()
  MESSAGE(STATUS "Unkown Platform")
endif ()
# inclue hpp files
target_include_directories(${LIB_NAME} 
  PUBLIC
    ../../include
    ../../3rd/c-ares/c-ares-1.18.1/include/
    ../../3rd/stack_walker
      ../../3rd/getopt
)


# set install
install(TARGETS ${LIB_NAME} DESTINATION netplus_demo/lib)
# install(FILES ../../include/ DESTINATION netplus_demo/include)
install(DIRECTORY ../../include/ DESTINATION netplus_demo/include)
install(DIRECTORY ../../3rd/ DESTINATION netplus_demo/3rd)

# Create executable file with netplus
add_executable(hello_world ../../test/hello_world/src/main.cpp)
TARGET_LINK_LIBRARIES(hello_world PRIVATE ${LIB_NAME})
