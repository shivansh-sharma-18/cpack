#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compress.h"

static int run_round_trip_test(const char *test_name, const uint8_t *input,
                               size_t input_length) {
  CpackCompressedBuffer compressed = {0};

  uint8_t *restored = NULL;
  size_t restored_length = 0;

  if (!cpack_compress_buffer(input, input_length, &compressed)) {
    printf("FAIL: %s - compression failed\n", test_name);
    return 0;
  }

  if (!cpack_decompress_buffer(&compressed, &restored, &restored_length)) {
    printf("FAIL: %s - decompression failed\n", test_name);
    cpack_free_compressed_buffer(&compressed);
    return 0;
  }

  int passed = (restored_length == input_length);

  if (passed && input_length > 0) {
    passed = (memcmp(input, restored, input_length) == 0);
  }

  if (passed) {
    printf("PASS: %s\n", test_name);
    printf("  Original size:   %zu bytes\n", input_length);
    printf("  Compressed size: %zu bytes\n", compressed.compressed_size);
  } else {
    printf("FAIL: %s - data mismatch\n", test_name);
    printf("  Expected size: %zu bytes\n", input_length);
    printf("  Restored size: %zu bytes\n", restored_length);
  }

  free(restored);
  cpack_free_compressed_buffer(&compressed);

  return passed;
}

static int
expect_decompression_failure(const char *test_name,
                             const CpackCompressedBuffer *compressed) {
  uint8_t *output = NULL;
  size_t output_length = 0;

  int result = cpack_decompress_buffer(compressed, &output, &output_length);

  if (result || output != NULL || output_length != 0) {
    printf("FAIL: %s - invalid input was accepted\n", test_name);
    free(output);
    return 0;
  }

  printf("PASS: %s\n", test_name);
  return 1;
}

static int test_invalid_arguments(void) {
  CpackCompressedBuffer compressed = {0};

  if (cpack_compress_buffer(NULL, 10, &compressed)) {
    printf("FAIL: NULL input validation\n");
    cpack_free_compressed_buffer(&compressed);
    return 0;
  }

  if (cpack_compress_buffer(NULL, 0, NULL)) {
    printf("FAIL: NULL result validation\n");
    return 0;
  }

  printf("PASS: Invalid argument validation\n");
  return 1;
}

static int test_invalid_metadata(void) {
  const uint8_t input[] =
      "CPack metadata validation test with repeated repeated data.";

  CpackCompressedBuffer original = {0};

  if (!cpack_compress_buffer(input, sizeof(input) - 1, &original)) {
    printf("FAIL: Could not prepare metadata test input\n");
    return 0;
  }

  int passed = 0;
  int total = 0;

  CpackCompressedBuffer corrupted = original;

  corrupted.data = original.data;
  corrupted.compressed_size++;

  total++;
  passed += expect_decompression_failure("Invalid compressed payload size",
                                         &corrupted);

  corrupted = original;
  corrupted.bit_length = (uint64_t)original.compressed_size * 8 + 1;

  total++;
  passed += expect_decompression_failure("Bit length exceeds payload capacity",
                                         &corrupted);

  corrupted = original;
  corrupted.frequencies[0]++;

  total++;
  passed += expect_decompression_failure("Incorrect Huffman frequency total",
                                         &corrupted);

  corrupted = original;
  corrupted.data = NULL;

  total++;
  passed += expect_decompression_failure("Missing compressed data", &corrupted);

  CpackCompressedBuffer invalid_empty = {0};
  invalid_empty.original_length = 0;
  invalid_empty.compressed_size = 1;

  total++;
  passed += expect_decompression_failure("Invalid empty-input metadata",
                                         &invalid_empty);

  cpack_free_compressed_buffer(&original);

  printf("\nMetadata tests passed: %d/%d\n", passed, total);

  return passed == total;
}

static int test_invalid_bitstream(void) {
  const uint8_t input[] =
      "CPack bounded Huffman decoding validation with repeated repeated data.";

  CpackCompressedBuffer original = {0};

  if (!cpack_compress_buffer(input, sizeof(input) - 1, &original)) {
    printf("FAIL: Could not prepare bitstream test input\n");
    return 0;
  }

  int passed = 0;
  int total = 0;

  uint64_t remainder = original.bit_length % 8;

  if (remainder > 1) {
    CpackCompressedBuffer corrupted = original;
    corrupted.bit_length--;

    total++;
    passed += expect_decompression_failure(
        "Truncated declared Huffman bit length", &corrupted);
  } else if (remainder < 7) {
    CpackCompressedBuffer corrupted = original;
    corrupted.bit_length++;

    total++;
    passed += expect_decompression_failure(
        "Extended declared Huffman bit length", &corrupted);
  } else {
    printf("SKIP: No suitable bit-length mutation\n");
  }

  if (remainder != 0) {
    size_t last_index = original.compressed_size - 1;
    uint8_t original_byte = original.data[last_index];

    original.data[last_index] |= (uint8_t)(1U << remainder);

    total++;
    passed += expect_decompression_failure("Non-zero Huffman padding bits",
                                           &original);

    original.data[last_index] = original_byte;
  } else {
    printf("SKIP: Payload is byte-aligned; padding test unavailable\n");
  }

  cpack_free_compressed_buffer(&original);

  printf("\nBitstream tests passed: %d/%d\n", passed, total);

  return total > 0 && passed == total;
}

int main(void) {
  int passed = 0;
  int total = 0;

  const uint8_t text[] =
      "CPack is a custom compression and archiving tool written in C.";

  const uint8_t repeated[] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                             "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

  const uint8_t binary_data[] = {
      0x00, 0x01, 0x02, 0x03, 0x00, 0xFF, 0xFE, 0xFD, 0x10, 0x20, 0x30, 0x40,
      0x00, 0xAA, 0xBB, 0xCC, 0x7F, 0x80, 0x81, 0x82, 0x00, 0x11, 0x22, 0x33};

  uint8_t all_bytes[256];

  for (int i = 0; i < 256; i++) {
    all_bytes[i] = (uint8_t)i;
  }

  printf("Running CPack compression tests...\n\n");

  total++;
  passed += run_round_trip_test("Empty input", NULL, 0);

  total++;
  passed += run_round_trip_test("Normal text", text, sizeof(text) - 1);

  total++;
  passed += run_round_trip_test("Repeated characters", repeated,
                                sizeof(repeated) - 1);

  total++;
  passed +=
      run_round_trip_test("Binary data", binary_data, sizeof(binary_data));

  total++;
  passed +=
      run_round_trip_test("All 256 byte values", all_bytes, sizeof(all_bytes));

  total++;
  passed += test_invalid_arguments();

  total++;
  passed += test_invalid_metadata();

  total++;
  passed += test_invalid_bitstream();

  printf("\nTests passed: %d/%d\n", passed, total);

  return (passed == total) ? EXIT_SUCCESS : EXIT_FAILURE;
}