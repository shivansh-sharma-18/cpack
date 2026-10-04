#include "archive.h"

#include "crc32.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CPACK_VERSION_1 1
#define CPACK_VERSION_2 2
#define CPACK_VERSION_3 3

#define CPACK_MAX_ENTRIES 100000
#define CPACK_MAX_PATH_LENGTH 4096

#define CPACK_ENTRY_FILE 0
#define CPACK_ENTRY_DIRECTORY 1

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

static int frequencies_are_zero(const uint64_t frequencies[HUFFMAN_SYMBOLS]) {
  for (size_t i = 0; i < HUFFMAN_SYMBOLS; i++) {
    if (frequencies[i] != 0) {
      return 0;
    }
  }
  return 1;
}

static int validate_archive_buffer(const CpackCompressedBuffer *compressed) {
  if (compressed == NULL) {
    return 0;
  }

  if (compressed->original_length > SIZE_MAX ||
      compressed->token_stream_length > SIZE_MAX) {
    return 0;
  }

  if (compressed->mode != CPACK_MODE_COMPRESSED &&
      compressed->mode != CPACK_MODE_RAW) {
    return 0;
  }

  if (compressed->original_length == 0) {
    return compressed->compressed_size == 0 && compressed->bit_length == 0 &&
           compressed->token_stream_length == 0 &&
           frequencies_are_zero(compressed->frequencies);
  }

  if (compressed->data == NULL || compressed->compressed_size == 0) {
    return 0;
  }

  if (compressed->mode == CPACK_MODE_RAW) {
    return compressed->original_length == compressed->compressed_size &&
           compressed->bit_length == 0 &&
           compressed->token_stream_length == 0 &&
           frequencies_are_zero(compressed->frequencies);
  }

  if (compressed->bit_length == 0 || compressed->token_stream_length == 0 ||
      compressed->compressed_size > UINT64_MAX / 8) {
    return 0;
  }

  uint64_t available_bits = (uint64_t)compressed->compressed_size * 8;
  if (compressed->bit_length > available_bits) {
    return 0;
  }

  uint64_t expected_size =
      compressed->bit_length / 8 + (compressed->bit_length % 8 != 0);
  if (expected_size != compressed->compressed_size) {
    return 0;
  }

  uint64_t frequency_total = 0;
  for (size_t i = 0; i < HUFFMAN_SYMBOLS; i++) {
    if (frequency_total > UINT64_MAX - compressed->frequencies[i]) {
      return 0;
    }
    frequency_total += compressed->frequencies[i];
  }

  return frequency_total == compressed->token_stream_length;
}

