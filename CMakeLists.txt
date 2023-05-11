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
  add_compile_options(-Wall -Wextra -Werror)
message(STATUS "Native compiling - Building Tests")
set(SRC_DIR ${CMAKE_SOURCE_DIR})
set(TEST_DIR ${CMAKE_SOURCE_DIR}/test)

# Setup testing
include(CTest)
add_subdirectory("third_party/googletest")
add_subdirectory("test")

# if we are a subproject, export variables with the source files
else()

set(BM_SERIAL_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(BM_SERIAL_FILES
    ${BM_SERIAL_DIR}/bm_serial.c
    ${BM_SERIAL_DIR}/crc16.c

    PARENT_SCOPE)

set(BM_SERIAL_INCLUDES
    ${BM_SERIAL_DIR}
    PARENT_SCOPE
    )

add_library(bm_serial EXCLUDE_FROM_ALL ${BM_SERIAL_FILES})
# target_compile_options(bm_serial PRIVATE ${BM_SERIAL_COMPILER_FLAGS})
# target_compile_definitions(bm_serial PRIVATE ${BM_SERIAL_DEFINITIONS})
target_include_directories(bm_serial PRIVATE ${BM_SERIAL_INCLUDES} )

endif()