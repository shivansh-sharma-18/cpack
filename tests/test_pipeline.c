#include "pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_round_trip(const char *input) {
  size_t input_length = strlen(input);

  uint8_t *output = NULL;
  size_t output_length = 0;

  if (!cpack_pipeline_round_trip((const uint8_t *)input, input_length, &output,
                                 &output_length)) {
    printf("FAIL: Pipeline returned an error\n");
    return 0;
  }

  if (output_length != input_length ||
      memcmp(input, output, input_length) != 0) {
    printf("FAIL: Data mismatch\n");
    free(output);
    return 0;
  }

  printf("PASS: %s\n", input);

  free(output);
  return 1;
}

int main(void) {
  if (!test_round_trip("ABCABCABC")) {
    return 1;
  }

  if (!test_round_trip("AAAAAAAAAAAA")) {
    return 1;
  }

  if (!test_round_trip("This is a test. This is a test.")) {
    return 1;
  }

  if (!test_round_trip("The quick brown fox jumps over the lazy dog")) {
    return 1;
  }

  if (!test_round_trip("")) {
    return 1;
  }

  printf("\nALL PIPELINE TESTS PASSED\n");

  return 0;
}