cmake_minimum_required(VERSION 3.18)
project(bm_serial
	LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)


# If we're not a subproject, build the tests
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  add_compile_options(
    -Wall
    -Werror
    -O0
    -g
    )
message(STATUS "Native compiling - Building Tests")
set(SRC_DIR ${CMAKE_SOURCE_DIR})
set(TEST_DIR ${CMAKE_SOURCE_DIR}/test)

include(FetchContent)
FetchContent_Declare(
  bmcore
  GIT_REPOSITORY https://github.com/bristlemouth/bm_core.git
  GIT_TAG        main
  GIT_SUBMODULES "third_party"
)
FetchContent_GetProperties(bmcore)
if(NOT bmcore_POPULATED)
    FetchContent_Populate(bmcore)
endif()
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Setup testing
include(CTest)
add_subdirectory("test")

else()
#
# if we are a subproject, export as a library
# Use target_link_libraries(${EXECUTABLE} bm_serial) in parent CMakeLists.txt to use
# Set target compiler flags with the BM_SERIAL_COMPILER_FLAGS variable
#
set(BM_SERIAL_DIR ${CMAKE_CURRENT_LIST_DIR})
message(status "BM SERIAL DIR ${BM_SERIAL_DIR}")

set(BM_SERIAL_FILES
    ${BM_SERIAL_DIR}/bm_serial.c
    ${BM_SERIAL_DIR}/bm_serial_crc16.c
)

set(BM_SERIAL_INCLUDES
    ${BM_SERIAL_DIR}
    ${BM_COMMON_MESSAGES_INCLUDES}
    )

add_library(bm_serial EXCLUDE_FROM_ALL ${BM_SERIAL_FILES})
target_compile_options(bm_serial PRIVATE ${BM_SERIAL_COMPILER_FLAGS})
target_compile_definitions(bm_serial PRIVATE ${BM_SERIAL_DEFINITIONS})
target_include_directories(bm_serial PRIVATE ${BM_SERIAL_INCLUDES})
target_link_libraries(bm_serial PUBLIC bmmessages)

set(BM_SERIAL_INCLUDES ${BM_SERIAL_INCLUDES} PARENT_SCOPE)

endif()
