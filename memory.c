#include "memory.h"
#include <stdlib.h>
#include "object.h"
#include "vm.h"

void *reallocate(void *ptr, size_t old_sz, size_t new_sz)
{
  if (new_sz == 0)
  {
    free(ptr);
    return NULL;
  }

  void *res = realloc(ptr, new_sz); 
  if (res == NULL)
    exit(1);
  return res;
}

static void free_obj(struct Obj *obj)
{
  switch (obj->type)
  {
    case OBJ_STRING:
    {
      struct ObjString *obj_str = (struct ObjString *)obj;
      reallocate(obj_str, sizeof(struct ObjString) +
                         (sizeof(char) * obj_str->length + 1), 0);
      break;
    }
  }
}

void free_objs()
{
  struct Obj *obj = vm.head_obj;
  while (obj != NULL)
  {
    struct Obj *next = obj->next;
    free_obj(obj);
    obj = next;
  }
}
