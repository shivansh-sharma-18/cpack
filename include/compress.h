#ifndef COMPRESS_H
#define COMPRESS_H

#include <stddef.h>
#include <stdint.h>

#include "huffman.h"

typedef struct {
  uint8_t *data;
  size_t compressed_size;

  uint64_t bit_length;
  uint64_t original_length;
  uint64_t token_stream_length;

  uint64_t frequencies[HUFFMAN_SYMBOLS];
} CpackCompressedBuffer;

int cpack_compress_buffer(const uint8_t *input, size_t input_length,
                          CpackCompressedBuffer *result);

int cpack_decompress_buffer(const CpackCompressedBuffer *compressed,
                            uint8_t **output, size_t *output_length);

void cpack_free_compressed_buffer(CpackCompressedBuffer *compressed);

#endif
