#include "file_io.h"

#include <stdio.h>
#include <stdlib.h>

int cpack_read_file(const char *path, uint8_t **data, size_t *length) {
  if (path == NULL || data == NULL || length == NULL) {
    return 0;
  }

  *data = NULL;
  *length = 0;

  FILE *file = fopen(path, "rb");

  if (file == NULL) {
    return 0;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return 0;
  }

  long file_size = ftell(file);

  if (file_size < 0) {
    fclose(file);
    return 0;
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return 0;
  }

  size_t size = (size_t)file_size;

  uint8_t *buffer = NULL;

  if (size > 0) {
    buffer = malloc(size);

    if (buffer == NULL) {
      fclose(file);
      return 0;
    }

    if (fread(buffer, 1, size, file) != size) {
      free(buffer);
      fclose(file);
      return 0;
    }
  }

  if (fclose(file) != 0) {
    free(buffer);
    return 0;
  }

  *data = buffer;
  *length = size;

  return 1;
}

int cpack_write_file(const char *path, const uint8_t *data, size_t length) {
  if (path == NULL || (data == NULL && length > 0)) {
    return 0;
  }

  FILE *file = fopen(path, "wb");

  if (file == NULL) {
    return 0;
  }

  int success = 1;

  if (length > 0 && fwrite(data, 1, length, file) != length) {
    success = 0;
  }

  if (fclose(file) != 0) {
    success = 0;
  }

  return success;
}
