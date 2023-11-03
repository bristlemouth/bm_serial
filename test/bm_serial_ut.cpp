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
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(fake_sub_called);

  fake_unsub_called = false;
  EXPECT_EQ(bm_serial_unsub("fake_unsub_topic", sizeof("fake_unsub_topic")), BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len),BM_SERIAL_OK);
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
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(fake_rtc_called);
}

static bool fake_dfu_start_called;
bool dfu_start_fn(bm_serial_dfu_start_t *start) {
  (void)start;

  fake_dfu_start_called = true;
  return true;
}

static bool fake_dfu_chunk_called;
bool dfu_chunk_fun(uint32_t offset, size_t length, uint8_t * data) {
  (void)offset;
  (void)length;
  (void)data;

  fake_dfu_chunk_called = true;
  return true;
}

static bool fake_dfu_finish_called;
bool dfu_finish_fn(uint64_t node_id, bool success, uint32_t err) {
  (void)node_id;
  (void) success;
  (void) err;

  fake_dfu_finish_called = true;
  return true;
}

TEST_F(NCPTest, DFUTest) {
  _callbacks.tx_fn = fake_tx_fn;
  _callbacks.dfu_start_fn = dfu_start_fn;
  _callbacks.dfu_chunk_fn = dfu_chunk_fun;
  _callbacks.dfu_end_fn = dfu_finish_fn;
  bm_serial_set_callbacks(&_callbacks);

  fake_dfu_start_called = false;
  bm_serial_dfu_start_t start = {
   .node_id = 0xdeaddeaddeaddead,
   .image_size = 2048,
   .chunk_size = 512,
   .crc16 = 0xbeef,
   .major_ver = 2,
   .minor_ver = 1,
   .filter_key = 0,
   .gitSHA = 0x12345678,
  };
  EXPECT_EQ(bm_serial_dfu_send_start(&start), BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(fake_dfu_start_called);

  fake_dfu_chunk_called = false;
  uint8_t buf = 1;
  EXPECT_EQ(bm_serial_dfu_send_chunk(0, sizeof(buf), &buf), BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(fake_dfu_chunk_called);

  fake_dfu_chunk_called = false;
  EXPECT_EQ(bm_serial_dfu_send_chunk(0, 0, NULL), BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(fake_dfu_chunk_called);

  fake_dfu_finish_called = false;
  EXPECT_EQ(bm_serial_dfu_send_finish(0xdeadbaaddaadbead, 1, 0), BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(fake_dfu_finish_called);
}

static bool fake_cfg_get_fn_called;
static bool fake_cfg_get_fn(uint64_t node_id, bm_common_config_partition_e partition, size_t key_len, const char* key) {
  (void) node_id;
  (void) partition;
  (void) key_len;
  (void) key;
  fake_cfg_get_fn_called = true;
  return true;
}
static bool fake_cfg_set_fn_called;
static bool fake_cfg_set_fn(uint64_t node_id, bm_common_config_partition_e partition,
  size_t key_len, const char* key, size_t value_size, void * val) {
  (void) node_id;
  (void) partition;
  (void) key_len;
  (void) key;
  (void) value_size;
  (void) val;
  fake_cfg_set_fn_called = true;
  return true;

}
static bool fake_cfg_value_fn_called;
static bool fake_cfg_value_fn(uint64_t node_id, bm_common_config_partition_e partition, uint32_t data_length, void* data) {
  (void) node_id;
  (void) partition;
  (void) data_length;
  (void) data;
  fake_cfg_value_fn_called = true;
  return true;

}
static bool fake_cfg_commit_fn_called;
static bool fake_cfg_commit_fn(uint64_t node_id, bm_common_config_partition_e partition) {
  (void) node_id;
  (void) partition;
  fake_cfg_commit_fn_called = true;
  return true;

}
static bool fake_cfg_status_request_fn_called;
static bool fake_cfg_status_request_fn(uint64_t node_id, bm_common_config_partition_e partition) {
  (void) node_id;
  (void) partition;
  fake_cfg_status_request_fn_called = true;
  return true;

}
static bool fake_cfg_status_response_fn_called;
static bool fake_cfg_status_response_fn(uint64_t node_id, bm_common_config_partition_e partition, bool commited, uint8_t num_keys, void* keys) {
  (void) node_id;
  (void) partition;
  (void) commited;
  (void) num_keys;
  (void) keys;
  fake_cfg_status_response_fn_called = true;
  return true;
}
static bool fake_cfg_key_del_request_fn_called;
static bool fake_cfg_key_del_request_fn(uint64_t node_id, bm_common_config_partition_e partition, size_t key_len, const char * key) {
  (void) node_id;
  (void) partition;
  (void) key_len;
  (void) key;
  fake_cfg_key_del_request_fn_called = true;
  return true;
}
static bool fake_cfg_key_del_response_fn_called;
static bool fake_cfg_key_del_response_fn(uint64_t node_id, bm_common_config_partition_e partition, size_t key_len, const char * key, bool success) {
  (void) node_id;
  (void) partition;
  (void) key_len;
  (void) key;
  (void) success;
  fake_cfg_key_del_response_fn_called = true;
  return true;
}

TEST_F(NCPTest, ConfigTest) {
  _callbacks.tx_fn = fake_tx_fn;
  _callbacks.cfg_get_fn = fake_cfg_get_fn;
  _callbacks.cfg_set_fn = fake_cfg_set_fn;
  _callbacks.cfg_value_fn = fake_cfg_value_fn;
  _callbacks.cfg_commit_fn = fake_cfg_commit_fn;
  _callbacks.cfg_status_request_fn = fake_cfg_status_request_fn;
  _callbacks.cfg_status_response_fn = fake_cfg_status_response_fn;
  _callbacks.cfg_key_del_request_fn = fake_cfg_key_del_request_fn;
  _callbacks.cfg_key_del_response_fn = fake_cfg_key_del_response_fn;
  bm_serial_set_callbacks(&_callbacks);
  fake_cfg_get_fn_called  = false;
  fake_cfg_set_fn_called = false;
  fake_cfg_value_fn_called = false;
  fake_cfg_commit_fn_called = false;
  fake_cfg_status_response_fn_called = false;
  fake_cfg_status_request_fn_called = false;
  fake_cfg_key_del_request_fn_called = false;
  fake_cfg_key_del_response_fn_called = false;

  EXPECT_EQ(bm_serial_cfg_get(0xdeadbadc0ffeedad, BM_COMMON_CFG_PARTITION_SYSTEM, sizeof("foo"), "foo"),BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(fake_cfg_get_fn_called);

  uint32_t test = 42;
  EXPECT_EQ(bm_serial_cfg_set(0xdeadbadc0ffeedad, BM_COMMON_CFG_PARTITION_SYSTEM, sizeof("foo"), "foo", sizeof(uint32_t), &test),BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(fake_cfg_set_fn_called);

  EXPECT_EQ(bm_serial_cfg_commit(0xdeadbadc0ffeedad, BM_COMMON_CFG_PARTITION_SYSTEM),BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(fake_cfg_commit_fn_called);

  uint32_t result = 10;
  EXPECT_EQ(bm_serial_cfg_value(0xdeadbadc0ffeedad, BM_COMMON_CFG_PARTITION_SYSTEM, sizeof(result), &result),BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(fake_cfg_value_fn_called);

  EXPECT_EQ(bm_serial_cfg_status_request(0xdeadbadc0ffeedad, BM_COMMON_CFG_PARTITION_SYSTEM),BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(fake_cfg_status_request_fn_called);

  const char hello[] = "hello_world";
  uint32_t len = sizeof(bm_common_config_status_key_data_t) + sizeof(hello);
  uint8_t * keybuffer = (uint8_t*) malloc(len);
  bm_common_config_status_key_data_t *key = (bm_common_config_status_key_data_t *)keybuffer;
  key->key_length = sizeof(hello);
  memcpy(key->key, hello, sizeof(hello));

  EXPECT_EQ(bm_serial_cfg_status_response(0xdeadbadc0ffeedad, BM_COMMON_CFG_PARTITION_SYSTEM, true, 1, keybuffer),BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(fake_cfg_status_response_fn_called);
  free(keybuffer);

  EXPECT_EQ(bm_serial_cfg_status_response(0xdeadbadc0ffeedad, BM_COMMON_CFG_PARTITION_SYSTEM, true, 0, NULL),BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);

  EXPECT_EQ(bm_serial_cfg_delete_request(0xdeadbadc0ffeedad, BM_COMMON_CFG_PARTITION_SYSTEM, sizeof("foo"), "foo"),BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(fake_cfg_key_del_request_fn_called);

  EXPECT_EQ(bm_serial_cfg_delete_response(0xdeadbadc0ffeedad, BM_COMMON_CFG_PARTITION_SYSTEM, sizeof("foo"), "foo", true),BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(fake_cfg_key_del_response_fn_called);

}

static bool fake_network_info_fn_called;
static bool fake_network_info_fn(bm_common_network_info_t* network_info) {
  (void) network_info;
  fake_network_info_fn_called = true;
  return true;
}

TEST_F(NCPTest, NetworkInfoTest) {
  _callbacks.tx_fn = fake_tx_fn;
  _callbacks.network_info_fn = fake_network_info_fn;
  bm_serial_set_callbacks(&_callbacks);
  fake_network_info_fn_called = false;

  bm_common_config_crc_t config_crc = {
    .partition = BM_COMMON_CFG_PARTITION_SYSTEM,
    .crc32 = 1234,
  };

  bm_common_fw_version_t fw_info = {
    .major = 1,
    .minor = 2,
    .revision = 3,
    .gitSHA = 1234,
  };

  uint64_t node_list[] = {12345678};
  uint16_t num_nodes = 1;

  // {"Hello":"World"}
  uint8_t cbor_map[] = {0xA1,0x65,0x48,0x65,0x6C,0x6C,0x6F,0x65,0x57,0x6F,0x72,0x6C,0x64};

  EXPECT_EQ(bm_serial_send_network_info((uint32_t)1234, &config_crc, &fw_info, num_nodes, node_list, sizeof(cbor_map), cbor_map), BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(fake_network_info_fn_called);
}

static bool reboot_info_fn_called;
static bool fake_reboot_info_fn(uint64_t node_id, uint32_t reboot_reason, uint32_t gitSHA, uint32_t reboot_count) {
  (void) node_id;
  (void) reboot_reason;
  (void) gitSHA;
  (void) reboot_count;
  reboot_info_fn_called = true;
  return true;
}

TEST_F(NCPTest, RebootInfoTest) {
  _callbacks.tx_fn = fake_tx_fn;
  _callbacks.reboot_info_fn = fake_reboot_info_fn;
  bm_serial_set_callbacks(&_callbacks);
  reboot_info_fn_called = false;

  EXPECT_EQ(bm_serial_send_reboot_info(0xdadb0dc0ffee, 3, 0xbaddd00d, 1), BM_SERIAL_OK);
  EXPECT_EQ(bm_serial_process_packet((bm_serial_packet_t *)serial_tx_buff, serial_tx_buff_len), BM_SERIAL_OK);
  EXPECT_TRUE(reboot_info_fn_called);
}
