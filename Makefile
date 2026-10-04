CC = clang
CFLAGS = -Wall -Wextra -Werror -std=c11 -Iinclude

SRC = src/main.c \
      src/file_io.c \
      src/archive.c \
      src/compress.c \
      src/token_codec.c \
      src/lz77.c \
      src/huffman.c \
      src/bitio.c \
      src/crc32.c \
      src/directory.c

TESTS = test_bitio test_crc32 test_lz77 test_huffman \
        test_token_codec test_compress test_archive test_directory

BENCHMARK_SRC = benchmark/benchmark.c \
                src/file_io.c \
                src/compress.c \
                src/token_codec.c \
                src/lz77.c \
                src/huffman.c \
                src/bitio.c \
                src/crc32.c

.PHONY: all test benchmark clean

# Build the main application
all: cpack

cpack: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o cpack

# Build individual test executables
test_bitio: tests/test_bitio.c src/bitio.c
	$(CC) $(CFLAGS) $^ -o $@

test_crc32: tests/test_crc32.c src/crc32.c
	$(CC) $(CFLAGS) $^ -o $@

test_lz77: tests/test_lz77.c src/lz77.c
	$(CC) $(CFLAGS) $^ -o $@

test_huffman: tests/test_huffman.c src/huffman.c src/bitio.c
	$(CC) $(CFLAGS) $^ -o $@

test_token_codec: tests/test_token_codec.c src/token_codec.c src/lz77.c
	$(CC) $(CFLAGS) $^ -o $@

test_compress: tests/test_compress.c src/compress.c src/token_codec.c src/lz77.c src/huffman.c src/bitio.c src/crc32.c
	$(CC) $(CFLAGS) $^ -o $@

test_archive: tests/test_archive.c src/archive.c src/compress.c src/token_codec.c src/lz77.c src/huffman.c src/bitio.c src/crc32.c
	$(CC) $(CFLAGS) $^ -o $@

test_directory: tests/test_directory.c src/directory.c
	$(CC) $(CFLAGS) $^ -o $@

# Build and run all tests
test: $(TESTS)
	@set -e; for test in $(TESTS); do \
		echo "\nRunning $$test..."; \
		./$$test; \
	done

# Build benchmark executable
benchmark: benchmark/cpack_benchmark

benchmark/cpack_benchmark: $(BENCHMARK_SRC)
	$(CC) $(CFLAGS) $^ -o $@

# Remove generated files
clean:
	rm -f cpack $(TESTS) benchmark/cpack_benchmark
	rm -f *.cpk *.restored directory.o