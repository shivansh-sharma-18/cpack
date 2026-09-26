#include <stdio.h>
#include <string.h>

static void print_usage(void) {
  printf("CPack - Custom Lossless Compressor and Archiver\n\n");

  printf("Usage:\n");
  printf("  cpack <command> [options]\n\n");

  printf("Commands:\n");
  printf("  compress    Compress file(s)\n");
  printf("  decompress  Decompress an archive\n");
  printf("  list        List archive contents\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_usage();
    return 1;
  }

  if (strcmp(argv[1], "compress") == 0) {
    printf("Compress command selected.\n");
  } else if (strcmp(argv[1], "decompress") == 0) {
    printf("Decompress command selected.\n");
  } else if (strcmp(argv[1], "list") == 0) {
    printf("List command selected.\n");
  } else {
    printf("Unknown command: %s\n\n", argv[1]);
    print_usage();
    return 1;
  }

  return 0;
}