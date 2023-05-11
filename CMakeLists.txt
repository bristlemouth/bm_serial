cmake_minimum_required(VERSION 3.18)
project(bm_serial
	LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

message(STATUS "Native compiling - Building Tests")
set(SRC_DIR ${CMAKE_SOURCE_DIR})
set(TEST_DIR ${CMAKE_SOURCE_DIR}/test)

# Setup testing
include(CTest)

add_subdirectory("third_party/googletest")

add_subdirectory("test")
