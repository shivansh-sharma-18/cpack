
#include "compress.h"

#include "bitio.h"
#include "huffman.h"
#include "lz77.h"
#include "token_codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int calculate_bit_length(const uint64_t frequencies[HUFFMAN_SYMBOLS],
                                const HuffmanCode codes[HUFFMAN_SYMBOLS],
                                uint64_t *bit_length) {

  if (frequencies == NULL || codes == NULL || bit_length == NULL) {
    return 0;
  }

  uint64_t total = 0;

  for (size_t i = 0; i < HUFFMAN_SYMBOLS; i++) {
    if (frequencies[i] == 0) {
      continue;
    }

    uint64_t code_length = codes[i].length;

    if (code_length == 0) {
      return 0;
    }

    if (frequencies[i] > UINT64_MAX / code_length) {
      return 0;
    }

    uint64_t contribution = frequencies[i] * code_length;

    if (total > UINT64_MAX - contribution) {
      return 0;
    }

    total += contribution;
  }

  *bit_length = total;

  return 1;
}

static int validate_frequencies(const uint64_t frequencies[HUFFMAN_SYMBOLS],
                                uint64_t expected_total) {

  uint64_t total = 0;

  for (size_t i = 0; i < HUFFMAN_SYMBOLS; i++) {
    if (total > UINT64_MAX - frequencies[i]) {
      return 0;
    }

    total += frequencies[i];
  }

  return total == expected_total;
}

static int
validate_compressed_metadata(const CpackCompressedBuffer *compressed) {

  if (compressed == NULL) {
    return 0;
  }

  if (compressed->original_length > SIZE_MAX ||
      compressed->token_stream_length > SIZE_MAX) {
    return 0;
  }

  if (compressed->original_length == 0) {
    if (compressed->compressed_size != 0 || compressed->bit_length != 0 ||
        compressed->token_stream_length != 0) {
      return 0;
    }

    for (size_t i = 0; i < HUFFMAN_SYMBOLS; i++) {
      if (compressed->frequencies[i] != 0) {
        return 0;
      }
    }

    return 1;
  }

  if (compressed->data == NULL || compressed->compressed_size == 0 ||
      compressed->bit_length == 0 || compressed->token_stream_length == 0) {
    return 0;
  }

  if (compressed->compressed_size > UINT64_MAX / 8) {
    return 0;
  }

  uint64_t available_bits = (uint64_t)compressed->compressed_size * 8;

  if (compressed->bit_length > available_bits) {
    return 0;
  }

  uint64_t expected_payload_size =
      compressed->bit_length / 8 + (compressed->bit_length % 8 != 0);

  if (expected_payload_size != compressed->compressed_size) {
    return 0;
  }

  if (!validate_frequencies(compressed->frequencies,
                            compressed->token_stream_length)) {
    return 0;
  }

  return 1;
}

static int validate_padding_bits(const CpackCompressedBuffer *compressed) {

  unsigned int remainder = (unsigned int)(compressed->bit_length % 8);

  if (remainder == 0) {
    return 1;
  }

  uint8_t last_byte = compressed->data[compressed->compressed_size - 1];

  uint8_t padding_mask = (uint8_t)(0xFFU << remainder);

  return (last_byte & padding_mask) == 0;
}

