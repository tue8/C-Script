#ifndef _TABLE_H_
#define _TABLE_H_

#include "common.h"
#include "value.h"

struct Entry
{
  struct ObjString *key;
  Value value;
};

struct Table
{
  int count;
  int capacity;
  struct Entry *entries;
};

void init_table(struct Table *table);
void free_table(struct Table *table);
bool table_get(struct Table *table, struct ObjString *key, Value *value);
bool table_set(struct Table *table, struct ObjString *key, Value value);
bool table_delete(struct Table *table, struct ObjString *key);
struct ObjString *table_find_str(struct Table *table, const char *c_str, int length, uint32_t hash);

#endif
