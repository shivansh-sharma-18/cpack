
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"

#define TEST_ARCHIVE "test_archive.cpk"

#define CPACK_VERSION_1 1
#define CPACK_VERSION_2 2

static const uint8_t CPACK_MAGIC[4] = {'C', 'P', 'A', 'K'};

static int write_u32(FILE *file, uint32_t value) {
  for (int i = 0; i < 4; i++) {
    if (fputc((int)((value >> (i * 8)) & 0xFF), file) == EOF) {
      return 0;
    }
  }

  return 1;
}

static int write_u64(FILE *file, uint64_t value) {
  for (int i = 0; i < 8; i++) {
    if (fputc((int)((value >> (i * 8)) & 0xFF), file) == EOF) {
      return 0;
    }
  }

  return 1;
}

static int write_v1_archive(const char *path,
                            const CpackCompressedBuffer *compressed) {
  FILE *file = fopen(path, "wb");

  if (file == NULL) {
    return 0;
  }

  int success = 1;

  if (fwrite(CPACK_MAGIC, 1, sizeof(CPACK_MAGIC), file) !=
      sizeof(CPACK_MAGIC)) {
    success = 0;
  }

  if (success && !write_u32(file, CPACK_VERSION_1)) {
    success = 0;
  }

  if (success && !write_u64(file, compressed->original_length)) {
    success = 0;
  }

  if (success && !write_u64(file, (uint64_t)compressed->compressed_size)) {
    success = 0;
  }

  if (success && !write_u64(file, compressed->bit_length)) {
    success = 0;
  }

  if (success && !write_u64(file, compressed->token_stream_length)) {
    success = 0;
  }

  for (size_t i = 0; success && i < HUFFMAN_SYMBOLS; i++) {
    if (!write_u64(file, compressed->frequencies[i])) {
      success = 0;
    }
  }

  if (success && compressed->compressed_size > 0) {
    if (fwrite(compressed->data, 1, compressed->compressed_size, file) !=
        compressed->compressed_size) {
      success = 0;
    }
  }

  if (fclose(file) != 0) {
    success = 0;
  }

  return success;
}

static int verify_restored_data(const CpackCompressedBuffer *compressed,
                                const uint8_t *input, size_t input_length) {
  uint8_t *output = NULL;
  size_t output_length = 0;

  if (!cpack_decompress_buffer(compressed, &output, &output_length)) {
    free(output);
    return 0;
  }

  int passed =
      output_length == input_length && memcmp(input, output, input_length) == 0;

  free(output);
  return passed;
}

static int test_archive_round_trip(void) {
  CpackCompressedBuffer original = {0};
  CpackCompressedBuffer restored = {0};

  const unsigned char input[] =
      "CPack archive module test with repeated repeated data.";

  int passed = 0;

  if (!cpack_compress_buffer(input, sizeof(input) - 1, &original)) {
    printf("FAIL: Compression failed\n");
    goto cleanup;
  }

  if (!cpack_archive_write(TEST_ARCHIVE, &original)) {
    printf("FAIL: Archive write failed\n");
    goto cleanup;
  }

  if (!cpack_archive_read(TEST_ARCHIVE, &restored)) {
    printf("FAIL: Archive read failed\n");
    goto cleanup;
  }

  if (!verify_restored_data(&restored, input, sizeof(input) - 1)) {
    printf("FAIL: Compressed archive data mismatch\n");
    goto cleanup;
  }

  printf("PASS: Compressed archive write/read round trip\n");
  passed = 1;

cleanup:
  cpack_free_compressed_buffer(&original);
  cpack_free_compressed_buffer(&restored);
  remove(TEST_ARCHIVE);

  return passed;
}