int cpack_compress_buffer(const uint8_t *input, size_t input_length,
                          CpackCompressedBuffer *result) {

  if (result == NULL || (input_length > 0 && input == NULL)) {
    return 0;
  }

  memset(result, 0, sizeof(*result));

  result->original_length = (uint64_t)input_length;

  if (input_length == 0) {
    result->data = malloc(1);

    if (result->data == NULL) {
      return 0;
    }

    result->compressed_size = 0;

    return 1;
  }

  LZ77TokenArray tokens = {0};

  uint8_t *serialized = NULL;
  size_t serialized_length = 0;

  HuffmanNode *root = NULL;
  FILE *temp = NULL;

  int success = 0;

  if (!lz77_compress(input, input_length, &tokens)) {
    goto cleanup;
  }

  if (!cpack_serialize_tokens(&tokens, &serialized, &serialized_length)) {
    goto cleanup;
  }

  if (serialized_length == 0) {
    goto cleanup;
  }

  result->token_stream_length = (uint64_t)serialized_length;

  huffman_count_frequencies(serialized, serialized_length, result->frequencies);

  root = huffman_build_tree(result->frequencies);

  if (root == NULL) {
    goto cleanup;
  }

  HuffmanCode codes[HUFFMAN_SYMBOLS];

  if (!huffman_generate_codes(root, codes)) {
    goto cleanup;
  }

  if (!calculate_bit_length(result->frequencies, codes, &result->bit_length)) {
    goto cleanup;
  }

  uint64_t payload_size =
      result->bit_length / 8 + (result->bit_length % 8 != 0);

  if (payload_size > SIZE_MAX) {
    goto cleanup;
  }

  result->compressed_size = (size_t)payload_size;

  result->data =
      malloc(result->compressed_size == 0 ? 1 : result->compressed_size);

  if (result->data == NULL) {
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

  if (fflush(temp) != 0) {
    goto cleanup;
  }

  if (fseek(temp, 0, SEEK_SET) != 0) {
    goto cleanup;
  }

  if (result->compressed_size > 0) {
    size_t bytes_read = fread(result->data, 1, result->compressed_size, temp);

    if (bytes_read != result->compressed_size) {
      goto cleanup;
    }
  }

  success = 1;

cleanup:

  if (temp != NULL) {
    fclose(temp);
  }

  huffman_free_tree(root);
  lz77_free_tokens(&tokens);
  free(serialized);

  if (!success) {
    cpack_free_compressed_buffer(result);
  }

  return success;
}

int cpack_decompress_buffer(const CpackCompressedBuffer *compressed,
                            uint8_t **output, size_t *output_length) {

  if (compressed == NULL || output == NULL || output_length == NULL) {
    return 0;
  }

  *output = NULL;
  *output_length = 0;

  if (!validate_compressed_metadata(compressed)) {
    return 0;
  }

  size_t original_length = (size_t)compressed->original_length;

  size_t token_stream_length = (size_t)compressed->token_stream_length;

  if (original_length == 0) {
    uint8_t *empty = malloc(1);

    if (empty == NULL) {
      return 0;
    }

    *output = empty;
    *output_length = 0;

    return 1;
  }

  HuffmanNode *root = huffman_build_tree(compressed->frequencies);

  if (root == NULL) {
    return 0;
  }

  FILE *temp = NULL;

  uint8_t *decoded_serialized = NULL;
  LZ77TokenArray tokens = {0};

  int success = 0;

  temp = tmpfile();

  if (temp == NULL) {
    goto cleanup;
  }

  if (fwrite(compressed->data, 1, compressed->compressed_size, temp) !=
      compressed->compressed_size) {
    goto cleanup;
  }

  if (fflush(temp) != 0) {
    goto cleanup;
  }

  if (fseek(temp, 0, SEEK_SET) != 0) {
    goto cleanup;
  }

  BitReader reader;

  bitreader_init_limited(&reader, temp, compressed->bit_length);

  uint64_t bits_consumed = 0;

  decoded_serialized = huffman_decode_limited(
      &reader, root, token_stream_length, &bits_consumed);

  if (decoded_serialized == NULL) {
    goto cleanup;
  }

  if (bits_consumed != compressed->bit_length) {
    goto cleanup;
  }

  if (!validate_padding_bits(compressed)) {
    goto cleanup;
  }

  if (!cpack_deserialize_tokens(decoded_serialized, token_stream_length,
                                &tokens)) {
    goto cleanup;
  }

  uint8_t *restored =
      lz77_decompress(tokens.tokens, tokens.count, original_length);

  if (restored == NULL) {
    goto cleanup;
  }

  *output = restored;
  *output_length = original_length;

  success = 1;

cleanup:

  if (temp != NULL) {
    fclose(temp);
  }

  huffman_free_tree(root);
  lz77_free_tokens(&tokens);
  free(decoded_serialized);

  return success;
}

void cpack_free_compressed_buffer(CpackCompressedBuffer *compressed) {

  if (compressed == NULL) {
    return;
  }

  free(compressed->data);

  memset(compressed, 0, sizeof(*compressed));
}
