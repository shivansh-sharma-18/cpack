#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "huffman.h"

#define TEST_FILE "/tmp/cpack_huffman_test.bin"

static int test_frequency_counting(void) {
  const uint8_t data[] = "BANANA";
  uint64_t frequencies[HUFFMAN_SYMBOLS];

  huffman_count_frequencies(data, sizeof(data) - 1, frequencies);

  if (frequencies['B'] != 1 || frequencies['A'] != 3 || frequencies['N'] != 2) {
    return 0;
  }

  return 1;
}

static int test_huffman_round_trip(void) {
  const uint8_t data[] = "BANANA BANANA BANANA BANANA";
  size_t length = sizeof(data) - 1;

  uint64_t frequencies[HUFFMAN_SYMBOLS];
  huffman_count_frequencies(data, length, frequencies);

  HuffmanNode *root = huffman_build_tree(frequencies);
  if (root == NULL) {
    return 0;
  }

  HuffmanCode codes[HUFFMAN_SYMBOLS];
  if (!huffman_generate_codes(root, codes)) {
    huffman_free_tree(root);
    return 0;
  }

  FILE *file = fopen(TEST_FILE, "wb");
  if (file == NULL) {
    huffman_free_tree(root);
    return 0;
  }

  BitWriter writer;
  bitwriter_init(&writer, file);

  if (!huffman_encode(data, length, codes, &writer)) {
    fclose(file);
    huffman_free_tree(root);
    return 0;
  }

  if (!bitwriter_flush(&writer)) {
    fclose(file);
    huffman_free_tree(root);
    return 0;
  }

  fclose(file);

  file = fopen(TEST_FILE, "rb");
  if (file == NULL) {
    huffman_free_tree(root);
    return 0;
  }

  BitReader reader;
  bitreader_init(&reader, file);

  uint8_t *decoded = huffman_decode(&reader, root, length);
  fclose(file);

  if (decoded == NULL) {
    huffman_free_tree(root);
    return 0;
  }

  int success = (memcmp(data, decoded, length) == 0);

  free(decoded);
  huffman_free_tree(root);

  return success;
}

static int test_single_symbol(void) {
  const uint8_t data[] = "AAAAAAAAAAAAAAAAAAAA";
  size_t length = sizeof(data) - 1;

  uint64_t frequencies[HUFFMAN_SYMBOLS];
  huffman_count_frequencies(data, length, frequencies);

  HuffmanNode *root = huffman_build_tree(frequencies);
  if (root == NULL) {
    return 0;
  }

  HuffmanCode codes[HUFFMAN_SYMBOLS];
  if (!huffman_generate_codes(root, codes)) {
    huffman_free_tree(root);
    return 0;
  }

  FILE *file = fopen(TEST_FILE, "wb");
  if (file == NULL) {
    huffman_free_tree(root);
    return 0;
  }

  BitWriter writer;
  bitwriter_init(&writer, file);

  if (!huffman_encode(data, length, codes, &writer)) {
    fclose(file);
    huffman_free_tree(root);
    return 0;
  }

  if (!bitwriter_flush(&writer)) {
    fclose(file);
    huffman_free_tree(root);
    return 0;
  }

  fclose(file);

  file = fopen(TEST_FILE, "rb");
  if (file == NULL) {
    huffman_free_tree(root);
    return 0;
  }

  BitReader reader;
  bitreader_init(&reader, file);

  uint8_t *decoded = huffman_decode(&reader, root, length);
  fclose(file);

  if (decoded == NULL) {
    huffman_free_tree(root);
    return 0;
  }

  int success = (memcmp(data, decoded, length) == 0);

  free(decoded);
  huffman_free_tree(root);

  return success;
}

int main(void) {
  if (!test_frequency_counting()) {
    printf("FAIL: Frequency counting test\n");
    return 1;
  }
  printf("PASS: Frequency counting test\n");

  if (!test_huffman_round_trip()) {
    printf("FAIL: Huffman round-trip test\n");
    return 1;
  }
  printf("PASS: Huffman round-trip test\n");

  if (!test_single_symbol()) {
    printf("FAIL: Single-symbol Huffman test\n");
    return 1;
  }
  printf("PASS: Single-symbol Huffman test\n");

  remove(TEST_FILE);

  printf("PASS: All Huffman tests\n");
  return 0;
}