#include <string.h>
#include "bm_serial.h"
#include "bm_serial_crc.h"

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

    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);

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
bm_serial_error_e bm_serial_pub(uint64_t node_id, const char *topic, uint16_t topic_len, const uint8_t *data, uint16_t data_len, uint8_t type, uint8_t version) {
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
    pub_header->type = type;
    pub_header->version = version;
    memcpy(pub_header->topic, topic, topic_len);

    // Copy data after payload (if any)
    if(data && data_len) {
      memcpy(&pub_header->topic[topic_len], data, data_len);
    }

    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);

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

    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);

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

    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);

    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }

  } while(0);

  return rval;
}

/*!
  Send out a bm_common_network_info_t

  \param[in] *config_crc
  \param[in] *fw_info
  \param[in] num_nodes
  \param[in] *node_id_list
  \return BM_SERIAL_OK if sent, nonzero otherwise
*/
bm_serial_error_e bm_serial_send_network_info(uint32_t network_crc32, bm_common_config_crc_t *config_crc, bm_common_fw_version_t *fw_info, uint16_t num_nodes, uint64_t* node_id_list) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {

    if (!config_crc || !fw_info || !node_id_list || num_nodes == 0) {
      rval = BM_SERIAL_MISC_ERR;
      break;
    }

    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_common_network_info_t) + (sizeof(uint64_t) * num_nodes);

    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_NETWORK_INFO, 0, message_len);

    if (!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }

    bm_common_network_info_t *network_info = (bm_common_network_info_t *)packet->payload;
    network_info->network_crc32 = network_crc32;
    memcpy(&network_info->config_crc, config_crc, sizeof(bm_common_config_crc_t));
    memcpy(&network_info->fw_info, fw_info, sizeof(bm_common_fw_version_t));
    network_info->node_list.num_nodes = num_nodes;
    memcpy(&network_info->node_list.list, node_id_list, sizeof(uint64_t)*num_nodes);

    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);

    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }

  } while (0);
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

    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);

    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }

  } while(0);

  return rval;
}

/*!
  Send out a reboot info message

  \param[in] node_id node id of device who ran self test (or 0 to request one)
  \param[in] reboot_reason reboot reason enum
  \param[in] gitSHA 32-bit gitSHA
  \param[in] reboot_count reboot count
  \return BM_SERIAL_OK on successful send, nonzero otherwise
*/
bm_serial_error_e bm_serial_send_reboot_info(uint64_t node_id, uint32_t reboot_reason, uint32_t gitSHA, uint32_t reboot_count) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_serial_reboot_info_t);

    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_REBOOT_INFO, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }
    bm_serial_reboot_info_t *reboot_info = (bm_serial_reboot_info_t*)packet->payload;
    reboot_info->node_id = node_id;
    reboot_info->reboot_reason = reboot_reason;
    reboot_info->gitSHA = gitSHA;
    reboot_info->reboot_count = reboot_count;
    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);
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
    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);

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
    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);

    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }

  } while(0);
  return rval;
}

bm_serial_error_e bm_serial_dfu_send_finish(uint64_t node_id, bool success, uint32_t status) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_serial_dfu_finish_t);
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_DFU_RESULT, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }

    bm_serial_dfu_finish_t *dfu_finish = (bm_serial_dfu_finish_t *)packet->payload;
    dfu_finish->dfu_status = status;
    dfu_finish->node_id = node_id;
    dfu_finish->success = success;
    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);

    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }
  } while(0);
  return rval;
}


bm_serial_error_e bm_serial_cfg_get(uint64_t node_id, bm_common_config_partition_e partition, size_t key_len, const char* key) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_common_config_get_t) + key_len;
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_CFG_GET, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }

    bm_common_config_get_t *cfg_get_msg = (bm_common_config_get_t *)packet->payload;
    cfg_get_msg->header.target_node_id = node_id;
    cfg_get_msg->header.source_node_id = 0; // UNUSED
    cfg_get_msg->partition = partition;
    cfg_get_msg->key_length = key_len;
    memcpy(cfg_get_msg->key, key, key_len);
    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);
    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }
  } while(0);
  return rval;
}

