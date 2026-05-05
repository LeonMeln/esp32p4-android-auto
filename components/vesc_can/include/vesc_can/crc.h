/*
    Copyright 2016 Benjamin Vedder benjamin@vedder.se
    Adapted from VESC firmware (GPL-3.0).
*/

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned short crc16(const unsigned char *buf, unsigned int len);
unsigned short crc16_with_init(const unsigned char *buf, unsigned int len,
                               unsigned short cksum);
uint32_t       crc32_with_init(const uint8_t *buf, uint32_t len, uint32_t cksum);

#ifdef __cplusplus
}
#endif
