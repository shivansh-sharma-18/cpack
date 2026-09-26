#include "bitio.h"

void bitwriter_init(BitWriter *writer, FILE *file) {
  writer->file = file;
  writer->buffer = 0;
  writer->bit_count = 0;
}

int bitwriter_write_bit(BitWriter *writer, int bit) {
  if (bit != 0 && bit != 1) {
    return 0;
  }

  if (bit) {
    writer->buffer |= (uint8_t)(1U << writer->bit_count);
  }

  writer->bit_count++;

  if (writer->bit_count == 8) {
    if (fputc(writer->buffer, writer->file) == EOF) {
      return 0;
    }

    writer->buffer = 0;
    writer->bit_count = 0;
  }

  return 1;
}

int bitwriter_flush(BitWriter *writer) {
  if (writer->bit_count == 0) {
    return 1;
  }

  if (fputc(writer->buffer, writer->file) == EOF) {
    return 0;
  }

  writer->buffer = 0;
  writer->bit_count = 0;
  return 1;
}

void bitreader_init(BitReader *reader, FILE *file) {
  reader->file = file;
  reader->buffer = 0;
  reader->bit_count = 8;
}

int bitreader_read_bit(BitReader *reader) {
  if (reader->bit_count == 8) {
    int byte = fgetc(reader->file);

    if (byte == EOF) {
      return -1;
    }

    reader->buffer = (uint8_t)byte;
    reader->bit_count = 0;
  }

  int bit = (reader->buffer >> reader->bit_count) & 1U;

  reader->bit_count++;

  return bit;
}

int bitwriter_write_bits(BitWriter *writer, uint32_t value, int count) {
  if (count < 0 || count > 32) {
    return 0;
  }

  for (int i = 0; i < count; i++) {
    int bit = (value >> i) & 1U;

    if (!bitwriter_write_bit(writer, bit)) {
      return 0;
    }
  }

  return 1;
}

uint32_t bitreader_read_bits(BitReader *reader, int count) {
  if (count < 0 || count > 32) {
    return 0;
  }

  uint32_t value = 0;

  for (int i = 0; i < count; i++) {
    int bit = bitreader_read_bit(reader);

    if (bit == -1) {
      return 0;
    }

    value |= ((uint32_t)bit << i);
  }

  return value;
}