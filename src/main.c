#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "archive.h"
#include "compress.h"
#include "crc32.h"
#include "directory.h"
#include "file_io.h"

#define MAX_PATH_LENGTH 4096

static void print_usage(void) {
  printf("CPack - Custom Lossless Compressor and Archiver\n\n");
  printf("Usage:\n");
  printf("  cpack compress <input_file_or_folder> <output.cpk>\n");
  printf("  cpack decompress <input.cpk> <output_file_or_folder>\n");
  printf("  cpack list <archive.cpk>\n");
}

static int create_directory(const char *path) {
  struct stat info;

  if (stat(path, &info) == 0) {
    return S_ISDIR(info.st_mode);
  }

  if (mkdir(path, 0755) == 0) {
    return 1;
  }

  if (errno == EEXIST && stat(path, &info) == 0) {
    return S_ISDIR(info.st_mode);
  }

  perror(path);
  return 0;
}

static int create_parent_directories(const char *path) {
  char buffer[MAX_PATH_LENGTH];
  size_t length = strlen(path);

  if (length >= sizeof(buffer)) {
    fprintf(stderr, "Error: Path too long.\n");
    return 0;
  }

  strcpy(buffer, path);

  for (size_t i = 1; i < length; i++) {
    if (buffer[i] == '/') {
      buffer[i] = '\0';

      if (buffer[0] != '\0' && !create_directory(buffer)) {
        return 0;
      }

      buffer[i] = '/';
    }
  }

  return 1;
}

static int join_paths(char *result, size_t size, const char *first,
                      const char *second) {
  int length = snprintf(result, size, "%s/%s", first, second);
  return length >= 0 && (size_t)length < size;
}

static int compress_file(const char *input_path, const char *output_path) {
  uint8_t *input = NULL;
  size_t input_length = 0;
  CpackCompressedBuffer compressed = {0};

  if (!cpack_read_file(input_path, &input, &input_length)) {
    fprintf(stderr, "Error: Could not read input file.\n");
    return 1;
  }

  if (!cpack_compress_buffer(input, input_length, &compressed)) {
    fprintf(stderr, "Error: Compression failed.\n");
    free(input);
    return 1;
  }

  if (!cpack_archive_write(output_path, &compressed)) {
    fprintf(stderr, "Error: Could not write archive.\n");
    free(input);
    cpack_free_compressed_buffer(&compressed);
    return 1;
  }

  printf("Compression successful.\n");
  printf("Original size: %zu bytes\n", input_length);
  printf("Archive created: %s\n", output_path);

  free(input);
  cpack_free_compressed_buffer(&compressed);

  return 0;
}

static int compress_directory(const char *input_path, const char *output_path) {
  CpackFileList files = {0};
  CpackArchive archive = {0};
  uint64_t total_original_size = 0;

  if (!cpack_collect_files(input_path, &files)) {
    fprintf(stderr, "Error: Could not collect directory files.\n");
    return 1;
  }

  if (files.count == 0) {
    printf("Warning: The directory contains no regular files.\n");
  }

  archive.entries = calloc(files.count, sizeof(CpackArchiveEntry));
  if (files.count > 0 && archive.entries == NULL) {
    fprintf(stderr, "Error: Memory allocation failed.\n");
    cpack_free_file_list(&files);
    return 1;
  }

  archive.capacity = files.count;

  for (size_t i = 0; i < files.count; i++) {
    CpackArchiveEntry *entry = &archive.entries[i];
    archive.count = i + 1;

    char full_path[MAX_PATH_LENGTH];
    if (!join_paths(full_path, sizeof(full_path), input_path, files.paths[i])) {
      fprintf(stderr, "Error: Path too long: %s\n", files.paths[i]);
      goto failure;
    }

    uint8_t *input = NULL;
    size_t input_length = 0;

    if (!cpack_read_file(full_path, &input, &input_length)) {
      fprintf(stderr, "Error: Could not read %s\n", full_path);
      goto failure;
    }

    entry->path = malloc(strlen(files.paths[i]) + 1);
    if (entry->path == NULL) {
      fprintf(stderr, "Error: Memory allocation failed.\n");
      free(input);
      goto failure;
    }

    strcpy(entry->path, files.paths[i]);
    entry->type = CPACK_ENTRY_FILE;
    entry->original_length = input_length;
    entry->checksum = crc32(input, input_length);

    if (!cpack_compress_buffer(input, input_length, &entry->compressed)) {
      fprintf(stderr, "Error: Compression failed for %s\n", full_path);
      free(input);
      goto failure;
    }

    free(input);

    if (UINT64_MAX - total_original_size < input_length) {
      fprintf(stderr, "Error: Total input size overflow.\n");
      goto failure;
    }

    total_original_size += (uint64_t)input_length;
    printf("Added: %s\n", files.paths[i]);
  }

  if (!cpack_archive_write_v3(output_path, &archive)) {
    fprintf(stderr, "Error: Could not write folder archive.\n");
    goto failure;
  }

  printf("\nFolder compression successful.\n");
  printf("Files archived: %zu\n", archive.count);
  printf("Original total size: %llu bytes\n",
         (unsigned long long)total_original_size);
  printf("Archive created: %s\n", output_path);

  cpack_free_archive(&archive);
  cpack_free_file_list(&files);

  return 0;

failure:
  cpack_free_archive(&archive);
  cpack_free_file_list(&files);

  return 1;
}

