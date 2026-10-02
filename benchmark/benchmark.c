
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "compress.h"
#include "file_io.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: benchmark <input_file>\n");
    return 1;
  }

  uint8_t *input = NULL;
  size_t input_length = 0;

  if (!cpack_read_file(argv[1], &input, &input_length)) {
    fprintf(stderr, "Error: Could not read input file.\n");
    return 1;
  }

  CpackCompressedBuffer compressed = {0};

  uint8_t *output = NULL;
  size_t output_length = 0;

  clock_t start = clock();

  if (!cpack_compress_buffer(input, input_length, &compressed)) {
    fprintf(stderr, "Error: Compression failed.\n");
    free(input);
    return 1;
  }

  clock_t compression_end = clock();

  if (!cpack_decompress_buffer(&compressed, &output, &output_length)) {
    fprintf(stderr, "Error: Decompression failed.\n");
    free(input);
    cpack_free_compressed_buffer(&compressed);
    return 1;
  }

  clock_t decompression_end = clock();

  if (input_length != output_length ||
      memcmp(input, output, input_length) != 0) {
    fprintf(stderr, "Error: Data verification failed.\n");
    free(input);
    free(output);
    cpack_free_compressed_buffer(&compressed);
    return 1;
  }

  double compression_time = (double)(compression_end - start) / CLOCKS_PER_SEC;

  double decompression_time =
      (double)(decompression_end - compression_end) / CLOCKS_PER_SEC;

  double ratio = 0.0;
  double space_saved = 0.0;

  if (input_length > 0) {
    ratio = (double)input_length / (double)compressed.compressed_size;

    space_saved =
        (1.0 - (double)compressed.compressed_size / (double)input_length) *
        100.0;
  }

  printf("\nCPack Benchmark Results\n");
  printf("Input file: %s\n", argv[1]);
  printf("Original size: %zu bytes\n", input_length);
  printf("Compressed size: %zu bytes\n", compressed.compressed_size);
  printf("Compression ratio: %.2f:1\n", ratio);
  printf("Space saved: %.2f%%\n", space_saved);
  printf("Compression time: %.6f seconds\n", compression_time);
  printf("Decompression time: %.6f seconds\n", decompression_time);
  printf("Data verification: PASS\n");

  free(input);
  free(output);
  cpack_free_compressed_buffer(&compressed);

  return 0;
}
