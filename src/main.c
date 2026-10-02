
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"
#include "compress.h"
#include "file_io.h"

static void print_usage(void) {
  printf("CPack - Custom Lossless Compressor and Archiver\n\n");

  printf("Usage:\n");
  printf("  cpack compress <input> <output.cpk>\n");
  printf("  cpack decompress <input.cpk> <output>\n");
  printf("  cpack list <archive.cpk>\n");
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

    return compress_file(argv[2], argv[3]);
  }

  if (strcmp(argv[1], "decompress") == 0) {
    if (argc != 4) {
      print_usage();
      return 1;
    }

    return decompress_file(argv[2], argv[3]);
  }

  if (strcmp(argv[1], "list") == 0) {
    fprintf(stderr, "List command is not implemented yet.\n");
    return 1;
  }

  fprintf(stderr, "Unknown command: %s\n\n", argv[1]);
  print_usage();

  return 1;
}
