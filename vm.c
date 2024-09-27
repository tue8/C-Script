#include "common.h"
#include "compiler.h"
#include "disassem.h"
#include "vm.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

struct VM vm;

static void reset_stack()
{
  vm.stack_top = vm.stack;
}

void push(Value value)
{
  *vm.stack_top = value;
  vm.stack_top++;
}

Value pop()
{
  vm.stack_top--;
  return *vm.stack_top;
}

static Value peek(int distance)
{
  return vm.stack_top[-1 - distance];
}

/*
 * remember: if the boolean is true it returns false
 *           if the boolean is false it returns true
 */
static bool is_falsey(Value value)
{
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate()
{
  struct ObjString *b = AS_STRING(pop());
  struct ObjString *a = AS_STRING(pop());

  int length = a->length + b->length;
  char *str = (char *)reallocate(NULL, 0, length + 1);
  memcpy(str, a->c_str, a->length);
  memcpy(str + a->length, b->c_str, b->length);
  str[length] = '\0'; 
  
  struct ObjString *res = take_str(str, length);
  push(OBJ_VAL(res));
}





void init_vm()
{
  reset_stack();
  vm.head_obj = NULL;
  init_table(&vm.globals);
  init_table(&vm.strings);
}

void free_vm()
{
  free_table(&vm.globals);
  free_table(&vm.strings);
  free_objs();
}

static void runtime_err(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = vm.ip - vm.chunk->code - 1;
  int line = vm.chunk->lines[instruction];
  fprintf(stderr, "[line %d] in script\n", line);
  reset_stack();
}



static enum InterpretResult run()
{
  for (;;)
  {
#define READ_BYTE() *(vm.ip++)
#define READ_CONSTANT() vm.chunk->constants.values[READ_BYTE()]
/* takes the next 2 bytes from the chunk and builds a 16-bit uint out of them */
#define READ_SHORT() (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(value_type, op) \
    do \
    { \
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) \
      { \
        runtime_err("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERR; \
      } \
      double b = AS_NUMBER(pop()); \
      double a = AS_NUMBER(pop()); \
      push(value_type(a op b)); \
    } while (false)
#ifdef DEBUG_TRACE_EXECUTION
    disassem_instruction(vm.chunk,
             (int)(vm.ip - vm.chunk->code));

    printf(" CURRENT STACK: [");
    for (Value *slot = vm.stack; slot < vm.stack_top; slot++)
    {
      print_value(*slot, false);
      if (slot != vm.stack_top - 1)
        printf(", ");
    }
    printf("]\n");

#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE())
    {
      case OP_CONSTANT:
      {
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }
      case OP_NIL:   push(NIL_VAL);         break;
      case OP_TRUE:  push(BOOL_VAL(true));  break;
      case OP_FALSE: push(BOOL_VAL(false)); break;
      case OP_EQUAL:
      {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(values_equal(a, b)));
        break;
      }
      case OP_POP: pop(); break;
      case OP_NEGATE:
        if (!IS_NUMBER(peek(0)))
        {
          runtime_err("Operand must be a number.");
          return INTERPRET_RUNTIME_ERR;
        }
        push(NUMBER_VAL(-AS_NUMBER(pop())));
        break;
      case OP_GREATER:  BINARY_OP(BOOL_VAL, >);   break;
      case OP_LESS:     BINARY_OP(BOOL_VAL, <);   break;
      case OP_ADD:
      {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1)))
          concatenate(); 
        else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1)))
        {
          double b = AS_NUMBER(pop());
          double a = AS_NUMBER(pop());
          push(NUMBER_VAL(a + b));
        }
        else
        {
          runtime_err("Operands must be numbers or strings.");
          return INTERPRET_RUNTIME_ERR;
        }
        break;
      }
      case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
      case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
      case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
      case OP_NOT:
        push(BOOL_VAL(is_falsey(pop())));
        break;
      case OP_GETGLOBAL:
      {
        /* Read operand which is an index to a string in the string table */
        struct ObjString *name = READ_STRING();
        Value value;
        if (!table_get(&vm.globals, name, &value))
        {
          runtime_err("Undefined variable '%s'.", name->c_str);
          return INTERPRET_RUNTIME_ERR;
        }
        push(value);
        break;
      }
      case OP_DEFINEGLOBAL:
      {
        struct ObjString *name = READ_STRING();
        table_set(&vm.globals, name, peek(0));
        pop();
        break;
      }
      case OP_SETGLOBAL:
      {
        struct ObjString *name = READ_STRING();
        if (table_set(&vm.globals, name, peek(0)))
        {
          table_delete(&vm.globals, name);
          runtime_err("Undefined variable '%s'.", name->c_str);
          return INTERPRET_RUNTIME_ERR;
        }
        break;
      }
      case OP_GETLOCAL:
      {
        uint8_t slot = READ_BYTE();
        push(vm.stack[slot]);
        break;
      }
      case OP_SETLOCAL:
      {
        uint8_t slot = READ_BYTE();
        vm.stack[slot] = peek(0);
        break;
      }
      case OP_PRINT:
      {
        print_value(pop(), false);
        printf("\n");
        break;
      }
      case OP_JMP:
      {
        uint16_t offset = READ_SHORT();
        vm.ip += offset;
        break;
      }
      case OP_JNT:
      {
        uint16_t offset = READ_SHORT();
        if (is_falsey(peek(0)))
          vm.ip += offset;
        break;
      }
      case OP_JL:
      {
        uint16_t offset = READ_SHORT();
        vm.ip -= offset;
        break;
      }
      case OP_RETURN:
        return INTERPRET_OK;
    }

#undef BINARY_OP 
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
  }
}


enum InterpretResult interpret(const char *src)
{
  struct Chunk chunk;
  init_chunk(&chunk);

  if (!compile(src, &chunk))
  {
    free_chunk(&chunk);
    return INTERPRET_COMPILE_ERR;
  }

  vm.chunk = &chunk;
  vm.ip = vm.chunk->code;

  enum InterpretResult result = run();

  free_chunk(&chunk);
  return INTERPRET_OK;
}

//enum InterpretResult interpret(struct Chunk *chunk)
//{
//  vm.chunk = chunk;
//  vm.ip = vm.chunk->code;
//  return run();
//}