int cpack_archive_write(const char *path,
                        const CpackCompressedBuffer *compressed) {
  if (path == NULL || !validate_archive_buffer(compressed)) {
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

  if (success && !write_u32(file, CPACK_VERSION_2)) {
    success = 0;
  }

  if (success && !write_u32(file, (uint32_t)compressed->mode)) {
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
  uint32_t mode = CPACK_MODE_COMPRESSED;

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

  if (success && version != CPACK_VERSION_1 && version != CPACK_VERSION_2) {
    success = 0;
  }

  if (success && version == CPACK_VERSION_2) {
    if (!read_u32(file, &mode)) {
      success = 0;
    }
  }

  if (success && mode != CPACK_MODE_COMPRESSED && mode != CPACK_MODE_RAW) {
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
    compressed->mode = (CpackStorageMode)mode;

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

  if (success && !validate_archive_buffer(compressed)) {
    success = 0;
  }

  if (!success) {
    cpack_free_compressed_buffer(compressed);
    return 0;
  }

  return 1;
}

static int validate_archive_path(const char *path) {
  if (path == NULL || path[0] == '\0') {
    return 0;
  }

  if (path[0] == '/' || path[0] == '\\') {
    return 0;
  }

  size_t length = strlen(path);
  if (length > CPACK_MAX_PATH_LENGTH) {
    return 0;
  }

  if (path[length - 1] == '/') {
    return 0;
  }

  const char *component = path;

  for (const char *p = path;; p++) {
    if (*p == '\\') {
      return 0;
    }

    if (*p == '/' || *p == '\0') {
      size_t component_length = (size_t)(p - component);

      if (component_length == 0) {
        return 0;
      }

      if (component_length == 1 && component[0] == '.') {
        return 0;
      }

      if (component_length == 2 && component[0] == '.' && component[1] == '.') {
        return 0;
      }

      if (*p == '\0') {
        break;
      }

      component = p + 1;
    }
  }

  return 1;
}

static int validate_v3_entry(const CpackArchiveEntry *entry) {
  if (entry == NULL || !validate_archive_path(entry->path)) {
    return 0;
  }

  if (entry->type == CPACK_ENTRY_DIRECTORY) {
    return entry->original_length == 0 && entry->checksum == 0 &&
           entry->compressed.data == NULL &&
           entry->compressed.compressed_size == 0;
  }

  if (entry->type != CPACK_ENTRY_FILE) {
    return 0;
  }

  if (!validate_archive_buffer(&entry->compressed)) {
    return 0;
  }

  return entry->original_length == entry->compressed.original_length;
}

int cpack_archive_write_v3(const char *path, const CpackArchive *archive) {
  if (path == NULL || archive == NULL || archive->count > CPACK_MAX_ENTRIES ||
      (archive->count > 0 && archive->entries == NULL)) {
    return 0;
  }

  for (size_t i = 0; i < archive->count; i++) {
    if (!validate_v3_entry(&archive->entries[i])) {
      return 0;
    }
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

  if (success && !write_u32(file, CPACK_VERSION_3)) {
    success = 0;
  }

  if (success && !write_u32(file, (uint32_t)archive->count)) {
    success = 0;
  }

  for (size_t i = 0; success && i < archive->count; i++) {
    const CpackArchiveEntry *entry = &archive->entries[i];
    uint32_t path_length = (uint32_t)strlen(entry->path);

    if (!write_u32(file, entry->type) || !write_u32(file, path_length) ||
        fwrite(entry->path, 1, path_length, file) != path_length ||
        !write_u64(file, entry->original_length) ||
        !write_u64(file, entry->checksum)) {
      success = 0;
      break;
    }

    if (entry->type == CPACK_ENTRY_DIRECTORY) {
      continue;
    }

    const CpackCompressedBuffer *compressed = &entry->compressed;

    if (!write_u32(file, (uint32_t)compressed->mode) ||
        !write_u64(file, (uint64_t)compressed->compressed_size) ||
        !write_u64(file, compressed->bit_length) ||
        !write_u64(file, compressed->token_stream_length)) {
      success = 0;
      break;
    }

    for (size_t j = 0; j < HUFFMAN_SYMBOLS; j++) {
      if (!write_u64(file, compressed->frequencies[j])) {
        success = 0;
        break;
      }
    }

    if (!success) {
      break;
    }

    if (compressed->compressed_size > 0 &&
        fwrite(compressed->data, 1, compressed->compressed_size, file) !=
            compressed->compressed_size) {
      success = 0;
      break;
    }
  }

  if (fclose(file) != 0) {
    success = 0;
  }

  return success;
}

int cpack_archive_read_v3(const char *path, CpackArchive *archive) {
  if (path == NULL || archive == NULL) {
    return 0;
  }

  *archive = (CpackArchive){0};

  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    return 0;
  }

  int success = 1;
  uint8_t magic[4];
  uint32_t version = 0;
  uint32_t entry_count = 0;

  if (fread(magic, 1, sizeof(magic), file) != sizeof(magic)) {
    success = 0;
  }

  if (success && memcmp(magic, CPACK_MAGIC, sizeof(CPACK_MAGIC)) != 0) {
    success = 0;
  }

  if (success && !read_u32(file, &version)) {
    success = 0;
  }

  if (success && version != CPACK_VERSION_3) {
    success = 0;
  }

  if (success && !read_u32(file, &entry_count)) {
    success = 0;
  }

  if (success && entry_count > CPACK_MAX_ENTRIES) {
    success = 0;
  }

  if (success && entry_count > 0) {
    archive->entries = calloc(entry_count, sizeof(CpackArchiveEntry));
    if (archive->entries == NULL) {
      success = 0;
    } else {
      archive->capacity = entry_count;
    }
  }

  for (uint32_t i = 0; success && i < entry_count; i++) {
    CpackArchiveEntry *entry = &archive->entries[i];
    uint32_t type = 0;
    uint32_t path_length = 0;

    if (!read_u32(file, &type) || !read_u32(file, &path_length)) {
      success = 0;
      break;
    }

    if (path_length == 0 || path_length > CPACK_MAX_PATH_LENGTH) {
      success = 0;
      break;
    }

    entry->path = malloc((size_t)path_length + 1);
    if (entry->path == NULL) {
      success = 0;
      break;
    }

    if (fread(entry->path, 1, path_length, file) != path_length) {
      success = 0;
      break;
    }

    entry->path[path_length] = '\0';
    archive->count++;

    if (memchr(entry->path, '\0', path_length) != NULL ||
        !validate_archive_path(entry->path)) {
      success = 0;
      break;
    }

    entry->type = type;

    if (!read_u64(file, &entry->original_length) ||
        !read_u64(file, &entry->checksum)) {
      success = 0;
      break;
    }

    if (entry->type == CPACK_ENTRY_DIRECTORY) {
      if (entry->original_length != 0 || entry->checksum != 0) {
        success = 0;
      }
      continue;
    }

    if (entry->type != CPACK_ENTRY_FILE) {
      success = 0;
      break;
    }

    CpackCompressedBuffer *compressed = &entry->compressed;
    uint32_t mode = 0;
    uint64_t compressed_size = 0;

    if (!read_u32(file, &mode) || !read_u64(file, &compressed_size) ||
        !read_u64(file, &compressed->bit_length) ||
        !read_u64(file, &compressed->token_stream_length)) {
      success = 0;
      break;
    }

    if (compressed_size > SIZE_MAX) {
      success = 0;
      break;
    }

    compressed->mode = (CpackStorageMode)mode;
    compressed->compressed_size = (size_t)compressed_size;
    compressed->original_length = entry->original_length;

    for (size_t j = 0; j < HUFFMAN_SYMBOLS; j++) {
      if (!read_u64(file, &compressed->frequencies[j])) {
        success = 0;
        break;
      }
    }

    if (!success) {
      break;
    }

    if (compressed->compressed_size > 0) {
      compressed->data = malloc(compressed->compressed_size);
      if (compressed->data == NULL) {
        success = 0;
        break;
      }

      if (fread(compressed->data, 1, compressed->compressed_size, file) !=
          compressed->compressed_size) {
        success = 0;
        break;
      }
    }

    if (!validate_archive_buffer(compressed)) {
      success = 0;
      break;
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
    cpack_free_archive(archive);
    return 0;
  }

  return 1;
}

void cpack_free_archive(CpackArchive *archive) {
  if (archive == NULL) {
    return;
  }

  for (size_t i = 0; i < archive->count; i++) {
    free(archive->entries[i].path);
    cpack_free_compressed_buffer(&archive->entries[i].compressed);
  }

  free(archive->entries);
  *archive = (CpackArchive){0};
}
