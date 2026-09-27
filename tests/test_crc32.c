#include <stdint.h>
#include <stdio.h>

#include "crc32.h"

int main(void) {
  const uint8_t data[] = "123456789";

  uint32_t result = crc32(data, 9);

  printf("CRC32: 0x%08X\n", result);

  if (result != 0xCBF43926U) {
    printf("FAIL: CRC32 test\n");
    return 1;
  }

  printf("PASS: CRC32 test\n");

  uint32_t empty_result = crc32(NULL, 0);

  printf("Empty CRC32: 0x%08X\n", empty_result);

  if (empty_result != 0x00000000U) {
    printf("FAIL: Empty CRC32 test\n");
    return 1;
  }
  const uint8_t other_data[] = "12345678";

  uint32_t other_result = crc32(other_data, 8);

  printf("Other CRC32: 0x%08X\n", other_result);

  if (other_result == result) {
    printf("FAIL: Different data produced same CRC32\n");
    return 1;
  }

  return 0;
}