bm_serial_error_e bm_serial_cfg_set(uint64_t node_id, bm_common_config_partition_e partition,
  size_t key_len, const char* key, size_t value_size, void * val) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_common_config_set_t) + key_len + value_size;
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_CFG_SET, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }

    bm_common_config_set_t *cfg_set_msg = (bm_common_config_set_t *)packet->payload;
    cfg_set_msg->header.target_node_id = node_id;
    cfg_set_msg->header.source_node_id = 0; // UNUSED
    cfg_set_msg->partition = partition;
    cfg_set_msg->key_length = key_len;
    cfg_set_msg->data_length = value_size;
    memcpy(cfg_set_msg->keyAndData, key, key_len);
    memcpy(&cfg_set_msg->keyAndData[key_len], val, value_size);
    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);
    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }
  } while(0);
  return rval;
}

bm_serial_error_e bm_serial_cfg_value(uint64_t node_id, bm_common_config_partition_e partition, uint32_t data_length, void* data) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_common_config_value_t) + data_length;
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_CFG_VALUE, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }

    bm_common_config_value_t *cfg_value_msg = (bm_common_config_value_t *)packet->payload;
    cfg_value_msg->header.target_node_id = 0; // UNUSED
    cfg_value_msg->header.source_node_id = node_id;
    cfg_value_msg->partition = partition;
    cfg_value_msg->data_length = data_length;
    memcpy(cfg_value_msg->data, data, data_length);
    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);
    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }
  } while(0);
  return rval;
}

bm_serial_error_e bm_serial_cfg_commit(uint64_t node_id, bm_common_config_partition_e partition) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_common_config_commit_t);
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_CFG_COMMIT, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }
    bm_common_config_commit_t *cfg_commit_msg = (bm_common_config_commit_t *)packet->payload;
    cfg_commit_msg->header.target_node_id = node_id;
    cfg_commit_msg->header.source_node_id = 0; // UNUSED.
    cfg_commit_msg->partition = partition;
    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);
    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }
  } while(0);
  return rval;
}

bm_serial_error_e bm_serial_cfg_status_request(uint64_t node_id, bm_common_config_partition_e partition) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_common_config_status_request_t);
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_CFG_STATUS_REQ, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }
    bm_common_config_status_request_t* status_req_msg =  (bm_common_config_status_request_t *)packet->payload;
    status_req_msg->header.target_node_id = node_id;
    status_req_msg->header.source_node_id = 0; // UNUSED.
    status_req_msg->partition = partition;
    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);
    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }
  } while(0);
  return rval;
}

bm_serial_error_e bm_serial_cfg_status_response(uint64_t node_id, bm_common_config_partition_e partition, bool commited, uint8_t num_keys, void * keys) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_common_config_status_response_t);
    size_t key_data_len = 0;
    bm_common_config_status_key_data_t* cur_key = (bm_common_config_status_key_data_t*) keys;
    for(int i = 0; i < num_keys; i++) {
      key_data_len += sizeof(bm_common_config_status_key_data_t);
      key_data_len += cur_key->key_length;
      cur_key +=  sizeof(bm_common_config_status_key_data_t) + cur_key->key_length;
    }
    message_len += key_data_len;
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_CFG_STATUS_RESP, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }
    bm_common_config_status_response_t* status_resp_msg = (bm_common_config_status_response_t*)packet->payload;
    status_resp_msg->header.target_node_id = 0; // UNUSED
    status_resp_msg->header.source_node_id = node_id; // UNUSED.
    status_resp_msg->partition = partition;
    status_resp_msg->committed = commited;
    status_resp_msg->num_keys = num_keys;
    memcpy(status_resp_msg->keyData, keys, key_data_len);
    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);
    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }
  } while(0);
  return rval;
}

