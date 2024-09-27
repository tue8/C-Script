#ifndef DISASSEM_H_
#define DiSASSEM_H_

#include "chunk.h"

void disassem_chunk(struct Chunk *chunk, const char *name);
int disassem_instruction(struct Chunk *chunk, int offset);

#endif
