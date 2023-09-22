#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  BM_SERIAL_DEBUG = 0x00,
  BM_SERIAL_ACK = 0x01,

  BM_SERIAL_PUB = 0x02,
  BM_SERIAL_SUB = 0x03,
  BM_SERIAL_UNSUB = 0x04,
  BM_SERIAL_LOG = 0x05,
  BM_SERIAL_NET_MSG = 0x06,
  BM_SERIAL_RTC_SET = 0x07,
  BM_SERIAL_SELF_TEST = 0x08,
  BM_SERIAL_NETWORK_INFO = 0x09,
  BM_SERIAL_REBOOT_INFO = 0x0A,

  BM_SERIAL_DFU_START = 0x30,
  BM_SERIAL_DFU_CHUNK = 0x31,
  BM_SERIAL_DFU_RESULT = 0x32,

  BM_SERIAL_CFG_GET = 0x40,
  BM_SERIAL_CFG_SET = 0x41,
  BM_SERIAL_CFG_VALUE = 0x42,
  BM_SERIAL_CFG_COMMIT = 0x43,
  BM_SERIAL_CFG_STATUS_REQ = 0x44,
  BM_SERIAL_CFG_STATUS_RESP = 0x45,
  BM_SERIAL_CFG_DEL_REQ = 0x46,
  BM_SERIAL_CFG_DEL_RESP = 0x47,

  BM_SERIAL_DEVICE_INFO_REQ = 0x50,
  BM_SERIAL_DEVICE_INFO_REPLY = 0x51,
  BM_SERIAL_RESOURCE_REQ = 0x52,
  BM_SERIAL_RESOURCE_REPLY = 0x53,

} bm_serial_message_t;

typedef struct {
  uint8_t type;
  uint8_t flags;
  uint16_t crc16;
  uint8_t payload[0];
} __attribute__ ((packed)) bm_serial_packet_t;

typedef struct {
  uint64_t node_id;
  uint8_t type;
  uint8_t version;
  uint16_t topic_len;
  uint8_t topic[0];
  // message goes after topic
  // len not included since we get the total len from COBS
} __attribute__ ((packed)) bm_serial_pub_header_t;

typedef struct {
  uint16_t topic_len;
  uint8_t topic[0];
} __attribute__ ((packed)) bm_serial_sub_unsub_header_t;

typedef struct {
  uint64_t node_id;

  // Will use later to signify if this should go out cellular/satellite or both
  uint8_t flags;
  uint8_t data[0];
} __attribute__ ((packed)) bm_serial_net_msg_header_t;

typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint32_t us;
} __attribute__ ((packed)) bm_serial_time_t;

typedef struct {
  // Can be used to determine time source and other things
  uint32_t flags;

  bm_serial_time_t time;
} __attribute__ ((packed)) bm_serial_rtc_t;

typedef struct {
  // Node id of unit reporting test (leave blank for test request)
  uint64_t node_id;
  // Flags for self test result
  uint32_t result;
} __attribute__ ((packed)) bm_serial_self_test_t;

// DFU BELOW HERE.
typedef struct {
  // Node id of unit for update
  uint64_t node_id;
  // size of image to update
  uint32_t image_size;
  // size of chunks to send
  uint16_t chunk_size;
  // crc16
  uint16_t crc16;
  // major version
  uint8_t major_ver;
  // minor version
  uint8_t minor_ver;
  // filter for update
  uint32_t filter_key;
  // git hash
  uint32_t gitSHA;
} __attribute__ ((packed)) bm_serial_dfu_start_t;

#define DFU_CHUNK_NAK_BITFLAG (1<<31)
typedef struct {
  // offset from image start
  uint32_t offset;
  // data length
  uint32_t length;
  // data packet
  uint8_t data[0];
} __attribute__ ((packed)) bm_serial_dfu_chunk_t;

typedef struct {
  // Node id of dfu unit
  uint64_t node_id;
  // success of dfu
  bool success;
  // Errors for dfu result
  uint32_t dfu_status;
} __attribute__ ((packed)) bm_serial_dfu_finish_t;

typedef struct {
  // Node id
  uint64_t node_id;
  // Reboot Reason
  uint32_t reboot_reason;
  // git hash
  uint32_t gitSHA;
  // reboot count
  uint32_t reboot_count;
} __attribute__ ((packed)) bm_serial_reboot_info_t;

typedef struct {
  // Node ID of the target node for which the request is being made. (Zeroed = all nodes)
  uint64_t target_node_id;
} __attribute__((packed)) bm_serial_device_info_request_t;

typedef struct {
  // Node ID of the responding node
  uint64_t node_id;

  // Vendor ID of the hardware module implementing the BM Node functions
  uint16_t vendor_id;

  // Product ID for the hardware module implementing the BM Node functions
  uint16_t product_id;

  // Factory-flashed unique serial number
  uint8_t serial_num[16];

  // Last 4 bytes of git SHA
  uint32_t git_sha;

  // Major Version
  uint8_t ver_major;

  // Minor Version
  uint8_t ver_minor;

  // Revision/Patch Version
  uint8_t ver_rev;

  // Version of the product hardware (0 for don't care)
  uint8_t ver_hw;
} __attribute__((packed)) bm_serial_device_info_t;

typedef struct {
  bm_serial_device_info_t info;

  // Length of the full version string
  uint8_t ver_str_len;

  // Length of device name
  uint8_t dev_name_len;

  // ver_str is immediately followed by dev_name_len
  char strings[0];
} __attribute__((packed)) bm_serial_device_info_reply_t;

typedef struct {
  // Node ID of the target node for which the request is being made. (Zeroed = all nodes)
  uint64_t target_node_id;
} __attribute__((packed)) bm_serial_resource_table_request_t;

typedef struct {
  // Length of resource name
  uint16_t resource_len;
  // Name of resource
  char resource[0];
} __attribute__((packed)) bm_serial_resource_t;

typedef struct {
  // Node ID of the responding node
  uint64_t node_id;

  // Number of published topics
  uint16_t num_pubs;

  // Number of subscribed topics
  uint16_t num_subs;

  // List of structures containing information for all resource interests known about in this node.
  // List of bcmp_resource_t structured as num_pub published resources, then num_sub, subscribed resources.
  uint8_t resource_list[0];
} __attribute__((packed)) bm_serial_resource_table_reply_t;