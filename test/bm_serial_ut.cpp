#include "gtest/gtest.h"
#include "bm_serial.h"

#include <string.h>

static bm_serial_callbacks_t _callbacks;

static uint8_t serial_tx_buff[1024];
static uint32_t serial_tx_buff_len;

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

    // Clear local callbacks
    memset(&_callbacks, 0, sizeof(_callbacks));

    // Clear callbacks in module
    bm_serial_set_callbacks(&_callbacks);

    // Clear serial tx buffer
    memset(serial_tx_buff, 0x00, sizeof(serial_tx_buff));
    serial_tx_buff_len = 0;
  }

  void TearDown() override {
     // Code here will be called immediately after each test (right
     // before the destructor).
  }

  // Objects declared here can be used by all tests in the test suite for Foo.
};

// Fake serial_tx function just copies data into buffer so we can process it
bool fake_tx_fn(const uint8_t *buff, size_t len) {
  if(len >= sizeof(serial_tx_buff)) {
    serial_tx_buff_len = 0;
    return false;
  }

  // Copy serial data into buffer
  memcpy(serial_tx_buff, buff, len);
  serial_tx_buff_len = len;

  return true;
}

TEST_F(NCPTest, ErrorTests) {
  // Try to transmit null buffer
  EXPECT_EQ(bm_serial_tx(BM_SERIAL_DEBUG, NULL, 0), BM_SERIAL_NULL_BUFF);

  uint8_t buff[128];
  // Try to send a message that's too big
  EXPECT_EQ(bm_serial_tx(BM_SERIAL_DEBUG, buff, 1024*10), BM_SERIAL_OVERFLOW);

  // Try to send message without a tx callback
  EXPECT_EQ(bm_serial_tx(BM_SERIAL_DEBUG, buff, sizeof(buff)), BM_SERIAL_MISSING_CALLBACK);
}

// static bool fake_pub_fn(const char *topic, uint16_t topic_len, uint64_t node_id, const uint8_t *payload, size_t len) {

//   return true;
// }

static bool fake_sub_called;
static bool fake_sub_fn(const char *topic, uint16_t topic_len) {
  EXPECT_STREQ(topic, "fake_sub_topic");
  EXPECT_EQ(topic_len, sizeof("fake_sub_topic"));

  fake_sub_called = true;
  return true;
}

static bool fake_unsub_called;
static bool fake_unsub_fn(const char *topic, uint16_t topic_len) {
  EXPECT_STREQ(topic, "fake_unsub_topic");
  EXPECT_EQ(topic_len, sizeof("fake_unsub_topic"));
  fake_unsub_called = true;
  return true;
}

TEST_F(NCPTest, SubUnsubTests) {
  _callbacks.tx_fn = fake_tx_fn;
  _callbacks.sub_fn = fake_sub_fn;
  _callbacks.unsub_fn = fake_unsub_fn;
  bm_serial_set_callbacks(&_callbacks);

  fake_sub_called = false;
  EXPECT_EQ(bm_serial_sub("fake_sub_topic", sizeof("fake_sub_topic")), BM_SERIAL_OK);
  EXPECT_TRUE(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len));
  EXPECT_TRUE(fake_sub_called);

  fake_unsub_called = false;
  EXPECT_EQ(bm_serial_unsub("fake_unsub_topic", sizeof("fake_unsub_topic")), BM_SERIAL_OK);
  EXPECT_TRUE(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len));
  EXPECT_TRUE(fake_unsub_called);
}

static bool fake_rtc_called;
bm_serial_time_t rtc_time;
bool rtc_set_fn(bm_serial_time_t *time) {
  (void)time;

  fake_rtc_called = true;
  return true;
}

TEST_F(NCPTest, RTCTest) {
  _callbacks.tx_fn = fake_tx_fn;
  _callbacks.rtc_set_fn = rtc_set_fn;
  bm_serial_set_callbacks(&_callbacks);

  fake_rtc_called = false;
  rtc_time.year = 2023;
  rtc_time.month = 5;
  rtc_time.day = 12;
  rtc_time.hour = 9;
  rtc_time.minute = 15;
  rtc_time.second = 10;
  rtc_time.us = 123456;
  EXPECT_EQ(bm_serial_set_rtc(&rtc_time), BM_SERIAL_OK);
  EXPECT_TRUE(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len));
  EXPECT_TRUE(fake_rtc_called);
}

