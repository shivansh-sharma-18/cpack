#include "pipeline.h"

#include "bitio.h"
#include "huffman.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cpack_serialize_tokens(const LZ77TokenArray *tokens, uint8_t **output,
                           size_t *output_length) {
  if (tokens == NULL || output == NULL || output_length == NULL ||
      (tokens->count > 0 && tokens->tokens == NULL)) {
    return 0;
  }

  *output = NULL;
  *output_length = 0;

  if (tokens->count > SIZE_MAX / 5) {
    return 0;
  }

  size_t capacity = tokens->count * 5;

  uint8_t *buffer = malloc(capacity == 0 ? 1 : capacity);

  if (buffer == NULL) {
    return 0;
  }

  size_t position = 0;

  for (size_t i = 0; i < tokens->count; i++) {
    const LZ77Token *token = &tokens->tokens[i];

    if (token->is_match == 0) {
      buffer[position++] = 0;
      buffer[position++] = token->literal;

    } else if (token->is_match == 1) {
      if (token->offset == 0 || token->offset > LZ77_WINDOW_SIZE ||
          token->length < LZ77_MIN_MATCH || token->length > LZ77_MAX_MATCH) {
        free(buffer);
        return 0;
      }

      buffer[position++] = 1;

      buffer[position++] = (uint8_t)(token->offset & 0xFF);
      buffer[position++] = (uint8_t)(token->offset >> 8);

      buffer[position++] = (uint8_t)(token->length & 0xFF);
      buffer[position++] = (uint8_t)(token->length >> 8);

    } else {
      free(buffer);
      return 0;
    }
  }

  *output = buffer;
  *output_length = position;

  return 1;
}

int cpack_deserialize_tokens(const uint8_t *data, size_t length,
                             LZ77TokenArray *tokens) {
  if (tokens == NULL || (length > 0 && data == NULL)) {
    return 0;
  }

  tokens->tokens = NULL;
  tokens->count = 0;

  if (length == 0) {
    return 1;
  }

  if (length > SIZE_MAX / sizeof(LZ77Token)) {
    return 0;
  }

  LZ77Token *result = malloc(length * sizeof(LZ77Token));

  if (result == NULL) {
    return 0;
  }

  size_t position = 0;
  size_t count = 0;

  while (position < length) {
    uint8_t type = data[position++];

    if (type == 0) {
      if (length - position < 1) {
        free(result);
        return 0;
      }

      result[count].is_match = 0;
      result[count].literal = data[position++];
      result[count].offset = 0;
      result[count].length = 0;

    } else if (type == 1) {
      if (length - position < 4) {
        free(result);
        return 0;
      }

      result[count].is_match = 1;
      result[count].literal = 0;

      result[count].offset =
          (uint16_t)data[position] | ((uint16_t)data[position + 1] << 8);

      position += 2;

      result[count].length =
          (uint16_t)data[position] | ((uint16_t)data[position + 1] << 8);

      position += 2;

      if (result[count].offset == 0 ||
          result[count].offset > LZ77_WINDOW_SIZE ||
          result[count].length < LZ77_MIN_MATCH ||
          result[count].length > LZ77_MAX_MATCH) {
        free(result);
        return 0;
      }

    } else {
      free(result);
      return 0;
    }

    count++;
  }

  tokens->tokens = result;
  tokens->count = count;

  return 1;
}

int cpack_pipeline_round_trip(const uint8_t *input, size_t input_length,
                              uint8_t **output, size_t *output_length) {
  if (output == NULL || output_length == NULL ||
      (input_length > 0 && input == NULL)) {
    return 0;
  }

  *output = NULL;
  *output_length = 0;

  if (input_length == 0) {
    uint8_t *empty = malloc(1);

    if (empty == NULL) {
      return 0;
    }

    *output = empty;
    return 1;
  }

  LZ77TokenArray tokens = {0};

  uint8_t *serialized = NULL;
  size_t serialized_length = 0;

  uint8_t *decoded_serialized = NULL;

  LZ77TokenArray decoded_tokens = {0};

  HuffmanNode *root = NULL;
  FILE *temp = NULL;

  int success = 0;

  if (!lz77_compress(input, input_length, &tokens)) {
    goto cleanup;
  }

  if (!cpack_serialize_tokens(&tokens, &serialized, &serialized_length)) {
    goto cleanup;
  }

  uint64_t frequencies[HUFFMAN_SYMBOLS];

  huffman_count_frequencies(serialized, serialized_length, frequencies);

  root = huffman_build_tree(frequencies);

  if (root == NULL) {
    goto cleanup;
  }

  HuffmanCode codes[HUFFMAN_SYMBOLS];

  if (!huffman_generate_codes(root, codes)) {
    goto cleanup;
  }

  temp = tmpfile();

  if (temp == NULL) {
    goto cleanup;
  }

  BitWriter writer;

  bitwriter_init(&writer, temp);

  if (!huffman_encode(serialized, serialized_length, codes, &writer)) {
    goto cleanup;
  }

  if (!bitwriter_flush(&writer)) {
    goto cleanup;
  }

  if (fseek(temp, 0, SEEK_SET) != 0) {
    goto cleanup;
  }

  BitReader reader;

  bitreader_init(&reader, temp);

  decoded_serialized = huffman_decode(&reader, root, serialized_length);

  if (decoded_serialized == NULL) {
    goto cleanup;
  }

  if (!cpack_deserialize_tokens(decoded_serialized, serialized_length,
                                &decoded_tokens)) {
    goto cleanup;
  }

  uint8_t *restored = lz77_decompress(decoded_tokens.tokens,
                                      decoded_tokens.count, input_length);

  if (restored == NULL) {
    goto cleanup;
  }

  *output = restored;
  *output_length = input_length;

  success = 1;

cleanup:
  if (temp != NULL) {
    fclose(temp);
  }

  huffman_free_tree(root);

  lz77_free_tokens(&tokens);
  lz77_free_tokens(&decoded_tokens);

  free(serialized);
  free(decoded_serialized);

  return success;
}