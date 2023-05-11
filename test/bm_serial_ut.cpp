#include "gtest/gtest.h"
#include "bm_serial.h"

#include <string.h>

static bm_serial_callbacks_t _callbacks;

// The fixture for testing class Foo.
class NCPTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  NCPTest() {
     // You can do set-up work for each test here.
  }

  ~NCPTest() override {
     // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

  void SetUp() override {
     // Code here will be called immediately after the constructor (right
     // before each test).

    // Clear callbacks
    memset(&_callbacks, 0, sizeof(_callbacks));
  }

  void TearDown() override {
     // Code here will be called immediately after each test (right
     // before the destructor).
  }

  // Objects declared here can be used by all tests in the test suite for Foo.
};

TEST_F(NCPTest, BasicTest)
{
  // Setting empty callbacks
  bm_serial_set_callbacks(&_callbacks);

  // Try to transmit null buffer
  EXPECT_EQ(bm_serial_tx(BM_NCP_DEBUG, NULL, 0), BM_NCP_NULL_BUFF);

  uint8_t buff[128];
  // Try to send a message that's too big
  EXPECT_EQ(bm_serial_tx(BM_NCP_DEBUG, buff, 1024*10), BM_NCP_OVERFLOW);

  // Try to send message without a tx callback
  EXPECT_EQ(bm_serial_tx(BM_NCP_DEBUG, buff, sizeof(buff)), BM_NCP_MISSING_CALLBACK);

  // Testing CI
  EXPECT_TRUE(false);

}


