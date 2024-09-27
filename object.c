#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

static struct Obj *allocate_obj(size_t sz, enum ObjType type)
{
  struct Obj *obj = (struct Obj *)reallocate(NULL, 0, sz);
  obj->type = type;
  obj->next = vm.head_obj;
  vm.head_obj = obj;
  return obj;
}

static struct ObjString *allocate_str(const char *c_str, int length, uint32_t hash)
{
  struct ObjString *string = (struct ObjString *)allocate_obj(sizeof(struct ObjString) +
                                                             (sizeof(char) * length + 1), OBJ_STRING);
  string->hash = hash;
  table_set(&vm.strings, string, NIL_VAL); 
  string->length = length;
  memcpy(string->c_str, c_str, length);
  string->c_str[length] = '\0';
  return string;
}

static uint32_t hash_str(const char *key, int length)
{
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++)
  {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

struct ObjString *take_str(char *c_str, int length)
{
  uint32_t hash = hash_str(c_str, length);
  struct ObjString *interned = table_find_str(&vm.strings, c_str, length, hash);
  if (interned != NULL)
  {
    reallocate(c_str, sizeof(char) * (length + 1), 0);
    return interned;
  }
  return allocate_str(c_str, length, hash);
}

struct ObjString *copy_str(const char *c_str, int length)
{
//  char *heap_chars = (char *)reallocate(NULL, 0, sizeof(char) * (length + 1));
//  memcpy(heap_chars, c_str, length);
//  heap_chars[length] = '\0';
  uint32_t hash = hash_str(c_str, length);
  struct ObjString *interned = table_find_str(&vm.strings, c_str, length, hash);
  if (interned != NULL)
    return interned;
  return allocate_str(c_str, length, hash);
}

void print_obj(Value value, bool align)
{
  switch (OBJ_TYPE(value))
  {
    case OBJ_STRING:
      struct ObjString *obj_str = (struct ObjString *)AS_OBJ(value);
      char *str = obj_str->c_str;
      printf(align ? "%-16s" : "%s", str);
      break;
  }
}
