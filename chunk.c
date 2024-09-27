#include "chunk.h"
#include "memory.h"
#include "value.h"

void init_chunk(struct Chunk *chunk)
{
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  init_value_array(&chunk->constants);
}

void write_chunk(struct Chunk *chunk, uint8_t byte, int line)
{
  if (chunk->capacity < chunk->count + 1)
  {
    int curr_capacity = chunk->capacity;
    chunk->capacity = (curr_capacity < 8) ? 8 : curr_capacity * 2;
    chunk->code = (uint8_t *)reallocate(chunk->code, curr_capacity * sizeof(uint8_t),
                                                     chunk->capacity * sizeof(uint8_t));
    chunk->lines = (int *)reallocate(chunk->lines, curr_capacity * sizeof(int),
                                                   chunk->capacity * sizeof(int));
  }
  chunk->code[chunk->count] = byte;
  chunk->lines[chunk->count] = line;
  chunk->count++;
}

void free_chunk(struct Chunk *chunk)
{
  reallocate(chunk->code, chunk->capacity * sizeof(uint8_t), 0);
  reallocate(chunk->lines, chunk->capacity * sizeof(uint8_t), 0);
  free_value_array(&chunk->constants);
  init_chunk(chunk);
}

int add_constant(struct Chunk *chunk, Value value)
{
  write_value_array(&chunk->constants, value);
  return chunk->constants.count - 1;
}
