#include "lz77.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_round_trip(const char *input) {
  size_t input_length = strlen(input);

  LZ77TokenArray compressed = {0};

  if (!lz77_compress((const uint8_t *)input, input_length, &compressed)) {
    printf("FAIL: Compression failed\n");
    return 0;
  }

  uint8_t *decompressed =
      lz77_decompress(compressed.tokens, compressed.count, input_length);

  if (decompressed == NULL) {
    printf("FAIL: Decompression failed\n");
    lz77_free_tokens(&compressed);
    return 0;
  }

  int passed = memcmp(input, decompressed, input_length) == 0;

  if (passed) {
    printf("PASS: %s\n", input);
    printf("Original size: %zu bytes\n", input_length);
    printf("Token count: %zu\n\n", compressed.count);
  } else {
    printf("FAIL: Round-trip mismatch\n");
  }

  free(decompressed);
  lz77_free_tokens(&compressed);

  return passed;
}

int main(void) {
  int passed = 1;

  passed &= test_round_trip("ABCABCABC");
  passed &= test_round_trip("AAAAAAAAAAAA");
  passed &= test_round_trip("ABCDEFG");
  passed &= test_round_trip("A");
  passed &= test_round_trip("This is a test. This is a test.");
  passed &= test_round_trip("");

  if (passed) {
    printf("ALL LZ77 TESTS PASSED\n");
    return 0;
  }

  printf("LZ77 TESTS FAILED\n");
  return 1;
}