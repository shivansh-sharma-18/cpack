#include <stdio.h>

#include "bitio.h"

int main(void) {

  int expected[] = {1, 0, 1, 1, 0, 0, 1, 0, 1};

  int bit_count = sizeof(expected) / sizeof(expected[0]);

  FILE *file = fopen("/tmp/cpack_bitio_test.bin", "wb");

  if (file == NULL) {
    perror("Failed to create test file");
    return 1;
  }

  BitWriter writer;

  bitwriter_init(&writer, file);

  for (int i = 0; i < bit_count; i++) {
    if (!bitwriter_write_bit(&writer, expected[i])) {
      printf("FAIL: Could not write bit %d\n", i);
      fclose(file);
      return 1;
    }
  }

  if (!bitwriter_flush(&writer)) {
    printf("FAIL: Could not flush writer\n");
    fclose(file);
    return 1;
  }

  fclose(file);

  file = fopen("/tmp/cpack_bitio_test.bin", "rb");

  if (file == NULL) {
    perror("Failed to open test file");
    return 1;
  }

  BitReader reader;

  bitreader_init(&reader, file);

  for (int i = 0; i < bit_count; i++) {
    int actual = bitreader_read_bit(&reader);

    if (actual != expected[i]) {
      printf("FAIL: Bit %d - expected %d, got %d\n", i, expected[i], actual);

      fclose(file);
      return 1;
    }
  }

  fclose(file);

  remove("/tmp/cpack_bitio_test.bin");

  printf("PASS: Bit I/O round-trip test\n");

  return 0;
}