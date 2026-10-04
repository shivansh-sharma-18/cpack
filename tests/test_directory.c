#include "directory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  CpackFileList list;

  if (!cpack_collect_files("test_directory_data", &list)) {
    fprintf(stderr, "FAIL: Directory traversal failed\n");
    return 1;
  }

  const char *expected[] = {"hello.txt", "src/main.c", "src/utils/helper.txt"};

  if (list.count != 3) {
    fprintf(stderr, "FAIL: Expected 3 files, found %zu\n", list.count);

    cpack_free_file_list(&list);
    return 1;
  }

  for (size_t i = 0; i < 3; i++) {
    int found = 0;

    for (size_t j = 0; j < list.count; j++) {
      if (strcmp(expected[i], list.paths[j]) == 0) {
        found = 1;
        break;
      }
    }

    if (!found) {
      fprintf(stderr, "FAIL: Missing file %s\n", expected[i]);
      cpack_free_file_list(&list);
      return 1;
    }
  }

  printf("Discovered files:\n");

  for (size_t i = 0; i < list.count; i++) {
    printf("  %s\n", list.paths[i]);
  }

  printf("\nPASS: Directory traversal test\n");

  cpack_free_file_list(&list);

  return 0;
}
