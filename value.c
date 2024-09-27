#include "value.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>

#include "object.h"

bool values_equal(Value a, Value b)
{
  if (a.type != b.type)
    return false;

  switch (a.type)
  {
    case VAL_BOOL:    return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL:     return true;
    case VAL_NUMBER:  return AS_NUMBER(a) == AS_NUMBER(b);
    case VAL_OBJ:     return AS_OBJ(a) == AS_OBJ(b);
    default:          return false;
  }
}

void print_value(Value value, bool align)
{
  switch (value.type)
  {
    case VAL_BOOL:
      printf(align ? "%-16s" : "%s", AS_BOOL(value) ? "true" : "false");
      break;
    case VAL_NIL:    printf(align ? "%-16s" : "%s", "nil"); break;
    case VAL_NUMBER: printf(align ? "%-16g" : "%g", AS_NUMBER(value)); break;
    case VAL_OBJ: print_obj(value, align); break;
  }
}
void init_value_array(struct ValueArray *value_array)
{
  value_array->count = 0;
  value_array->capacity = 0;
  value_array->values = NULL;
}

void write_value_array(struct ValueArray *value_array, Value value)
{
  if (value_array->capacity < value_array->count + 1)
  {
    int curr_capacity = value_array->capacity;
    value_array->capacity = (curr_capacity < 8) ? 8 : curr_capacity * 2;
    value_array->values = (Value *)reallocate(value_array->values,
                      curr_capacity * sizeof(Value),
                      value_array->capacity * sizeof(Value));
  }
  value_array->values[value_array->count++] = value;
}

void free_value_array(struct ValueArray *value_array)
{
  reallocate(value_array->values, value_array->capacity * sizeof(Value), 0);
  init_value_array(value_array);
}
