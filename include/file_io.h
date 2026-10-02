#ifndef FILE_IO_H
#define FILE_IO_H

#include <stddef.h>
#include <stdint.h>

int cpack_read_file(const char *path, uint8_t **data, size_t *length);

int cpack_write_file(const char *path, const uint8_t *data, size_t length);

#endif
