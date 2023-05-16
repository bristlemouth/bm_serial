#include <string.h>
#include "bm_serial.h"
#include "crc.h"

#define MAX_TOPIC_LEN 64

#define SERIAL_BUFF_LEN 2048
static uint8_t bm_serial_tx_buff[SERIAL_BUFF_LEN];

static bm_serial_callbacks_t _callbacks;

/*!
  Set all the callback functions for bm_serial

  \param[in] *callbacks pointer to callback structure. This file keeps it's own copy
  \return none
*/
void bm_serial_set_callbacks(bm_serial_callbacks_t *callbacks) {
  memcpy(&_callbacks, callbacks, sizeof(bm_serial_callbacks_t));
}

/*!
  Validate the topic and check that the transmit callback function is set,
  otherwise there's no point

  \param[in] *topic topic string
  \param[in] topic_len length of the topic
  \return BM_SERIAL_OK if topic is valid, nonzero otherwise
*/
static bm_serial_error_e _bm_serial_validate_topic_and_cb(const char *topic, uint16_t topic_len) {
   bm_serial_error_e rval = BM_SERIAL_OK;
   do {
    if(!topic) {
      rval = BM_SERIAL_NULL_BUFF;
      break;
    }

    // Topic too long
    if(topic_len > MAX_TOPIC_LEN) {
      rval = BM_SERIAL_OVERFLOW;
      break;
    }

    // No transmit function :'(
    if(!_callbacks.tx_fn) {
      rval = BM_SERIAL_MISSING_CALLBACK;
      break;
    }

  } while(0);

  return rval;
}

/*!
  Get packet buffer with initialized header
  Using static buffer for now (NOT THREAD SAFE)
  If we ever want to allocate or use a message pool, this is where we'd do it

  \param type bm_serial message type
  \param flags optional flags
  \param buff_len size of required buffer
  \return pointer to buffer if allocated successfully, NULL otherwise
*/
static bm_serial_packet_t *_bm_serial_get_packet(bm_serial_message_t type, uint8_t flags, uint16_t buff_len) {

  if(buff_len <= SERIAL_BUFF_LEN) {
    bm_serial_packet_t *packet = (bm_serial_packet_t *)bm_serial_tx_buff;

    packet->type = type;
    packet->flags = flags;
    packet->crc16 = 0;

    return packet;
  } else {
    return NULL;
  }
}

/*!
  Send raw bm_serial data

  \param[in] type bm_serial message type
  \param[in] *payload message payload
  \param[in] len payload length
  \return BM_SERIAL_OK if topic is valid, nonzero otherwise
*/
bm_serial_error_e bm_serial_tx(bm_serial_message_t type, const uint8_t *payload, size_t len) {
  bm_serial_error_e rval = BM_SERIAL_OK;

  do {
    if(!payload) {
      rval = BM_SERIAL_NULL_BUFF;
      break;
    }

    // Lets make sure that what we are trying to send will fit in the payload
    if((uint32_t)len + sizeof(bm_serial_packet_t) >= (SERIAL_BUFF_LEN - 1)) {
      rval = BM_SERIAL_OVERFLOW;
      break;
    }

    if(!_callbacks.tx_fn) {
      rval = BM_SERIAL_MISSING_CALLBACK;
      break;
    }

    uint16_t message_len = sizeof(bm_serial_packet_t) + len;
    bm_serial_packet_t *packet = _bm_serial_get_packet(type, 0, message_len);
    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }
    memcpy(packet->payload, payload, len);

    packet->crc16 = crc16_ccitt(0, (uint8_t *)packet, message_len);

    uint16_t bm_serial_message_len = sizeof(bm_serial_packet_t) + len;
    if(!_callbacks.tx_fn((uint8_t *)packet, bm_serial_message_len)) {
      rval = BM_SERIAL_TX_ERR;

      break;
    }

  } while(0);

  return rval;
}

/*!
  bm_serial publish data to topic

  \param node_id node id of publisher
  \param *topic topic to publish on
  \param topic_len length of topic
  \param *data data to publish
  \param data_len length of data
  \return BM_SERIAL_OK if ok, nonzero otherwise
*/
bm_serial_error_e bm_serial_pub(uint64_t node_id, const char *topic, uint16_t topic_len, const uint8_t *data, uint16_t data_len) {
  bm_serial_error_e rval = BM_SERIAL_OK;

  do {
    rval = _bm_serial_validate_topic_and_cb(topic, topic_len);
    if (rval){
      break;
    }

    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_serial_pub_header_t) + topic_len + data_len;
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_PUB, 0, message_len);
    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }

    bm_serial_pub_header_t *pub_header = (bm_serial_pub_header_t *)packet->payload;
    pub_header->node_id = node_id;
    pub_header->topic_len = topic_len;
    memcpy(pub_header->topic, topic, topic_len);

    // Copy data after payload (if any)
    if(data && data_len) {
      memcpy(&pub_header->topic[topic_len], data, data_len);
    }

    packet->crc16 = crc16_ccitt(0, (uint8_t *)packet, message_len);

    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }

    // TODO - do we wait for an ack?

  } while(0);

  return rval;
}

