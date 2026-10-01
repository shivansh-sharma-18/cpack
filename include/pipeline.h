#ifndef PIPELINE_H
#define PIPELINE_H

#include <stddef.h>
#include <stdint.h>

int cpack_pipeline_round_trip(const uint8_t *input, size_t input_length,
                              uint8_t **output, size_t *output_length);

#endif