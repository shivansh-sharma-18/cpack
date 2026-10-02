#include "archive.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CPACK_VERSION 1
#define CPACK_HEADER_SIZE 36

static const uint8_t CPACK_MAGIC[4] = {'C', 'P', 'A', 'K'};

static int write_u32(FILE *file, uint32_t value) {
  for (int i = 0; i < 4; i++) {
    if (fputc((int)((value >> (i * 8)) & 0xFF), file) == EOF) {
      return 0;
    }
  }

  return 1;
}

static int write_u64(FILE *file, uint64_t value) {
  for (int i = 0; i < 8; i++) {
    if (fputc((int)((value >> (i * 8)) & 0xFF), file) == EOF) {
      return 0;
    }
  }

  return 1;
}

static int read_u32(FILE *file, uint32_t *value) {
  uint32_t result = 0;

  for (int i = 0; i < 4; i++) {
    int byte = fgetc(file);

    if (byte == EOF) {
      return 0;
    }

    result |= (uint32_t)(uint8_t)byte << (i * 8);
  }

  *value = result;
  return 1;
}

static int read_u64(FILE *file, uint64_t *value) {
  uint64_t result = 0;

  for (int i = 0; i < 8; i++) {
    int byte = fgetc(file);

    if (byte == EOF) {
      return 0;
    }

    result |= (uint64_t)(uint8_t)byte << (i * 8);
  }

  *value = result;
  return 1;
}

int cpack_archive_write(const char *path,
                        const CpackCompressedBuffer *compressed) {
  if (path == NULL || compressed == NULL) {
    return 0;
  }

  if (compressed->compressed_size > 0 && compressed->data == NULL) {
    return 0;
  }

  FILE *file = fopen(path, "wb");

  if (file == NULL) {
    return 0;
  }

  int success = 1;

  if (fwrite(CPACK_MAGIC, 1, sizeof(CPACK_MAGIC), file) !=
      sizeof(CPACK_MAGIC)) {
    success = 0;
  }

  if (success && !write_u32(file, CPACK_VERSION)) {
    success = 0;
  }

  if (success && !write_u64(file, compressed->original_length)) {
    success = 0;
  }

  if (success && !write_u64(file, (uint64_t)compressed->compressed_size)) {
    success = 0;
  }

  if (success && !write_u64(file, compressed->bit_length)) {
    success = 0;
  }

  if (success && !write_u64(file, compressed->token_stream_length)) {
    success = 0;
  }

  for (size_t i = 0; success && i < HUFFMAN_SYMBOLS; i++) {
    if (!write_u64(file, compressed->frequencies[i])) {
      success = 0;
    }
  }

  if (success && compressed->compressed_size > 0) {
    if (fwrite(compressed->data, 1, compressed->compressed_size, file) !=
        compressed->compressed_size) {
      success = 0;
    }
  }

  if (fclose(file) != 0) {
    success = 0;
  }

  return success;
}

int cpack_archive_read(const char *path, CpackCompressedBuffer *compressed) {
  if (path == NULL || compressed == NULL) {
    return 0;
  }

  *compressed = (CpackCompressedBuffer){0};

  FILE *file = fopen(path, "rb");

  if (file == NULL) {
    return 0;
  }

  int success = 1;
  uint8_t magic[4];
  uint32_t version = 0;

  if (fread(magic, 1, sizeof(magic), file) != sizeof(magic)) {
    success = 0;
  }

  if (success) {
    for (size_t i = 0; i < sizeof(magic); i++) {
      if (magic[i] != CPACK_MAGIC[i]) {
        success = 0;
        break;
      }
    }
  }

  if (success && !read_u32(file, &version)) {
    success = 0;
  }

  if (success && version != CPACK_VERSION) {
    success = 0;
  }

  uint64_t compressed_size = 0;

  if (success && !read_u64(file, &compressed->original_length)) {
    success = 0;
  }

  if (success && !read_u64(file, &compressed_size)) {
    success = 0;
  }

  if (success && !read_u64(file, &compressed->bit_length)) {
    success = 0;
  }

  if (success && !read_u64(file, &compressed->token_stream_length)) {
    success = 0;
  }

  for (size_t i = 0; success && i < HUFFMAN_SYMBOLS; i++) {
    if (!read_u64(file, &compressed->frequencies[i])) {
      success = 0;
    }
  }

  if (success && compressed_size > SIZE_MAX) {
    success = 0;
  }

  if (success) {
    compressed->compressed_size = (size_t)compressed_size;

    if (compressed->compressed_size > 0) {
      compressed->data = malloc(compressed->compressed_size);

      if (compressed->data == NULL) {
        success = 0;
      } else if (fread(compressed->data, 1, compressed->compressed_size,
                       file) != compressed->compressed_size) {
        success = 0;
      }
    }
  }

  if (success && fgetc(file) != EOF) {
    success = 0;
  }

  if (ferror(file)) {
    success = 0;
  }

  if (fclose(file) != 0) {
    success = 0;
  }

  if (!success) {
    cpack_free_compressed_buffer(compressed);
    return 0;
  }

  return 1;
}
