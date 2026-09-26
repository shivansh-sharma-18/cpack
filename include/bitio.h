#ifndef BITIO_H
#define BITIO_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
  FILE *file;
  uint8_t buffer;
  int bit_count;
} BitWriter;

typedef struct {
  FILE *file;
  uint8_t buffer;
  int bit_count;
} BitReader;

void bitwriter_init(BitWriter *writer, FILE *file);
int bitwriter_write_bit(BitWriter *writer, int bit);
int bitwriter_write_bits(BitWriter *writer, uint32_t value, int count);
int bitwriter_flush(BitWriter *writer);

void bitreader_init(BitReader *reader, FILE *file);
int bitreader_read_bit(BitReader *reader);
uint32_t bitreader_read_bits(BitReader *reader, int count);

#endif