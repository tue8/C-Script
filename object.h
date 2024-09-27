#ifndef _OBJECT_H_
#define _OBJECT_H_

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) is_obj_type(value, OBJ_STRING)

#define AS_STRING(value) ((struct ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) ((struct ObjString *)AS_OBJ(value))->c_str

enum ObjType
{
  OBJ_STRING,
};

typedef struct Obj
{
  enum ObjType type;
  struct Obj *next;
} Obj;

struct ObjString
{
  struct Obj obj;
  uint32_t hash;
  int length;
  char c_str[];
};

struct ObjString *take_str(char *c_str, int length);
struct ObjString *copy_str(const char *c_str, int length);
void print_obj(Value value, bool align);

static inline bool is_obj_type(Value value, enum ObjType type)
{
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
