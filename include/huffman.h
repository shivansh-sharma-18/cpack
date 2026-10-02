
#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stddef.h>
#include <stdint.h>

#include "bitio.h"

#define HUFFMAN_SYMBOLS 256
#define HUFFMAN_MAX_CODE_LENGTH 256

typedef struct HuffmanNode {
  uint8_t symbol;
  uint64_t frequency;

  struct HuffmanNode *left;
  struct HuffmanNode *right;

  uint32_t order;
} HuffmanNode;

typedef struct {
  uint8_t bits[HUFFMAN_MAX_CODE_LENGTH];
  uint16_t length;
} HuffmanCode;

void huffman_count_frequencies(const uint8_t *data, size_t length,
                               uint64_t frequencies[HUFFMAN_SYMBOLS]);

HuffmanNode *huffman_create_node(uint8_t symbol, uint64_t frequency);

HuffmanNode *huffman_build_tree(const uint64_t frequencies[HUFFMAN_SYMBOLS]);

void huffman_free_tree(HuffmanNode *root);

int huffman_generate_codes(const HuffmanNode *root,
                           HuffmanCode codes[HUFFMAN_SYMBOLS]);

int huffman_encode(const uint8_t *data, size_t length,
                   const HuffmanCode codes[HUFFMAN_SYMBOLS], BitWriter *writer);

uint8_t *huffman_decode(BitReader *reader, const HuffmanNode *root,
                        size_t original_length);

uint8_t *huffman_decode_limited(BitReader *reader, const HuffmanNode *root,
                                size_t original_length,
                                uint64_t *bits_consumed);

#endif
