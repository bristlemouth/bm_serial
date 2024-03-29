#pragma once

#include "bm_common_structs.h"
#include "bm_serial_messages.h"
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // Function used to transmit data over the wire
  bool (*tx_fn)(const uint8_t *buff, size_t len);

  // Function called when published data is received
  bool (*pub_fn)(const char *topic, uint16_t topic_len, uint64_t node_id,
                 const uint8_t *payload, size_t len, uint8_t type,
                 uint8_t version);

  // Function called when a subscribe request is received
  bool (*sub_fn)(const char *topic, uint16_t topic_len);

  // Function called when an unsusbscribe request is received
  bool (*unsub_fn)(const char *topic, uint16_t topic_len);

  // Function called when a log request is received
  bool (*log_fn)(uint64_t node_id, const uint8_t *data, size_t len);

  // Function called when a log request is received
  bool (*debug_fn)(const uint8_t *data, size_t len);

  // Function called when a message to send over wireless network is received
  bool (*net_msg_fn)(uint64_t node_id, const uint8_t *data, size_t len);

  // Function called to set RTC on device
  bool (*rtc_set_fn)(bm_serial_time_t *time);

  // Function called when self test is received.
  bool (*self_test_fn)(uint64_t node_id, uint32_t result);

  // Function called when reboot info is received.
  bool (*reboot_info_fn)(uint64_t node_id, uint32_t reboot_reason,
                         uint32_t gitSHA, uint32_t reboot_count,
                         uint32_t pc, uint32_t lr);

  // Function called when dfu start message is recieved.
  bool (*dfu_start_fn)(bm_serial_dfu_start_t *dfu_start);

  // Function called when a dfu chunk is recieved.
  bool (*dfu_chunk_fn)(uint32_t offset, size_t length, uint8_t *data);

  // Function called when a dfu end is received.
  bool (*dfu_end_fn)(uint64_t node_id, bool success, uint32_t err);

  // Function called when a cfg get is received.
  bool (*cfg_get_fn)(uint64_t node_id, bm_common_config_partition_e partition,
                     size_t key_len, const char *key);

  // Function called when a cfg set is received.
  bool (*cfg_set_fn)(uint64_t node_id, bm_common_config_partition_e partition,
                     size_t key_len, const char *key, size_t value_size,
                     void *val);

  // Function called when a cfg value is recieved.
  bool (*cfg_value_fn)(uint64_t node_id, bm_common_config_partition_e partition,
                       uint32_t data_length, void *data);

  // Function called when a cfg commit is received.
  bool (*cfg_commit_fn)(uint64_t node_id,
                        bm_common_config_partition_e partition);

  // Function called when a cfg status request is received.
  bool (*cfg_status_request_fn)(uint64_t node_id,
                                bm_common_config_partition_e partition);

  // Function called when a cfg status response is received.
  bool (*cfg_status_response_fn)(uint64_t node_id,
                                 bm_common_config_partition_e partition,
                                 bool commited, uint8_t num_keys, void *keys);

  // Function called when a cfg del is received.
  bool (*cfg_key_del_request_fn)(uint64_t node_id,
                                 bm_common_config_partition_e partition,
                                 size_t key_len, const char *key);

  // Function called when a cfg del is received.
  bool (*cfg_key_del_response_fn)(uint64_t node_id,
                                  bm_common_config_partition_e partition,
                                  size_t key_len, const char *key,
                                  bool success);

  // Function called when a network info is received.
  bool (*network_info_fn)(bm_common_network_info_t *network_info);

  // Function called when a BCMP info request is received.
  bool (*bcmp_info_request_fn)(uint64_t node_id);

  // Function called when a BCMP info response is received.
  bool (*bcmp_info_response_fn)(uint64_t node_id,
                                bm_serial_device_info_reply_t *bcmp_info);

  // Function called when a BCMP resource request is received.
  bool (*bcmp_resource_request_fn)(uint64_t node_id);

  // Function called when a BCMP resource response is received.
  bool (*bcmp_resource_response_fn)(
      uint64_t node_id, bm_serial_resource_table_reply_t *bcmp_resource);
} bm_serial_callbacks_t;

