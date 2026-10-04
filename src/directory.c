#include "directory.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define INITIAL_CAPACITY 16

static int add_file(CpackFileList *list, const char *path) {
  if (list->count == list->capacity) {
    size_t new_capacity =
        list->capacity == 0 ? INITIAL_CAPACITY : list->capacity * 2;

    char **new_paths = realloc(list->paths, new_capacity * sizeof(char *));

    if (new_paths == NULL) {
      return 0;
    }

    list->paths = new_paths;
    list->capacity = new_capacity;
  }

  char *copy = malloc(strlen(path) + 1);

  if (copy == NULL) {
    return 0;
  }

  strcpy(copy, path);

  list->paths[list->count++] = copy;

  return 1;
}

static int traverse_directory(const char *root, const char *relative_path,
                              CpackFileList *list) {
  char directory_path[4096];

  int length = snprintf(directory_path, sizeof(directory_path), "%s%s%s", root,
                        relative_path[0] == '\0' ? "" : "/", relative_path);

  if (length < 0 || (size_t)length >= sizeof(directory_path)) {
    return 0;
  }

  DIR *directory = opendir(directory_path);

  if (directory == NULL) {
    perror(directory_path);
    return 0;
  }

  struct dirent *entry;
  int success = 1;

  while ((entry = readdir(directory)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char child_relative[4096];
    char child_full[4096];

    int relative_length = snprintf(
        child_relative, sizeof(child_relative), "%s%s%s", relative_path,
        relative_path[0] == '\0' ? "" : "/", entry->d_name);

    int full_length =
        snprintf(child_full, sizeof(child_full), "%s/%s", root, child_relative);

    if (relative_length < 0 ||
        (size_t)relative_length >= sizeof(child_relative) || full_length < 0 ||
        (size_t)full_length >= sizeof(child_full)) {
      fprintf(stderr, "Path too long: %s\n", entry->d_name);
      success = 0;
      break;
    }

    struct stat file_info;

    if (lstat(child_full, &file_info) != 0) {
      perror(child_full);
      success = 0;
      break;
    }

    if (S_ISDIR(file_info.st_mode)) {
      if (!traverse_directory(root, child_relative, list)) {
        success = 0;
        break;
      }
    } else if (S_ISREG(file_info.st_mode)) {
      if (!add_file(list, child_relative)) {
        fprintf(stderr, "Memory allocation failed\n");
        success = 0;
        break;
      }
    }
  }

  closedir(directory);

  return success;
}

int cpack_collect_files(const char *directory, CpackFileList *list) {
  if (directory == NULL || list == NULL) {
    return 0;
  }

  list->paths = NULL;
  list->count = 0;
  list->capacity = 0;

  struct stat info;

  if (stat(directory, &info) != 0 || !S_ISDIR(info.st_mode)) {
    fprintf(stderr, "Invalid directory: %s\n", directory);
    return 0;
  }

  if (!traverse_directory(directory, "", list)) {
    cpack_free_file_list(list);
    return 0;
  }

  return 1;
}

void cpack_free_file_list(CpackFileList *list) {
  if (list == NULL) {
    return;
  }

  for (size_t i = 0; i < list->count; i++) {
    free(list->paths[i]);
  }

  free(list->paths);

  list->paths = NULL;
  list->count = 0;
  list->capacity = 0;
}
