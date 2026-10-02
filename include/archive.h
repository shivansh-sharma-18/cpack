#ifndef ARCHIVE_H
#define ARCHIVE_H

#include "compress.h"

int cpack_archive_write(const char *path,
                        const CpackCompressedBuffer *compressed);

int cpack_archive_read(const char *path, CpackCompressedBuffer *compressed);

#endif
