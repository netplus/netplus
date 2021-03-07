# cmake 版本要求
cmake_minimum_required(VERSION 3.1)

set(CMAKE_VERBOSE_MAKEFILE ON)

# 工程名
project(netplus CXX C)

# 包含头文件
include_directories(
        ../../3rd/http_parser
        ../../3rd/udns/0.4
        ../../include
)
if (WIN32)
    MESSAGE(STATUS "Now is windows")
elseif (APPLE)
    MESSAGE(STATUS "Now is Apple systems.")
    #include(machine/endian.h)
elseif (UNIX)
    MESSAGE(STATUS "Now is UNIX-like OS's.")
endif ()

# 设置编译项
set(CMAKE_CXX_FLAGS "-std=c++11 -O2 -g")

set(LIB_NAME netplus)
aux_source_directory(../../3rd/http_parser PROGRAM_SOURCE)
aux_source_directory(../../3rd/udns/0.4 PROGRAM_SOURCE)
aux_source_directory(../../src/ PROGRAM_SOURCE)
aux_source_directory(../../src/handler PROGRAM_SOURCE)
aux_source_directory(../../src/http PROGRAM_SOURCE)
aux_source_directory(../../src/logger PROGRAM_SOURCE)
aux_source_directory(../../src/os PROGRAM_SOURCE)
aux_source_directory(../../src/task PROGRAM_SOURCE)
aux_source_directory(../../src/thread_impl PROGRAM_SOURCE)
aux_source_directory(../../src/traffic PROGRAM_SOURCE)


add_library(${LIB_NAME} ${PROGRAM_SOURCE})
TARGET_LINK_LIBRARIES(netplus dl)
TARGET_LINK_LIBRARIES(netplus pthread)


add_executable(helloworld ../../test/hello_world/hello_world.cpp)
TARGET_LINK_LIBRARIES(helloworld netplus)
TARGET_LINK_LIBRARIES(helloworld dl)
TARGET_LINK_LIBRARIES(helloworld pthread)

