#ifndef PIPELINE_H
#define PIPELINE_H

#include <stddef.h>
#include <stdint.h>

#include "lz77.h"

int cpack_serialize_tokens(const LZ77TokenArray *tokens, uint8_t **output,
                           size_t *output_length);

int cpack_deserialize_tokens(const uint8_t *data, size_t length,
                             LZ77TokenArray *tokens);

int cpack_pipeline_round_trip(const uint8_t *input, size_t input_length,
                              uint8_t **output, size_t *output_length);

#endif