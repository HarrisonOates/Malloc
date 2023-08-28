#ifndef MYGC_HEADER
#define MYGC_HEADER

#include <stddef.h>

void *my_malloc_gc(size_t size);
void my_free_gc();

void set_start_of_stack(void *start_addr);
void *get_end_of_stack();
void my_gc();

#endif