static int test_raw_archive_round_trip(void) {
  CpackCompressedBuffer original = {0};
  CpackCompressedBuffer restored = {0};

  const uint8_t input[] = {0x00, 0xFF, 0x12, 0x80, 0x34, 0x00, 0xAB, 0xCD,
                           0xEF, 0x01, 0x7F, 0x90, 0x55, 0xAA, 0x10, 0xFE};

  int passed = 0;

  original.data = malloc(sizeof(input));

  if (original.data == NULL) {
    printf("FAIL: Raw buffer allocation failed\n");
    goto cleanup;
  }

  memcpy(original.data, input, sizeof(input));

  original.original_length = sizeof(input);
  original.compressed_size = sizeof(input);
  original.mode = CPACK_MODE_RAW;

  if (!cpack_archive_write(TEST_ARCHIVE, &original)) {
    printf("FAIL: Raw archive write failed\n");
    goto cleanup;
  }

  if (!cpack_archive_read(TEST_ARCHIVE, &restored)) {
    printf("FAIL: Raw archive read failed\n");
    goto cleanup;
  }

  if (restored.mode != CPACK_MODE_RAW) {
    printf("FAIL: Raw storage mode was not preserved\n");
    goto cleanup;
  }

  if (!verify_restored_data(&restored, input, sizeof(input))) {
    printf("FAIL: Raw archive data mismatch\n");
    goto cleanup;
  }

  printf("PASS: Raw archive write/read round trip\n");
  passed = 1;

cleanup:
  cpack_free_compressed_buffer(&original);
  cpack_free_compressed_buffer(&restored);
  remove(TEST_ARCHIVE);

  return passed;
}

static int test_v1_compatibility(void) {
  CpackCompressedBuffer original = {0};
  CpackCompressedBuffer restored = {0};

  uint8_t input[2048];

  const char pattern[] = "CPack V1 backward compatibility test. ";

  for (size_t i = 0; i < sizeof(input); i++) {
    input[i] = (uint8_t)pattern[i % (sizeof(pattern) - 1)];
  }

  int passed = 0;

  if (!cpack_compress_buffer(input, sizeof(input), &original)) {
    printf("FAIL: V1 test compression failed\n");
    goto cleanup;
  }

  if (original.mode != CPACK_MODE_COMPRESSED) {
    printf("FAIL: V1 test requires compressed storage\n");
    goto cleanup;
  }

  if (!write_v1_archive(TEST_ARCHIVE, &original)) {
    printf("FAIL: Could not create V1 archive\n");
    goto cleanup;
  }

  if (!cpack_archive_read(TEST_ARCHIVE, &restored)) {
    printf("FAIL: V1 archive was not accepted\n");
    goto cleanup;
  }

  if (restored.mode != CPACK_MODE_COMPRESSED) {
    printf("FAIL: V1 archive mode was not interpreted correctly\n");
    goto cleanup;
  }

  if (!verify_restored_data(&restored, input, sizeof(input))) {
    printf("FAIL: V1 restored data mismatch\n");
    goto cleanup;
  }

  printf("PASS: Archive V1 backward compatibility\n");
  passed = 1;

cleanup:
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

  int write_success = fwrite(invalid_data, 1, sizeof(invalid_data), file) ==
                      sizeof(invalid_data);

  if (fclose(file) != 0) {
    write_success = 0;
  }

  if (!write_success) {
    remove(TEST_ARCHIVE);
    printf("FAIL: Could not write invalid archive\n");
    return 0;
  }

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

static int test_unsupported_version(void) {
  FILE *file = fopen(TEST_ARCHIVE, "wb");

  if (file == NULL) {
    printf("FAIL: Could not create version test archive\n");
    return 0;
  }

  int success =
      fwrite(CPACK_MAGIC, 1, sizeof(CPACK_MAGIC), file) == sizeof(CPACK_MAGIC);

  if (success && !write_u32(file, 99)) {
    success = 0;
  }

  if (fclose(file) != 0) {
    success = 0;
  }

  if (!success) {
    remove(TEST_ARCHIVE);
    printf("FAIL: Could not write version test archive\n");
    return 0;
  }

  CpackCompressedBuffer compressed = {0};

  int result = cpack_archive_read(TEST_ARCHIVE, &compressed);

  cpack_free_compressed_buffer(&compressed);
  remove(TEST_ARCHIVE);

  if (result) {
    printf("FAIL: Unsupported archive version was accepted\n");
    return 0;
  }

  printf("PASS: Unsupported archive version rejection\n");
  return 1;
}

int main(void) {
  int passed = 0;
  const int total = 5;

  printf("Running CPack archive tests...\n\n");

  passed += test_archive_round_trip();
  passed += test_raw_archive_round_trip();
  passed += test_v1_compatibility();
  passed += test_invalid_archive();
  passed += test_unsupported_version();

  printf("\nTests passed: %d/%d\n", passed, total);

  return passed == total ? EXIT_SUCCESS : EXIT_FAILURE;
}
