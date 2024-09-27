#ifndef VM_H_
#define VM_H_

#include "chunk.h"
#include "table.h"
#include "object.h"

#define STACK_MAX 256

struct VM 
{
  struct Chunk* chunk;
  uint8_t *ip;
  Value stack[STACK_MAX];
  Value *stack_top;
  struct Obj *head_obj;
  struct Table globals;
  struct Table strings;
};

enum InterpretResult
{
  INTERPRET_OK,
  INTERPRET_COMPILE_ERR,
  INTERPRET_RUNTIME_ERR,
};

extern struct VM vm;

void init_vm();
// enum InterpretResult interpret(struct Chunk *chunk);
enum InterpretResult interpret(const char *src);
void push(Value value);
Value pop();
void free_vm();

#endif
