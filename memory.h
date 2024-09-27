#ifndef MEMORY_H_
#define MEMORY_H_
#include <stddef.h>

void *reallocate(void *ptr, size_t old_sz, size_t new_sz);
void free_objs();

#endif