/*!
  bm_serial subscribe/unsubscribe from topic

  \param *topic topic to subscribe to
  \param topic_len lenth of topic
  \param sub subscribe/unsubscribe
  \return BM_SERIAL_OK on success, nonzero otherwise
*/
static bm_serial_error_e _bm_serial_sub_unsub(const char *topic, uint16_t topic_len, bool sub) {
  bm_serial_error_e rval = BM_SERIAL_OK;

  do {
    rval = _bm_serial_validate_topic_and_cb(topic, topic_len);
    if (rval){
      break;
    }

    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_serial_pub_header_t) + topic_len;

    bm_serial_packet_t *packet = NULL;
    if(sub) {
      packet = _bm_serial_get_packet(BM_SERIAL_SUB, 0, message_len);
    } else {
      packet = _bm_serial_get_packet(BM_SERIAL_UNSUB, 0, message_len);
    }

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }

    bm_serial_sub_unsub_header_t *sub_header = (bm_serial_sub_unsub_header_t *)packet->payload;
    sub_header->topic_len = topic_len;
    memcpy(sub_header->topic, topic, topic_len);

    packet->crc16 = crc16_ccitt(0, (uint8_t *)packet, message_len);

    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }

  } while(0);

  return rval;
}

/*!
  bm_serial subscribe to topic

  \param *topic topic to subscribe to
  \param topic_len lenth of topic
  \return BM_SERIAL_OK on success, nonzero otherwise
*/
bm_serial_error_e bm_serial_sub(const char *topic, uint16_t topic_len) {
  // TODO - do we wait for an ack?
  return _bm_serial_sub_unsub(topic, topic_len, true);
}

/*!
  bm_serial unsubscribe from topic

  \param *topic topic to subscribe to
  \param topic_len lenth of topic
  \return BM_SERIAL_OK on success, nonzero otherwise
*/
bm_serial_error_e bm_serial_unsub(const char *topic, uint16_t topic_len) {
  // TODO - do we want for an ack?
 return _bm_serial_sub_unsub(topic, topic_len, false);
}

/*!
  Update RTC on target device

  \param[in] *time time to set the clock to
  \return BM_SERIAL_OK if sent, nonzero otherwise
*/
bm_serial_error_e bm_serial_set_rtc(bm_serial_time_t *time) {
  bm_serial_error_e rval = BM_SERIAL_OK;

  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_serial_rtc_t);

    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_RTC_SET, 0, message_len);


    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }

    bm_serial_rtc_t *rtc_header = (bm_serial_rtc_t *)packet->payload;
    memcpy(&rtc_header->time, time, sizeof(bm_serial_time_t));

    packet->crc16 = crc16_ccitt(0, (uint8_t *)packet, message_len);

    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }

  } while(0);

  return rval;
}

/*!
  Send out a self test request or response

  \param[in] node_id node id of device who ran self test (or 0 to request one)
  \param[in] result self test result
  \return BM_SERIAL_OK on successful send, nonzero otherwise
*/
bm_serial_error_e bm_serial_send_self_test(uint64_t node_id, uint32_t result) {
  bm_serial_error_e rval = BM_SERIAL_OK;

  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_serial_self_test_t);

    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_SELF_TEST, 0, message_len);


    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }

    bm_serial_self_test_t *self_test = (bm_serial_self_test_t *)packet->payload;
    self_test->node_id = node_id;
    self_test->result = result;

    packet->crc16 = crc16_ccitt(0, (uint8_t *)packet, message_len);

    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }

  } while(0);

  return rval;
}

bm_serial_error_e bm_serial_dfu_send_start(bm_serial_dfu_start_t *dfu_start) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_serial_dfu_start_t);
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_DFU_START, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }

    bm_serial_dfu_start_t *msg_start = (bm_serial_dfu_start_t *)packet->payload;
    memcpy(msg_start, dfu_start, sizeof(bm_serial_dfu_start_t));

    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }

  } while(0);
  return rval;
}

bm_serial_error_e bm_serial_dfu_send_chunk(uint32_t offset, size_t length, uint8_t * data) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {

    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_serial_dfu_chunk_t) + length;
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_DFU_CHUNK, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }

    bm_serial_dfu_chunk_t *dfu_chunk = (bm_serial_dfu_chunk_t *)packet->payload;
    dfu_chunk->offset = offset;
    dfu_chunk->length = length;
    memcpy(dfu_chunk->data, data, length);

    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }

  } while(0);
  return rval;
}

bm_serial_error_e bm_serial_dfu_send_finish(uint64_t node_id, bool success, uint32_t err) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_serial_dfu_finish_t);
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_DFU_RESULT, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }

    bm_serial_dfu_finish_t *dfu_finish = (bm_serial_dfu_finish_t *)packet->payload;
    dfu_finish->err = err;
    dfu_finish->node_id = node_id;
    dfu_finish->success = success;

    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }
  } while(0);
  return rval;
}

