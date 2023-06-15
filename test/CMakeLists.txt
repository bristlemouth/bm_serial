#
# NCP tests
#
add_executable(bm_serial_tests)
target_include_directories(bm_serial_tests
    PRIVATE
    ${SRC_DIR}
    ${TEST_DIR}
    ${BM_COMMON_MESSAGES_INCLUDES}
)

target_sources(bm_serial_tests
    PRIVATE
    # File we're testing
    ${SRC_DIR}/bm_serial.c

    # Supporting files
    ${SRC_DIR}/bm_serial_crc16.c

    # Stubs

    # Unit test wrapper for test
    bm_serial_ut.cpp
)

target_link_libraries(bm_serial_tests gtest gmock gtest_main)

add_test(
  NAME
    bm_serial_tests
  COMMAND
    bm_serial_tests
  )