static int decompress_file(const char *input_path, const char *output_path) {
  CpackCompressedBuffer compressed = {0};
  uint8_t *output = NULL;
  size_t output_length = 0;

  if (!cpack_archive_read(input_path, &compressed)) {
    fprintf(stderr, "Error: Could not read archive.\n");
    return 1;
  }

  if (!cpack_decompress_buffer(&compressed, &output, &output_length)) {
    fprintf(stderr, "Error: Decompression failed.\n");
    cpack_free_compressed_buffer(&compressed);
    return 1;
  }

  if (!cpack_write_file(output_path, output, output_length)) {
    fprintf(stderr, "Error: Could not write output file.\n");
    free(output);
    cpack_free_compressed_buffer(&compressed);
    return 1;
  }

  printf("Decompression successful.\n");
  printf("Restored size: %zu bytes\n", output_length);
  printf("Output file: %s\n", output_path);

  free(output);
  cpack_free_compressed_buffer(&compressed);

  return 0;
}

static int validate_directory_archive(const CpackArchive *archive) {
  for (size_t i = 0; i < archive->count; i++) {
    const CpackArchiveEntry *entry = &archive->entries[i];

    if (entry->type == CPACK_ENTRY_DIRECTORY) {
      continue;
    }

    if (entry->type != CPACK_ENTRY_FILE) {
      fprintf(stderr, "Error: Unknown archive entry type.\n");
      return 0;
    }

    uint8_t *output = NULL;
    size_t output_length = 0;

    if (!cpack_decompress_buffer(&entry->compressed, &output, &output_length)) {
      fprintf(stderr, "Error: Decompression failed for %s\n", entry->path);
      return 0;
    }

    if ((uint64_t)output_length != entry->original_length ||
        crc32(output, output_length) != entry->checksum) {
      fprintf(stderr, "Error: Integrity check failed for %s\n", entry->path);
      free(output);
      return 0;
    }

    free(output);
  }

  return 1;
}

static int decompress_directory_archive(CpackArchive *archive,
                                        const char *output_path) {
  if (!validate_directory_archive(archive)) {
    return 0;
  }

  if (!create_directory(output_path)) {
    fprintf(stderr, "Error: Could not create output directory.\n");
    return 0;
  }

  size_t restored_files = 0;

  for (size_t i = 0; i < archive->count; i++) {
    CpackArchiveEntry *entry = &archive->entries[i];
    char output_file[MAX_PATH_LENGTH];

    if (!join_paths(output_file, sizeof(output_file), output_path,
                    entry->path)) {
      fprintf(stderr, "Error: Output path too long.\n");
      return 0;
    }

    if (entry->type == CPACK_ENTRY_DIRECTORY) {
      if (!create_parent_directories(output_file) ||
          !create_directory(output_file)) {
        fprintf(stderr, "Error: Could not create directory: %s\n", output_file);
        return 0;
      }
      continue;
    }

    if (entry->type != CPACK_ENTRY_FILE) {
      fprintf(stderr, "Error: Unknown archive entry type.\n");
      return 0;
    }

    if (!create_parent_directories(output_file)) {
      return 0;
    }

    uint8_t *output = NULL;
    size_t output_length = 0;

    if (!cpack_decompress_buffer(&entry->compressed, &output, &output_length)) {
      fprintf(stderr, "Error: Decompression failed for %s\n", entry->path);
      return 0;
    }

    if ((uint64_t)output_length != entry->original_length ||
        crc32(output, output_length) != entry->checksum) {
      fprintf(stderr, "Error: Checksum mismatch for %s\n", entry->path);
      free(output);
      return 0;
    }

    if (!cpack_write_file(output_file, output, output_length)) {
      fprintf(stderr, "Error: Could not write %s\n", output_file);
      free(output);
      return 0;
    }

    printf("Restored: %s\n", entry->path);
    restored_files++;
    free(output);
  }

  printf("\nFolder decompression successful.\n");
  printf("Files restored: %zu\n", restored_files);
  printf("Output directory: %s\n", output_path);

  return 1;
}