bm_serial_error_e bm_serial_cfg_delete_request(uint64_t node_id, bm_common_config_partition_e partition, size_t key_len, const char * key) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_common_config_delete_key_request_t) + key_len;
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_CFG_DEL_REQ, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }
    bm_common_config_delete_key_request_t* del_key_req = (bm_common_config_delete_key_request_t*) packet->payload;
    del_key_req->header.target_node_id = node_id;
    del_key_req->header.source_node_id = 0; // UNUSED.
    del_key_req->partition = partition;
    del_key_req->key_length = key_len;
    memcpy(del_key_req->key, key, key_len);
    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);
    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }
  } while(0);
  return rval;
}

bm_serial_error_e bm_serial_cfg_delete_response(uint64_t node_id, bm_common_config_partition_e partition, size_t key_len, const char * key, bool success) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_common_config_delete_key_response_t) + key_len;
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_CFG_DEL_RESP, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }
    bm_common_config_delete_key_response_t* del_key_resp = (bm_common_config_delete_key_response_t*) packet->payload;
    del_key_resp->header.target_node_id = 0; // UNUSED
    del_key_resp->header.source_node_id = node_id;
    del_key_resp->partition = partition;
    del_key_resp->success = success;
    del_key_resp->key_length = key_len;
    memcpy(del_key_resp->key, key, key_len);
    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);
    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }
  } while(0);
  return rval;
}

bm_serial_error_e bm_serial_send_info_request(uint64_t node_id) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_serial_device_info_request_t);
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_DEVICE_INFO_REQ, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }
    bm_serial_device_info_request_t *device_info_req_msg = (bm_serial_device_info_request_t *)packet->payload;
    device_info_req_msg->target_node_id = node_id;
    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);
    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }
  } while(0);
  return rval;
}

bm_serial_error_e bm_serial_send_info_reply(uint64_t node_id, bm_serial_device_info_reply_t* bcmp_info) {
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_serial_device_info_request_t);
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_DEVICE_INFO_REQ, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }
    bm_serial_device_info_request_t *device_info_req_msg = (bm_serial_device_info_request_t *)packet->payload;
    device_info_req_msg->target_node_id = node_id;
    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);
    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }
  } while(0);
  return rval;
}

bm_serial_error_e bm_serial_send_resource_request(uint64_t node_id){
  bm_serial_error_e rval = BM_SERIAL_OK;
  do {
    uint16_t message_len = sizeof(bm_serial_packet_t) + sizeof(bm_serial_resource_table_request_t);
    bm_serial_packet_t *packet = _bm_serial_get_packet(BM_SERIAL_DEVICE_INFO_REQ, 0, message_len);

    if(!packet) {
      rval = BM_SERIAL_OUT_OF_MEMORY;
      break;
    }
    bm_serial_resource_table_request_t *resource_req_msg = (bm_serial_resource_table_request_t *)packet->payload;
    resource_req_msg->target_node_id = node_id;
    packet->crc16 = bm_serial_crc16_ccitt(0, (uint8_t *)packet, message_len);
    if(!_callbacks.tx_fn((uint8_t *)packet, message_len)) {
      rval = BM_SERIAL_TX_ERR;
      break;
    }
  } while(0);
}

bm_serial_error_e bm_serial_send_resource_reply(uint64_t node_id, bm_serial_resource_table_reply_t* bcmp_resource) {

}

