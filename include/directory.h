#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <stddef.h>

typedef struct {
  char **paths;
  size_t count;
  size_t capacity;
} CpackFileList;

int cpack_collect_files(const char *directory, CpackFileList *list);

void cpack_free_file_list(CpackFileList *list);

#endif
