#include "crc32.h"

uint32_t crc32(const uint8_t *data, size_t length) {

  uint32_t crc = 0xFFFFFFFF;

  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];

    for (int bit = 0; bit < 8; bit++) {
      if (crc & 1U) {
        crc = (crc >> 1) ^ 0xEDB88320;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc ^ 0xFFFFFFFF;
}