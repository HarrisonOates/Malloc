#include "mygc.h"
#include <string.h>

// Version of your malloc that clears the block returned by malloc. To make sure when testing
// that there aren't random values in the blocks which just so happen to be pointers to other blocks.
void *my_calloc_gc(size_t size) {
  void *p = my_malloc_gc(size);
  memset(p, 0, size);
  return p;
}

int main() {
  set_start_of_stack(__builtin_frame_address(0));
  // Replace below with your own GC tests

  char *a = my_calloc_gc(8);
  char *b = my_calloc_gc(16);
  b = NULL;

  my_gc();

  // At this point the block "b" should have been freed as garbage as no pointers exist to it anymore
}
