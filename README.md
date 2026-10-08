# CPack — Custom Compression and Archiving Tool

CPack is a command-line compression and archiving utility written in C. It implements a custom lossless compression pipeline using **LZ77 dictionary-based compression and Huffman coding**, along with bit-level I/O, CRC32 integrity checks, and a custom versioned archive format.

It can compress and decompress single files, and archive and extract whole directories, without using any external compression library.

---

## Table of Contents

- [What CPack Does](#what-cpack-does)
- [Quick Start](#quick-start)
- [Prerequisites](#prerequisites)
- [Build Instructions](#build-instructions)
- [Usage](#usage)
- [Project Proposal](#project-proposal)
  - [Project Description](#1-project-description)
  - [Goals](#2-goals)
  - [Specifications](#3-specifications)
  - [Design](#4-design)
- [How It Works](#how-it-works)
- [Archive Format](#archive-format)
- [Project Structure](#project-structure)
- [Testing](#testing)
- [Real-World Verification](#real-world-verification)
- [Benchmark Results](#benchmark-results)
- [Limitations](#limitations)
- [Future Improvements](#future-improvements)
- [License](#license)
- [Author](#author)

---

## What CPack Does

Most compression tools rely on established libraries that hide how compression actually works. CPack implements the core components itself:

1. **LZ77** finds repeated byte sequences.
2. **Huffman coding** gives frequent symbols shorter codes.
3. **Bit-level I/O** stores variable-length codes compactly.
4. **CRC32** verifies the integrity of decompressed data.
5. **A custom archive format** stores compressed data and metadata for one file or a whole directory tree.

LZ77 followed by Huffman coding is the same core idea behind DEFLATE, used by gzip and zip.

---

## Quick Start

```bash
git clone https://github.com/shivansh-sharma-18/cpack.git
cd cpack
make
./cpack
```

Running `./cpack` with no arguments prints the supported commands. A typical session looks like this:

```bash
# compress a single file
./cpack compress input.txt output.cpk

# restore it
./cpack decompress output.cpk restored.txt

# verify the round trip
cmp input.txt restored.txt && echo "identical"

# compress a whole folder, inspect it, and restore it
./cpack compress my_folder my_folder.cpk
./cpack list my_folder.cpk
./cpack decompress my_folder.cpk restored_folder
```

---

## Prerequisites

| Requirement | Details |
|---|---|
| Operating system | Linux or macOS |
| Compiler | GCC or Clang with C11 support |
| Build tool | GNU Make (or compatible) |
| Git | To clone the repository |
| External libraries | None. Only the C standard library is used |

Check your tools:

```bash
gcc --version    # or: clang --version
make --version
```

---

## Build Instructions

1. **Clone the repository**
   ```bash
   git clone https://github.com/shivansh-sharma-18/cpack.git
   ```
2. **Move into the project directory**
   ```bash
   cd cpack
   ```
3. **Compile**
   ```bash
   make
   ```
   The build uses strict warnings: `-Wall -Wextra -Werror -std=c11`. A successful build produces the `cpack` executable in the project root.
4. **Run**
   ```bash
   ./cpack
   ```
5. **(Optional) Run the tests**
   ```bash
   make test
   ```
6. **(Optional) Clean build files**
   ```bash
   make clean
   ```

---

## Usage

Running `./cpack` with no arguments prints:

```text
CPack - Custom Lossless Compressor and Archiver

Usage:
  cpack compress <input_file_or_folder> <output.cpk>
  cpack decompress <input.cpk> <output_file_or_folder>
  cpack list <archive.cpk>
```

CPack has three commands. `compress` and `decompress` accept either a single file or a folder.

| Command | Purpose | Example |
|---|---|---|
| `compress` | Compress a file, or recursively archive and compress a folder, into one `.cpk` | `./cpack compress notes.txt notes.cpk` |
| `decompress` | Restore the original file, or rebuild the folder from a `.cpk` | `./cpack decompress notes.cpk notes_restored.txt` |
| `list` | Show the entries stored in an archive without extracting | `./cpack list my_folder.cpk` |

Example folder:

```text
my_folder/
├── hello.txt
├── src/
│   ├── main.c
│   └── utils/
│       └── helper.c
└── README.md
```

Compressing this folder stores each file with its relative path, so `decompress` can rebuild the same structure:

```bash
./cpack compress my_folder my_folder.cpk
./cpack list my_folder.cpk
./cpack decompress my_folder.cpk restored_folder
```

---

## Project Proposal

### 1. Project Description

CPack is a lossless compression and archiving utility written in C. Its purpose is to show how the main parts of a DEFLATE-style compressor work internally. It takes a file or directory as input, compresses the data with LZ77 and Huffman coding, protects it with a CRC32 checksum, and writes it into a versioned `.cpk` archive. The same tool reads those archives back and restores the original data exactly.

The project doubles as a systems-programming study covering binary file handling, bit-level manipulation, dynamic memory management, tree data structures, and file system traversal.

### 2. Goals

**Primary goals**

- Implement lossless compression (LZ77 + Huffman) from scratch in C, without compression libraries.
- Guarantee that decompression reproduces the original data byte for byte.
- Detect corruption using CRC32 checksums.
- Support both single-file compression and multi-file directory archiving.

**Secondary goals**

- Design a versioned archive format that stays backward compatible.
- Reject malformed, unsupported, or unsafe archives (for example, absolute paths).
- Keep the code modular, with each stage (LZ77, Huffman, bit I/O, CRC32, archive) in its own file.
- Verify correctness with an automated test suite and compare results against `gzip`.

**Non-goals**

- Matching the speed or ratio of production tools such as gzip.
- Cryptographic security or authentication.
- Preserving permissions, timestamps, or empty directories.

### 3. Specifications

**Functional requirements**

| ID | Requirement |
|---|---|
| F1 | Compress a single file into a `.cpk` archive (`compress`) |
| F2 | Decompress a `.cpk` archive back to the original file |
| F3 | Compress a directory recursively into one archive, storing relative paths |
| F4 | List archive contents without extracting (`list`) |
| F5 | Decompress an archive into a directory, rebuilding its structure |
| F6 | Store a CRC32 of the original data and verify it on extraction |
| F7 | Read Version 1 archives (backward compatibility) |
| F8 | Reject malformed archives, unsupported versions, and unsafe paths |

**Non-functional requirements**

- **Correctness:** lossless round trip for any input, including empty files and binary data.
- **Portability:** standard C11, builds on Linux and macOS with GCC or Clang.
- **Maintainability:** modular source layout with a test file per module.
- **Robustness:** builds cleanly under `-Wall -Wextra -Werror`, and invalid input produces errors instead of crashes.

**Technical specifications**

| Item | Value |
|---|---|
| Language | C11 |
| Compression | LZ77 sliding window + Huffman coding |
| LZ77 window size / match lengths | 4096-byte window, matches 3–258 bytes |
| Bit order | LSB-first for both byte packing and multi-bit values |
| Checksum | CRC32 (polynomial `0xEDB88320`, initial value and final XOR `0xFFFFFFFF`) |
| Archive format | Custom binary, versions 1 to 3 |
| Output extension | `.cpk` |

### 4. Design

**Architecture.** The program is split into independent modules. `main.c` parses the command line and dispatches to either the single-file path (`compress.c`) or the archive path (`archive.c`).

```text
                 +------------------+
                 |   Command Line   |
                 |      main.c      |
                 +--------+---------+
                          |
            +-------------+-------------+
            |                           |
            v                           v
     File Compression             Archive Handling
            |                           |
            v                           v
        compress.c                  archive.c
            |                           |
       +----+----+          +-----------+-----------+
       |         |          |           |           |
       v         v          v           v           v
     LZ77     Huffman    file_io    directory     crc32
       |         |
       +----+----+
            |
            v
      token_codec.c
            |
            v
         bitio.c
```

**Module responsibilities**

| Module | Responsibility |
|---|---|
| `main.c` | Command-line interface and dispatch |
| `file_io.c` | File reading and writing |
| `compress.c` | Orchestrates compression and decompression |
| `lz77.c` | Produces and reconstructs literal and match tokens |
| `huffman.c` | Builds the Huffman tree and generates codes |
| `token_codec.c` | Serializes tokens to bits and back |
| `bitio.c` | Bit-level reader and writer |
| `crc32.c` | CRC32 checksum |
| `archive.c` | Archive read, write, and validation |
| `directory.c` | Recursive directory traversal |

**Key design decisions**

- **Two-stage compression.** LZ77 removes repeated sequences and Huffman coding shortens frequent symbols. Together they compress better than either one alone.
- **Bit-level I/O layer.** Huffman codes are not byte-aligned, so a dedicated bit reader and writer keeps encoding logic separate from file handling.
- **CRC of the original data.** The checksum is computed on the uncompressed data, so it verifies the full compress and decompress round trip rather than only the stored bytes.
- **Versioned archive header.** A version field lets the format evolve (V1 to V3) while older archives remain readable.
- **Validate before trusting.** The reader checks versions, sizes, and paths before using any archive data.

**Data flow.** Compression: read file, LZ77 tokens, Huffman frequency analysis, Huffman encoding via the bit writer, then write archive metadata and CRC32. Decompression reverses this and verifies the CRC32.

---

## How It Works

### Compression Pipeline

```text
Input File
    |
    v
Read File as Binary Data
    |
    v
LZ77 Compression
    |
    v
Token Sequence (literals and matches)
    |
    v
Huffman Frequency Analysis
    |
    v
Huffman Encoding
    |
    v
Bit-Level Output
    |
    v
Compressed Data
    |
    v
Archive Metadata + CRC32
    |
    v
CPack Archive (.cpk)
```

### LZ77

LZ77 replaces repeated byte sequences with references to earlier occurrences:

- **Offset:** how far back the earlier occurrence starts.
- **Length:** how many bytes the match covers.

The output is a stream of literal tokens (raw bytes) and match tokens (offset, length).

### Huffman Coding

Frequent symbols receive shorter binary codes and rare symbols receive longer ones. CPack builds the tree from token frequencies and uses it to encode and decode the token stream.

### Bit-Level I/O

Bits are packed into bytes starting from the least significant bit, and multi-bit values are also written least significant bit first. The reader unpacks bits in the same order, which makes round trips work.

### CRC32

CRC32 treats the data as one large binary polynomial and computes the remainder of dividing it by a fixed generator polynomial. In code this is shift-and-conditional-XOR repeated bit by bit, since XOR is subtraction in this arithmetic (no borrowing). Any change in the data almost always changes the result. CPack stores the CRC32 of the original data and recomputes it after decompression. CRC32 detects accidental corruption only and is not a security mechanism.

---

## Archive Format

CPack archives are binary files with an explicit version number.

| Version | Description |
|---|---|
| 1 | Legacy single-file format, kept for backward compatibility |
| 2 | Extended single-file representation |
| 3 | Multi-entry format supporting files and directories, with relative paths and entry types |

```text
[ magic ][ version ][ entry count ]
  per entry:
    [ entry type ][ path length ][ path ]
    [ original size ][ compressed size ][ CRC32 of original data ]
    [ compressed data ]
```

The layout above is a general sketch. Exact field widths and byte order are defined in `src/archive.c`.

**Validation.** The reader rejects unsupported versions, malformed metadata, inconsistent structure, absolute paths, and unsafe path components.

---

## Project Structure

```text
cpack/
├── benchmark/
│   ├── benchmark.c
│   └── data/
├── include/
│   ├── archive.h
│   ├── bitio.h
│   ├── compress.h
│   ├── crc32.h
│   ├── directory.h
│   ├── file_io.h
│   ├── huffman.h
│   ├── lz77.h
│   └── token_codec.h
├── src/
│   ├── main.c
│   ├── archive.c
│   ├── bitio.c
│   ├── compress.c
│   ├── crc32.c
│   ├── directory.c
│   ├── file_io.c
│   ├── huffman.c
│   ├── lz77.c
│   └── token_codec.c
├── tests/
│   ├── test_archive.c
│   ├── test_bitio.c
│   ├── test_compress.c
│   ├── test_crc32.c
│   ├── test_directory.c
│   ├── test_huffman.c
│   ├── test_lz77.c
│   └── test_token_codec.c
├── Makefile
├── LICENSE
└── README.md
```

---

## Testing

```bash
make test
```

| Component | Coverage |
|---|---|
| Bit I/O | Bit-level round trip |
| CRC32 | Known checksum vectors |
| LZ77 | Repeated, unique, empty, and normal input |
| Huffman | Frequency analysis and round trip |
| Token Codec | Literal tokens, match tokens, malformed streams |
| Compression | Text, binary, empty input, invalid metadata |
| Archive | Round trip, raw data, V1 compatibility, invalid versions |
| Directory | Recursive file discovery |

Results from project validation:

```text
Bit I/O:             PASS
CRC32:               PASS
LZ77:                PASS
Huffman:             PASS
Token Codec:         PASS
Compression:         8/8 passed
Archive:             5/5 passed
Directory traversal: PASS
```

Compression tests cover empty input, normal text, repeated characters, binary data, all 256 byte values, invalid arguments, invalid payload size, bit length exceeding payload capacity, incorrect Huffman frequency total, missing compressed data, invalid empty-input metadata, truncated Huffman bit length, and non-zero Huffman padding bits.

Archive tests cover compressed and raw round trips, Version 1 backward compatibility, invalid archive rejection, and unsupported version rejection.

---

## Real-World Verification

CPack was tested on a real directory of 24 files (about 74 KB). The directory was archived, extracted to a separate location, and recursively compared against the original. The restored directory matched. Single-file round trips were verified byte for byte, and corrupted archives were correctly rejected.

---

## Benchmark Results

CPack was compared with the system `gzip` on two 1 MB datasets.

**Repetitive text (1,048,576 bytes)**

| | CPack | gzip |
|---|---|---|
| Compressed size | 5,706 B | 3,153 B |
| Compression ratio | 183.77:1 | |
| Space saved | 99.46% | |
| Compression time | 0.154 s | 0.005 s |
| Decompression time | 0.002 s | |
| Verification | PASS | PASS |

**Random binary (1,048,576 bytes)**

| | CPack | gzip |
|---|---|---|
| Compressed size | 1,048,576 B | 1,048,929 B |
| Compression ratio | 1.00:1 | |
| Space saved | 0.00% | |
| Compression time | 9.849 s | 0.018 s |
| Decompression time | 0.00004 s | |
| Verification | PASS | PASS |

**Notes**

- CPack's size is the compressed buffer, while gzip's includes format overhead.
- CPack was timed with C `clock()` and gzip with shell `time`, so timings are indicative only.
- gzip was smaller and much faster on both datasets.
- The long time on random data indicates that LZ77 match searching is likely the main bottleneck, since the search scans the window without finding useful matches.

---

## Limitations

CPack is an educational project, not a production replacement for established tools.

- Slower than optimized tools such as gzip
- High-entropy data gains little or no compression
- LZ77 matching can be expensive on difficult inputs
- Extraction needs more hardening before handling untrusted archives
- CRC32 gives no cryptographic authenticity
- Empty directories, permissions, and timestamps are not preserved
- No streaming; files are processed in memory

---

## Future Improvements

- **Performance:** hash-chain or hash-table LZ77 match search, faster Huffman construction, lower memory use
- **Compression:** compression levels, better handling of incompressible data, improved match selection
- **Archive:** empty directories, permissions, timestamps, stronger extraction security
- **I/O:** streaming compression and decompression
- **Testing:** fuzz testing, larger benchmark sets, performance regression tests

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

---

## Author

**Shivansh Sharma**
GitHub: [shivansh-sharma-18](https://github.com/shivansh-sharma-18)
