#include "table.h"
#include "object.h"
#include "memory.h"
#include "value.h"

#include <string.h>

#define TABLE_MAX_LOAD 0.75

void init_table(struct Table *table)
{
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void free_table(struct Table *table)
{
  reallocate(table->entries, sizeof(struct Entry) * table->capacity, 0);
  init_table(table);
}

static struct Entry *find_entry(struct Entry *entries, int capacity, struct ObjString *key)
{
  uint32_t index = key->hash % capacity;
  struct Entry *tombstone = NULL;
  for (;;)
  {
    struct Entry *entry = &entries[index];
    if (entry->key == NULL)
    {
      /* empty entry */
      if (IS_NIL(entry->value))
        return tombstone != NULL ? tombstone : entry;
      /* tombstone found */
      else if (tombstone == NULL)
        tombstone = entry;
    }
    else if (entry->key == key)
      return entry;
    
    /* probe until key is found or an empty bucket is found */
    index = (index + 1) % capacity;
  }
}

static void adjust_capacity(struct Table *table, int capacity)
{
  struct Entry *entries = (struct Entry *)reallocate(NULL, 0, sizeof(struct Entry) * capacity);  
  for (int i = 0; i < capacity; i++)
  {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }

  /*
   * when rebuilding a table, tombstones are omitted
   * they serve no value since we are rebuilding the
   * probe sequences anyway
   */
  table->count = 0;
  for (int i = 0; i < table->capacity; i++)
  {
    struct Entry *entry = &table->entries[i];
    if (entry->key == NULL)
      continue;
    
    struct Entry *dest = find_entry(entries, capacity, entry->key); 
    dest->key = entry->key; 
    dest->value = entry->value;
    table->count++;
  }

  reallocate(table->entries, sizeof(struct Entry *) * table->capacity, 0);
  table->entries = entries;
  table->capacity = capacity;
}

bool table_get(struct Table *table, struct ObjString *key, Value *value)
{
  if (table->count == 0)
    return false;
  
  struct Entry *entry = find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL)
    return false;

  *value = entry->value;
  return true;
}

bool table_set(struct Table *table, struct ObjString *key, Value value)
{
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD)
  {
    int capacity = (table->capacity < 8) ? 8 : table->capacity * 2;
    adjust_capacity(table, capacity);
  }

  struct Entry *entry = find_entry(table->entries, table->capacity, key);
  bool is_new_key = (entry->key == NULL);
  /*
   * we dont reduce count when an element is deleted
   * since tombstones are treated like full buckets
   * similarly, we also dont increase count if the new entry
   * is a tombstone
   */
  if (is_new_key && IS_NIL(entry->value))
    table->count++;
  
  entry->key = key;
  entry->value = value;
  return is_new_key;
}

bool table_delete(struct Table *table, struct ObjString *key)
{
  if (table->count == 0)
    return false;

  struct Entry *entry = find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL)
    return false;

  entry->key = NULL;
  entry->value = BOOL_VAL(true);
  return true;
}

struct ObjString *table_find_str(struct Table *table, const char *c_str, int length, uint32_t hash)
{
  if (table->count == 0)
    return NULL;

  uint32_t index = hash % table->capacity;
  for (;;)
  {
    struct Entry* entry = &table->entries[index];
    if (entry->key == NULL && IS_NIL(entry->value))
      /*
       * stop if we find an empty non-tombstone entry
       * if its a tombstone can still try to find the string
       * by probing
       */
      return NULL;
    else if (entry->key->length == length &&
             entry->key->hash == hash &&
      memcmp(entry->key->c_str, c_str, (int)length) == 0)
      return entry->key;
    index = (index + 1) % table->capacity;
  }
}
