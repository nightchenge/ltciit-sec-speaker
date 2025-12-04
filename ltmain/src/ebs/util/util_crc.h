#ifndef __UTIL_CRC_H__
#define __UTIL_CRC_H__

#include "util_types.h"

uint16_t api_crc16(uint16_t crc16val, uint8_t const *buffer, uint32_t len);
uint32_t api_crc32(uint32_t crc32val, uint8_t *s, int len);
uint32_t crc32_mpeg2(const uint8_t *data, int len);
uint16_t crc16_ccitt_false(const uint8_t *data, int len);
uint8_t get_crc8(const uint8_t *data, int len);
uint8_t get_crc8_maxim(const uint8_t *data, int len);
#endif
