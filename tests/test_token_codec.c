
#include "token_codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition, message)                                              \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__);              \
      return 0;                                                                \
    }                                                                          \
  } while (0)

static int tokens_equal(const LZ77TokenArray *a, const LZ77TokenArray *b) {
  if (a->count != b->count) {
    return 0;
  }

  for (size_t i = 0; i < a->count; i++) {
    if (a->tokens[i].is_match != b->tokens[i].is_match ||
        a->tokens[i].literal != b->tokens[i].literal ||
        a->tokens[i].offset != b->tokens[i].offset ||
        a->tokens[i].length != b->tokens[i].length) {
      return 0;
    }
  }

  return 1;
}

static int test_literal_tokens(void) {
  LZ77Token original_data[] = {
      {.is_match = 0, .literal = 'A', .offset = 0, .length = 0},
      {.is_match = 0, .literal = 'B', .offset = 0, .length = 0},
      {.is_match = 0, .literal = 'C', .offset = 0, .length = 0},
  };

  LZ77TokenArray original = {
      .tokens = original_data,
      .count = 3,
  };

  uint8_t *serialized = NULL;
  size_t serialized_length = 0;

  CHECK(cpack_serialize_tokens(&original, &serialized, &serialized_length),
        "Literal serialization");

  CHECK(serialized_length == 6, "Literal serialized length");

  LZ77TokenArray decoded = {0};

  CHECK(cpack_deserialize_tokens(serialized, serialized_length, &decoded),
        "Literal deserialization");

  CHECK(tokens_equal(&original, &decoded), "Literal token equality");

  free(serialized);
  lz77_free_tokens(&decoded);

  printf("PASS: Literal tokens\n");
  return 1;
}

static int test_match_tokens(void) {
  LZ77Token original_data[] = {
      {.is_match = 0, .literal = 'A', .offset = 0, .length = 0},
      {.is_match = 1, .literal = 0, .offset = 1, .length = 3},
      {.is_match = 1, .literal = 0, .offset = 258, .length = 258},
  };

  LZ77TokenArray original = {
      .tokens = original_data,
      .count = 3,
  };

  uint8_t *serialized = NULL;
  size_t serialized_length = 0;

  CHECK(cpack_serialize_tokens(&original, &serialized, &serialized_length),
        "Match serialization");

  CHECK(serialized_length == 12, "Mixed token serialized length");

  LZ77TokenArray decoded = {0};

  CHECK(cpack_deserialize_tokens(serialized, serialized_length, &decoded),
        "Match deserialization");

  CHECK(tokens_equal(&original, &decoded), "Match token equality");

  free(serialized);
  lz77_free_tokens(&decoded);

  printf("PASS: Match tokens\n");
  return 1;
}

static int test_empty_tokens(void) {
  LZ77TokenArray original = {0};

  uint8_t *serialized = NULL;
  size_t serialized_length = 0;

  CHECK(cpack_serialize_tokens(&original, &serialized, &serialized_length),
        "Empty serialization");

  CHECK(serialized != NULL, "Empty serialization allocation");
  CHECK(serialized_length == 0, "Empty serialized length");

  LZ77TokenArray decoded = {0};

  CHECK(cpack_deserialize_tokens(serialized, serialized_length, &decoded),
        "Empty deserialization");

  CHECK(decoded.tokens == NULL, "Empty decoded token pointer");
  CHECK(decoded.count == 0, "Empty decoded token count");

  free(serialized);

  printf("PASS: Empty tokens\n");
  return 1;
}

static int test_invalid_token_type(void) {
  LZ77Token invalid_data[] = {
      {.is_match = 2, .literal = 'X', .offset = 0, .length = 0},
  };

  LZ77TokenArray invalid = {
      .tokens = invalid_data,
      .count = 1,
  };

  uint8_t *serialized = NULL;
  size_t serialized_length = 0;

  CHECK(!cpack_serialize_tokens(&invalid, &serialized, &serialized_length),
        "Reject invalid token type");

  CHECK(serialized == NULL, "Invalid serialization output");

  printf("PASS: Invalid token type\n");
  return 1;
}

static int test_invalid_match(void) {
  LZ77Token invalid_data[] = {
      {.is_match = 1, .literal = 0, .offset = 0, .length = 3},
  };

  LZ77TokenArray invalid = {
      .tokens = invalid_data,
      .count = 1,
  };

  uint8_t *serialized = NULL;
  size_t serialized_length = 0;

  CHECK(!cpack_serialize_tokens(&invalid, &serialized, &serialized_length),
        "Reject zero match offset");

  printf("PASS: Invalid match\n");
  return 1;
}

static int test_malformed_streams(void) {
  const uint8_t truncated_literal[] = {0};
  const uint8_t truncated_match[] = {1, 1, 0, 3};
  const uint8_t invalid_type[] = {2};
  const uint8_t invalid_offset[] = {1, 0, 0, 3, 0};
  const uint8_t invalid_length[] = {1, 1, 0, 2, 0};

  LZ77TokenArray decoded = {0};

  CHECK(!cpack_deserialize_tokens(truncated_literal, sizeof(truncated_literal),
                                  &decoded),
        "Reject truncated literal");

  CHECK(!cpack_deserialize_tokens(truncated_match, sizeof(truncated_match),
                                  &decoded),
        "Reject truncated match");

  CHECK(!cpack_deserialize_tokens(invalid_type, sizeof(invalid_type), &decoded),
        "Reject unknown token type");

  CHECK(!cpack_deserialize_tokens(invalid_offset, sizeof(invalid_offset),
                                  &decoded),
        "Reject invalid offset");

  CHECK(!cpack_deserialize_tokens(invalid_length, sizeof(invalid_length),
                                  &decoded),
        "Reject invalid match length");

  printf("PASS: Malformed streams\n");
  return 1;
}

static int test_null_arguments(void) {
  LZ77TokenArray tokens = {0};
  uint8_t *output = NULL;
  size_t output_length = 0;

  CHECK(!cpack_serialize_tokens(NULL, &output, &output_length),
        "Reject null token array");

  CHECK(!cpack_serialize_tokens(&tokens, NULL, &output_length),
        "Reject null output");

  CHECK(!cpack_deserialize_tokens(NULL, 1, &tokens),
        "Reject null input with nonzero length");

  CHECK(!cpack_deserialize_tokens(NULL, 0, NULL), "Reject null destination");

  printf("PASS: Null arguments\n");
  return 1;
}

int main(void) {
  if (!test_literal_tokens() || !test_match_tokens() || !test_empty_tokens() ||
      !test_invalid_token_type() || !test_invalid_match() ||
      !test_malformed_streams() || !test_null_arguments()) {
    return EXIT_FAILURE;
  }

  printf("\nALL TOKEN CODEC TESTS PASSED\n");

  return EXIT_SUCCESS;
}
