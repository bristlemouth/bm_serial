#pragma once

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t crc16_ccitt(uint16_t seed, const uint8_t *src, size_t len);

#ifdef __cplusplus
}
#endif
