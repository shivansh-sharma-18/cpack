#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

#include "compress.h"

#define CPACK_ENTRY_FILE 0
#define CPACK_ENTRY_DIRECTORY 1

typedef struct {
  char *path;
  uint32_t type;
  uint64_t original_length;
  uint64_t checksum;
  CpackCompressedBuffer compressed;
} CpackArchiveEntry;

typedef struct {
  CpackArchiveEntry *entries;
  size_t count;
  size_t capacity;
} CpackArchive;

int cpack_archive_write(const char *path,
                        const CpackCompressedBuffer *compressed);
int cpack_archive_read(const char *path, CpackCompressedBuffer *compressed);

int cpack_archive_write_v3(const char *path, const CpackArchive *archive);
int cpack_archive_read_v3(const char *path, CpackArchive *archive);

void cpack_free_archive(CpackArchive *archive);

#endif