// Process bm_serial packet (not COBS anymore!)
bm_serial_error_e bm_serial_process_packet(bm_serial_packet_t *packet, size_t len) {
  bm_serial_error_e rval = BM_SERIAL_OK;

  // calc the crc16 and compare
  uint16_t crc16_pre = packet->crc16;
  packet->crc16 = 0;
  do {
    uint16_t crc16_post = bm_serial_crc16_ccitt(0, (uint8_t *)packet, len);

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
                          data_len,
                          pub_header->type,
                          pub_header->version);

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

      case BM_SERIAL_REBOOT_INFO: {
        if(_callbacks.reboot_info_fn){
          bm_serial_reboot_info_t *reboot_info = (bm_serial_reboot_info_t *)packet->payload;
          _callbacks.reboot_info_fn(reboot_info->node_id, reboot_info->reboot_reason, reboot_info->gitSHA, reboot_info->reboot_count);
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
          _callbacks.dfu_end_fn(dfu_end->node_id, dfu_end->success, dfu_end->dfu_status);
        }
        break;
      }

      case BM_SERIAL_CFG_GET: {
        if(_callbacks.cfg_get_fn) {
          bm_common_config_get_t* cfg_get = (bm_common_config_get_t*)packet->payload;
          _callbacks.cfg_get_fn(cfg_get->header.target_node_id, cfg_get->partition, cfg_get->key_length, cfg_get->key);
        }
        break;
      }
      case BM_SERIAL_CFG_SET: {
        if(_callbacks.cfg_set_fn){
          bm_common_config_set_t* cfg_set = (bm_common_config_set_t*) packet->payload;
          _callbacks.cfg_set_fn(cfg_set->header.target_node_id, cfg_set->partition, cfg_set->key_length, (char *)cfg_set->keyAndData, cfg_set->data_length, &cfg_set->keyAndData[cfg_set->key_length]);
        }
        break;
      }
      case BM_SERIAL_CFG_VALUE: {
        if(_callbacks.cfg_value_fn) {
          bm_common_config_value_t* cfg_value = (bm_common_config_value_t *) packet->payload;
          _callbacks.cfg_value_fn(cfg_value->header.source_node_id, cfg_value->partition, cfg_value->data_length, cfg_value->data);
        }
        break;
      }
      case BM_SERIAL_CFG_COMMIT: {
        if(_callbacks.cfg_commit_fn){
          bm_common_config_commit_t* cfg_commit = (bm_common_config_commit_t*) packet->payload;
          _callbacks.cfg_commit_fn(cfg_commit->header.target_node_id, cfg_commit->partition);
        }
        break;
      }
      case BM_SERIAL_CFG_STATUS_REQ: {
        if(_callbacks.cfg_status_request_fn){
          bm_common_config_status_request_t* cfg_status_req = (bm_common_config_status_request_t*) packet->payload;
          _callbacks.cfg_status_request_fn(cfg_status_req->header.target_node_id, cfg_status_req->partition);
        }
        break;
      }
      case BM_SERIAL_CFG_STATUS_RESP: {
        if(_callbacks.cfg_status_response_fn){
          bm_common_config_status_response_t* cfg_status_resp = (bm_common_config_status_response_t*) packet->payload;
          _callbacks.cfg_status_response_fn(cfg_status_resp->header.source_node_id, cfg_status_resp->partition, cfg_status_resp->committed, cfg_status_resp->num_keys, cfg_status_resp->keyData);
        }
        break;
      }
      case BM_SERIAL_CFG_DEL_REQ: {
        if(_callbacks.cfg_key_del_request_fn) {
          bm_common_config_delete_key_request_t* cfg_del_req = (bm_common_config_delete_key_request_t*) packet->payload;
          _callbacks.cfg_key_del_request_fn(cfg_del_req->header.target_node_id, cfg_del_req->partition, cfg_del_req->key_length, cfg_del_req->key);
        }
        break;
      }
      case BM_SERIAL_CFG_DEL_RESP: {
        if(_callbacks.cfg_key_del_response_fn) {
          bm_common_config_delete_key_response_t* cfg_del_resp = (bm_common_config_delete_key_response_t *) packet->payload;
          _callbacks.cfg_key_del_response_fn(cfg_del_resp->header.source_node_id, cfg_del_resp->partition, cfg_del_resp->key_length, cfg_del_resp->key, cfg_del_resp->success);
        }
        break;
      }
      case BM_SERIAL_NETWORK_INFO: {
        if(_callbacks.network_info_fn) {
          bm_common_network_info_t* network_info = (bm_common_network_info_t*) packet->payload;
          _callbacks.network_info_fn(network_info);
        }
        break;
      }
      default: {
        rval = BM_SERIAL_UNSUPPORTED_MSG;
        break;
      }
    }

  } while(0);

  return rval;
}