typedef enum {
  BM_SERIAL_OK = 0,
  BM_SERIAL_NULL_BUFF = -1,
  BM_SERIAL_OVERFLOW = -2,
  BM_SERIAL_MISSING_CALLBACK = -3,
  BM_SERIAL_OUT_OF_MEMORY = -4,
  BM_SERIAL_TX_ERR = -5,
  BM_SERIAL_CRC_ERR = -6,
  BM_SERIAL_UNSUPPORTED_MSG = -7,
  BM_SERIAL_INVALID_TOPIC_LEN = -8,
  BM_SERIAL_INVALID_MSG_LEN = -9,
  BM_SERIAL_MISC_ERR = -10,
} bm_serial_error_e;

void bm_serial_set_callbacks(bm_serial_callbacks_t *callbacks);
bm_serial_error_e bm_serial_process_packet(bm_serial_packet_t *packet,
                                           size_t len);
bm_serial_error_e bm_serial_tx(bm_serial_message_t type, const uint8_t *buff,
                               size_t len);

bm_serial_error_e bm_serial_pub(uint64_t node_id, const char *topic,
                                uint16_t topic_len, const uint8_t *data,
                                uint16_t data_len, uint8_t type,
                                uint8_t version);
bm_serial_error_e bm_serial_sub(const char *topic, uint16_t topic_len);
bm_serial_error_e bm_serial_unsub(const char *topic, uint16_t topic_len);
bm_serial_error_e bm_serial_set_rtc(bm_serial_time_t *time);
bm_serial_error_e bm_serial_send_self_test(uint64_t node_id, uint32_t result);
bm_serial_error_e bm_serial_send_reboot_info(uint64_t node_id,
                                             uint32_t reboot_reason,
                                             uint32_t gitSHA,
                                             uint32_t reboot_count,
                                             uint32_t pc,
                                             uint32_t lr);

bm_serial_error_e bm_serial_dfu_send_start(bm_serial_dfu_start_t *dfu_start);
bm_serial_error_e bm_serial_dfu_send_chunk(uint32_t offset, size_t length,
                                           uint8_t *data);
bm_serial_error_e bm_serial_dfu_send_finish(uint64_t node_id, bool success,
                                            uint32_t status);

bm_serial_error_e bm_serial_cfg_get(uint64_t node_id,
                                    bm_common_config_partition_e partition,
                                    size_t key_len, const char *key);
bm_serial_error_e bm_serial_cfg_set(uint64_t node_id,
                                    bm_common_config_partition_e partition,
                                    size_t key_len, const char *key,
                                    size_t value_size, void *val);
bm_serial_error_e bm_serial_cfg_value(uint64_t node_id,
                                      bm_common_config_partition_e partition,
                                      uint32_t data_length, void *data);
bm_serial_error_e bm_serial_cfg_commit(uint64_t node_id,
                                       bm_common_config_partition_e partition);
bm_serial_error_e
bm_serial_cfg_status_request(uint64_t node_id,
                             bm_common_config_partition_e partition);
bm_serial_error_e
bm_serial_cfg_status_response(uint64_t node_id,
                              bm_common_config_partition_e partition,
                              bool commited, uint8_t num_keys, void *keys);
bm_serial_error_e
bm_serial_cfg_delete_request(uint64_t node_id,
                             bm_common_config_partition_e partition,
                             size_t key_len, const char *key);
bm_serial_error_e
bm_serial_cfg_delete_response(uint64_t node_id,
                              bm_common_config_partition_e partition,
                              size_t key_len, const char *key, bool success);

bm_serial_error_e bm_serial_send_info_request(uint64_t node_id);
bm_serial_error_e
bm_serial_send_info_reply(uint64_t node_id,
                          bm_serial_device_info_reply_t *bcmp_info);

bm_serial_error_e bm_serial_send_resource_request(uint64_t node_id);
bm_serial_error_e
bm_serial_send_resource_reply(uint64_t node_id,
                              bm_serial_resource_table_reply_t *bcmp_resource);

bm_serial_error_e bm_serial_send_network_info(
    uint32_t network_crc32, bm_common_config_crc_t *config_crc,
    bm_common_fw_version_t *fw_info, uint16_t num_nodes, uint64_t *node_id_list,
    uint16_t config_map_size, uint8_t *cbor_config_map);
#ifdef __cplusplus
}
#endif
