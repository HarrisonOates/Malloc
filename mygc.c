#include "mymalloc.h"
#include "mygc.h"

static void *start_of_stack = NULL;

// Call this function in your test code (at the start of main)
void set_start_of_stack(void *start_addr) {
  start_of_stack = start_addr;
}

const size_t kMaxAllocationSize = 0ull;

/* We're storing the allocated addresses in a binary tree */
typedef struct Node {
  size_t address;
  struct Node *left;
  struct Node *right;
} Node;

Node *addressTree = NULL;

void *addNode(size_t address){
  
}


void *my_malloc_gc(size_t size) {
  return NULL;
}

void my_free_gc() {
  
}

void *get_end_of_stack() {
  return __builtin_frame_address(1);
}

void my_gc() {
  void *end_of_stack = get_end_of_stack();

  // TODO
}
