#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"

#define TEST_ARCHIVE "test_archive.cpk"

static int test_archive_round_trip(void) {
  CpackCompressedBuffer original = {0};
  CpackCompressedBuffer restored = {0};

  const unsigned char input[] =
      "CPack archive module test with repeated repeated data.";

  uint8_t *decompressed = NULL;
  size_t decompressed_length = 0;

  if (!cpack_compress_buffer(input, sizeof(input) - 1, &original)) {
    printf("FAIL: Compression failed\n");
    return 0;
  }

  if (!cpack_archive_write(TEST_ARCHIVE, &original)) {
    printf("FAIL: Archive write failed\n");
    cpack_free_compressed_buffer(&original);
    return 0;
  }

  if (!cpack_archive_read(TEST_ARCHIVE, &restored)) {
    printf("FAIL: Archive read failed\n");
    cpack_free_compressed_buffer(&original);
    remove(TEST_ARCHIVE);
    return 0;
  }

  if (!cpack_decompress_buffer(&restored, &decompressed,
                               &decompressed_length)) {
    printf("FAIL: Decompression failed\n");
    cpack_free_compressed_buffer(&original);
    cpack_free_compressed_buffer(&restored);
    remove(TEST_ARCHIVE);
    return 0;
  }

  int passed = decompressed_length == sizeof(input) - 1 &&
               memcmp(input, decompressed, sizeof(input) - 1) == 0;

  if (passed) {
    printf("PASS: Archive write/read round trip\n");
  } else {
    printf("FAIL: Restored data mismatch\n");
  }

  free(decompressed);
  cpack_free_compressed_buffer(&original);
  cpack_free_compressed_buffer(&restored);
  remove(TEST_ARCHIVE);

  return passed;
}

static int test_invalid_archive(void) {
  FILE *file = fopen(TEST_ARCHIVE, "wb");

  if (file == NULL) {
    printf("FAIL: Could not create invalid archive\n");
    return 0;
  }

  const unsigned char invalid_data[] = {'B',  'A',  'D',  '!',
                                        0x00, 0x00, 0x00, 0x00};

  fwrite(invalid_data, 1, sizeof(invalid_data), file);
  fclose(file);

  CpackCompressedBuffer compressed = {0};

  int result = cpack_archive_read(TEST_ARCHIVE, &compressed);

  cpack_free_compressed_buffer(&compressed);
  remove(TEST_ARCHIVE);

  if (result) {
    printf("FAIL: Invalid archive was accepted\n");
    return 0;
  }

  printf("PASS: Invalid archive rejection\n");
  return 1;
}

int main(void) {
  int passed = 0;
  int total = 2;

  printf("Running CPack archive tests...\n\n");

  passed += test_archive_round_trip();
  passed += test_invalid_archive();

  printf("\nTests passed: %d/%d\n", passed, total);

  return passed == total ? EXIT_SUCCESS : EXIT_FAILURE;
}