static int decompress_archive(const char *input_path, const char *output_path) {
  CpackArchive archive = {0};

  if (cpack_archive_read_v3(input_path, &archive)) {
    int success = decompress_directory_archive(&archive, output_path);
    cpack_free_archive(&archive);
    return success ? 0 : 1;
  }

  return decompress_file(input_path, output_path);
}

static int list_directory_archive(const char *archive_path) {
  CpackArchive archive = {0};

  if (!cpack_archive_read_v3(archive_path, &archive)) {
    return 0;
  }

  printf("CPack Folder Archive Information\n");
  printf("--------------------------------\n");
  printf("Archive: %s\n", archive_path);
  printf("Total entries: %zu\n", archive.count);

  uint64_t total_size = 0;
  size_t file_count = 0;
  size_t directory_count = 0;

  for (size_t i = 0; i < archive.count; i++) {
    CpackArchiveEntry *entry = &archive.entries[i];

    if (entry->type == CPACK_ENTRY_DIRECTORY) {
      printf("[DIR]  %s\n", entry->path);
      directory_count++;
    } else if (entry->type == CPACK_ENTRY_FILE) {
      printf("[FILE] %s  (%llu bytes)\n", entry->path,
             (unsigned long long)entry->original_length);

      if (UINT64_MAX - total_size < entry->original_length) {
        fprintf(stderr, "Error: Archive size overflow.\n");
        cpack_free_archive(&archive);
        return 0;
      }

      total_size += entry->original_length;
      file_count++;
    } else {
      fprintf(stderr, "Error: Unknown archive entry type.\n");
      cpack_free_archive(&archive);
      return 0;
    }
  }

  printf("\nFiles: %zu\n", file_count);
  printf("Directories: %zu\n", directory_count);
  printf("Total original size: %llu bytes\n", (unsigned long long)total_size);

  cpack_free_archive(&archive);
  return 1;
}

static int list_file_archive(const char *archive_path) {
  CpackCompressedBuffer compressed = {0};

  if (!cpack_archive_read(archive_path, &compressed)) {
    fprintf(stderr, "Error: Could not read archive.\n");
    return 1;
  }

  printf("CPack Archive Information\n");
  printf("-------------------------\n");
  printf("Archive: %s\n", archive_path);
  printf("Original size: %llu bytes\n",
         (unsigned long long)compressed.original_length);
  printf("Compressed size: %zu bytes\n", compressed.compressed_size);
  printf("Bit length: %llu bits\n", (unsigned long long)compressed.bit_length);
  printf("Token stream size: %llu bytes\n",
         (unsigned long long)compressed.token_stream_length);

  if (compressed.compressed_size > 0) {
    double ratio =
        (double)compressed.original_length / (double)compressed.compressed_size;
    printf("Compression ratio: %.2f:1\n", ratio);
  }

  cpack_free_compressed_buffer(&compressed);
  return 0;
}

static int list_archive(const char *archive_path) {
  if (list_directory_archive(archive_path)) {
    return 0;
  }

  return list_file_archive(archive_path);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_usage();
    return 1;
  }

  if (strcmp(argv[1], "compress") == 0) {
    if (argc != 4) {
      print_usage();
      return 1;
    }

    struct stat info;
    if (stat(argv[2], &info) != 0) {
      perror(argv[2]);
      return 1;
    }

    if (S_ISDIR(info.st_mode)) {
      return compress_directory(argv[2], argv[3]);
    }

    if (S_ISREG(info.st_mode)) {
      return compress_file(argv[2], argv[3]);
    }

    fprintf(stderr, "Error: Input must be a regular file or directory.\n");
    return 1;
  }

  if (strcmp(argv[1], "decompress") == 0) {
    if (argc != 4) {
      print_usage();
      return 1;
    }

    return decompress_archive(argv[2], argv[3]);
  }

  if (strcmp(argv[1], "list") == 0) {
    if (argc != 3) {
      print_usage();
      return 1;
    }

    return list_archive(argv[2]);
  }

  fprintf(stderr, "Unknown command: %s\n\n", argv[1]);
  print_usage();

  return 1;
}