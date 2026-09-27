#ifndef CRC32_H
#define CRC32_H

#include <stddef.h>
#include <stdint.h>

uint32_t crc32(const uint8_t *data, size_t length);

#endif