// Process bm_serial packet (not COBS anymore!)
bm_serial_error_e bm_serial_process_packet(bm_serial_packet_t *packet, size_t len) {
  bm_serial_error_e rval = BM_SERIAL_OK;

  // calc the crc16 and compare
  uint16_t crc16_pre = packet->crc16;
  packet->crc16 = 0;
  do {
    uint16_t crc16_post = crc16_ccitt(0, (uint8_t *)packet, len);

    if (crc16_post != crc16_pre) {
      rval = BM_SERIAL_CRC_ERR;
      break;
    }

    switch(packet->type){
      case BM_SERIAL_DEBUG: {
        if(_callbacks.debug_fn) {
          _callbacks.debug_fn(packet->payload, len - sizeof(bm_serial_packet_t));
        }
        break;
      }

      case BM_SERIAL_PUB: {
        if(!_callbacks.pub_fn) {
          break;
        }

        bm_serial_pub_header_t *pub_header = (bm_serial_pub_header_t *)packet->payload;

        // Protect against topic length being incorrect
        // (would result in overflow when subtracting from len to determine data len)
        uint32_t non_data_len = sizeof(bm_serial_packet_t) + sizeof(bm_serial_pub_header_t) + pub_header->topic_len;
        if(non_data_len > len) {
          rval = BM_SERIAL_INVALID_TOPIC_LEN;
          break;
        }

        uint32_t data_len = len - non_data_len;
        _callbacks.pub_fn((const char *)pub_header->topic,
                          pub_header->topic_len,
                          pub_header->node_id,
                          &pub_header->topic[pub_header->topic_len],
                          data_len);

        break;
      }

      case BM_SERIAL_SUB: {
        if(!_callbacks.sub_fn) {
          break;
        }

        bm_serial_sub_unsub_header_t *sub_header = (bm_serial_sub_unsub_header_t *)packet->payload;
        _callbacks.sub_fn((const char *)sub_header->topic, sub_header->topic_len);

        break;
      }

      case BM_SERIAL_UNSUB: {
        if(!_callbacks.unsub_fn) {
          break;
        }

        bm_serial_sub_unsub_header_t *unsub_header = (bm_serial_sub_unsub_header_t *)packet->payload;
        _callbacks.unsub_fn((const char *)unsub_header->topic, unsub_header->topic_len);

        break;
      }

      case BM_SERIAL_LOG: {
        if(_callbacks.log_fn) {
          // TODO - decode and use actual topic
          _callbacks.log_fn(0, packet->payload, len - sizeof(bm_serial_packet_t));
        }
        break;
      }

      case BM_SERIAL_NET_MSG: {
        if(_callbacks.net_msg_fn) {
          uint32_t non_data_len = sizeof(bm_serial_packet_t) + sizeof(bm_serial_net_msg_header_t);
          if(non_data_len > len) {
            rval = BM_SERIAL_INVALID_MSG_LEN;
            break;
          }
          bm_serial_net_msg_header_t *net_msg = (bm_serial_net_msg_header_t *)packet->payload;

          uint32_t data_len = len - non_data_len;
          _callbacks.net_msg_fn(net_msg->node_id,
                                net_msg->data,
                                data_len);
        }
        break;
      }

      case BM_SERIAL_RTC_SET: {
        if(_callbacks.rtc_set_fn) {
          bm_serial_rtc_t *rtc_msg = (bm_serial_rtc_t *)packet->payload;
          _callbacks.rtc_set_fn(&rtc_msg->time);
        }
        break;
      }

      case BM_SERIAL_SELF_TEST: {
        if(_callbacks.self_test_fn) {
          bm_serial_self_test_t *self_test = (bm_serial_self_test_t *)packet->payload;
          _callbacks.self_test_fn(self_test->node_id, self_test->result);
        }
        break;
      }

      case BM_SERIAL_DFU_START: {
        if(_callbacks.dfu_start_fn){
          bm_serial_dfu_start_t *dfu_start = ( bm_serial_dfu_start_t *)packet->payload;
          _callbacks.dfu_start_fn(dfu_start);
        }
        break;
      }

      case BM_SERIAL_DFU_CHUNK: {
        if(_callbacks.dfu_chunk_fn) {
          bm_serial_dfu_chunk_t* dfu_chunk = (bm_serial_dfu_chunk_t*)packet->payload;
          _callbacks.dfu_chunk_fn(dfu_chunk->offset, dfu_chunk->length, dfu_chunk->data);
        }
        break;
      }

      case BM_SERIAL_DFU_RESULT: {
        if(_callbacks.dfu_end_fn) {
          bm_serial_dfu_finish_t* dfu_end= (bm_serial_dfu_finish_t*)packet->payload;
          _callbacks.dfu_end_fn(dfu_end->node_id, dfu_end->success, dfu_end->err);
        }
        break;
      }

      default: {
        rval = BM_SERIAL_UNSUPPORTED_MSG;
        break;
      }
    }

    rval = true;
  } while(0);

  return rval;
}
