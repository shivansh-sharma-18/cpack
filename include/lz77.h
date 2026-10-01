#ifndef LZ77_H
#define LZ77_H

#include <stddef.h>
#include <stdint.h>

#define LZ77_WINDOW_SIZE 4096
#define LZ77_MIN_MATCH 3
#define LZ77_MAX_MATCH 258

typedef struct {
  uint8_t is_match;
  uint8_t literal;
  uint16_t offset;
  uint16_t length;
} LZ77Token;

typedef struct {
  LZ77Token *tokens;
  size_t count;
} LZ77TokenArray;

int lz77_compress(const uint8_t *input, size_t input_length,
                  LZ77TokenArray *result);

uint8_t *lz77_decompress(const LZ77Token *tokens, size_t token_count,
                         size_t original_length);

void lz77_free_tokens(LZ77TokenArray *array);

#endif