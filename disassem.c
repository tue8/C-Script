#include "disassem.h"
#include <stdio.h>

static int simple_instruction(const char *name, int offset)
{
  //printf("%s\n", name);
  printf("%-40s", name);
  return offset + 1;
}

static int constant_instruction(const char *name,
        struct Chunk *chunk,
        int offset)
{
  /* cuz the constant comes after the OP_CONSTANT */
  uint8_t constant = chunk->code[offset + 1];
  printf("%-16s %4d [", name, constant);
  print_value(chunk->constants.values[constant], true);
  printf("]");
  return offset + 2;
}

static int byte_instruction(const char *name, struct Chunk *chunk,
                            int offset)
{
  uint8_t slot = chunk->code[offset + 1];
  printf("%-16s %4d %18s", name, slot, "");
  return offset + 2;
}

static int jmp_instruction(const char *name, int sign,
                           struct Chunk *chunk, int offset)
{
  uint16_t jmp = (uint16_t)(chunk->code[offset + 1] << 8);
  jmp |= chunk->code[offset + 2];
  printf("%-16s %4d -> %04d %10s", name, offset, offset + 3 + sign * jmp, " ");
  return offset + 3;
}

void disassem_chunk(struct Chunk *chunk, const char *name)
{
  printf("DISASSEMBLING CHUNK: %s\n", name);
  for (int offset = 0; offset < chunk->count;)
    offset = disassem_instruction(chunk, offset);
}

int disassem_instruction(struct Chunk *chunk, int offset)
{
  printf("%04d ", offset);
  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1])
    printf("   | ");
  else
    printf("%4d ", chunk->lines[offset]);

  uint8_t instruction = chunk->code[offset];
  switch (instruction)
  {
    case OP_CONSTANT:
      return constant_instruction("OP_CONSTANT", chunk, offset);
    case OP_NIL:
      return simple_instruction("OP_NIL", offset);
    case OP_TRUE:
      return simple_instruction("OP_TRUE", offset);
    case OP_FALSE:
      return simple_instruction("OP_FALSE", offset);
    case OP_EQUAL:
      return simple_instruction("OP_EQUAL", offset);
    case OP_POP:
      return simple_instruction("OP_POP", offset);
    case OP_GREATER:
      return simple_instruction("OP_GREATER", offset);
    case OP_LESS:
      return simple_instruction("OP_LESS", offset);
    case OP_ADD:
      return simple_instruction("OP_ADD", offset);
    case OP_SUBTRACT:
      return simple_instruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
      return simple_instruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
      return simple_instruction("OP_DIVIDE", offset);
    case OP_NOT:
      return simple_instruction("OP_NOT", offset);
    case OP_NEGATE:
      return simple_instruction("OP_NEGATE", offset);
    case OP_PRINT:
      return simple_instruction("OP_PRINT", offset);
    case OP_JMP:
      return jmp_instruction("OP_JMP", 1, chunk, offset);
    case OP_JL:
      return jmp_instruction("OP_JL", -1, chunk, offset);
    case OP_JNT:
      return jmp_instruction("OP_JNT", 1, chunk, offset);
    case OP_RETURN:
      return simple_instruction("OP_RETURN", offset);
    case OP_GETGLOBAL:
      return constant_instruction("OP_GETGLOBAL", chunk, offset);
    case OP_DEFINEGLOBAL:
      return constant_instruction("OP_DEFINEGLOBAL", chunk, offset); 
    case OP_SETLOCAL:
      return byte_instruction("OP_SETLOCAL", chunk, offset); 
    case OP_GETLOCAL:
      return byte_instruction("OP_GETLOCAL", chunk, offset); 
    default:
      printf("Unknown opcode %d", instruction);
      return offset + 1;
  }
}
