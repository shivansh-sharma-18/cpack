#include "lz77.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int find_longest_match(const uint8_t *input, size_t input_length,
                              size_t position, uint16_t *best_offset,
                              uint16_t *best_length) {
  *best_offset = 0;
  *best_length = 0;

  size_t window_start = 0;

  if (position > LZ77_WINDOW_SIZE) {
    window_start = position - LZ77_WINDOW_SIZE;
  }

  size_t max_length = input_length - position;

  if (max_length > LZ77_MAX_MATCH) {
    max_length = LZ77_MAX_MATCH;
  }

  for (size_t candidate = window_start; candidate < position; candidate++) {
    size_t offset = position - candidate;
    size_t length = 0;

    while (length < max_length &&
           input[candidate + length] == input[position + length]) {
      length++;
    }

    if (length > *best_length) {
      *best_length = (uint16_t)length;
      *best_offset = (uint16_t)offset;
    }
  }

  return *best_length >= LZ77_MIN_MATCH;
}

int lz77_compress(const uint8_t *input, size_t input_length,
                  LZ77TokenArray *result) {
  if (result == NULL) {
    return 0;
  }

  result->tokens = NULL;
  result->count = 0;

  if (input_length == 0) {
    return 1;
  }

  if (input == NULL) {
    return 0;
  }

  if (input_length > SIZE_MAX / sizeof(LZ77Token)) {
    return 0;
  }

  LZ77Token *tokens = malloc(input_length * sizeof(LZ77Token));

  if (tokens == NULL) {
    return 0;
  }

  size_t position = 0;
  size_t token_count = 0;

  while (position < input_length) {
    uint16_t offset = 0;
    uint16_t length = 0;

    int found =
        find_longest_match(input, input_length, position, &offset, &length);

    if (found) {
      tokens[token_count].is_match = 1;
      tokens[token_count].literal = 0;
      tokens[token_count].offset = offset;
      tokens[token_count].length = length;

      position += length;
    } else {
      tokens[token_count].is_match = 0;
      tokens[token_count].literal = input[position];
      tokens[token_count].offset = 0;
      tokens[token_count].length = 0;

      position++;
    }

    token_count++;
  }

  result->tokens = tokens;
  result->count = token_count;

  return 1;
}

void lz77_free_tokens(LZ77TokenArray *array) {
  if (array == NULL) {
    return;
  }

  free(array->tokens);

  array->tokens = NULL;
  array->count = 0;
}

uint8_t *lz77_decompress(const LZ77Token *tokens, size_t token_count,
                         size_t original_length) {
  if (token_count > 0 && tokens == NULL) {
    return NULL;
  }

  if (original_length == 0) {
    return malloc(1);
  }

  uint8_t *output = malloc(original_length);

  if (output == NULL) {
    return NULL;
  }

  size_t output_position = 0;

  for (size_t i = 0; i < token_count; i++) {
    if (tokens[i].is_match == 0) {
      if (output_position >= original_length) {
        free(output);
        return NULL;
      }

      output[output_position] = tokens[i].literal;
      output_position++;
    } else {
      size_t offset = tokens[i].offset;
      size_t length = tokens[i].length;

      if (offset == 0 || offset > output_position || length < LZ77_MIN_MATCH ||
          length > LZ77_MAX_MATCH ||
          length > original_length - output_position) {
        free(output);
        return NULL;
      }

      for (size_t j = 0; j < length; j++) {
        output[output_position] = output[output_position - offset];

        output_position++;
      }
    }
  }

  if (output_position != original_length) {
    free(output);
    return NULL;
  }

  return output;
}
