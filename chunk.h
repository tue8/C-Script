#ifndef CHUNK_H_
#define CHUNK_H_

#include "common.h"
#include "value.h"

enum Opcode
{
  OP_CONSTANT,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NOT,
  OP_NEGATE,
  OP_PRINT,
  OP_JMP,
  OP_JNT,
  OP_JL,
  OP_RETURN,
  OP_GREATER,
  OP_LESS,
  OP_EQUAL,
  OP_POP,
  OP_GETLOCAL,
  OP_SETLOCAL,
  OP_GETGLOBAL,
  OP_DEFINEGLOBAL,
  OP_SETGLOBAL
}; 

struct Chunk
{
  int count;
  int capacity;
  uint8_t *code;
  int* lines;
  struct ValueArray constants;
};

void init_chunk(struct Chunk *chunk);
void write_chunk(struct Chunk *chunk, uint8_t byte, int line);
int add_constant(struct Chunk *chunk, Value value);
void free_chunk(struct Chunk *chunk);

#endif
