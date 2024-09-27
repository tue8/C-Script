#ifndef VALUE_H_
#define VALUE_H_
#include <stdbool.h>

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)  ((value).type == VAL_OBJ)

#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)
#define AS_OBJ(value)  ((value).as.obj)

#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object) ((Value){VAL_OBJ, {.obj = (struct Obj *)object}})


typedef struct Obj Obj;

enum ValueType
{
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJ,
};

typedef struct
{
  enum ValueType type;
  union
  {
    bool boolean;
    double number;
    Obj *obj;
  } as;
} Value;

struct ValueArray
{
  int capacity;
  int count;
  Value *values;
};

bool values_equal(Value a, Value b);
void print_value(Value value, bool align);
void init_value_array(struct ValueArray *value_array);
void write_value_array(struct ValueArray *value_array, Value value);
void free_value_array(struct ValueArray *value_array);

#endif
