#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  BM_NCP_DEBUG = 0x00,
  BM_NCP_ACK = 0x01,

  BM_NCP_PUB = 0x02,
  BM_NCP_SUB = 0x03,
  BM_NCP_UNSUB = 0x04,
  BM_NCP_LOG = 0x05,
  BM_NCP_NET_MSG = 0x06,
} bm_serial_message_t;

typedef struct {
  uint8_t type;
  uint8_t flags;
  uint16_t crc16;
  uint8_t payload[0];
} __attribute__ ((packed)) bm_serial_packet_t;

typedef struct {
  uint64_t node_id;
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
} __attribute__ ((packed)) ncp_net_msg_header_t;
