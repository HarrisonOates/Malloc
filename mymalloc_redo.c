/* 
 * MyMalloc.c
 * Harrison Oates 2023
 * COMP2310 Assignment
*/
#include "mymalloc.h"
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/* Each block of memory is sandwiched between a header and a footer */
typedef struct Header {
  size_t size; /* if bit 0 is hot, the block is allocated */
  struct Header* prev;
  struct Header* next;
} Header;

typedef struct Footer {
  size_t size; /* if bit 0 is hot, the block is allocated */
} Footer;

Header* root = NULL;


const size_t kBlockMetadataSize = sizeof(Header) + sizeof(Footer);
const size_t kMaxAllocationSize = (32ull << 20) - kBlockMetadataSize; // 32MB - size metadata

void printBlock(Header* block){
  LOG("Block: %p\n", block);
  LOG("Size: %zu\n", block->size);
  LOG("Prev: %p\n", block->prev);
  LOG("Next: %p\n", block->next);
  LOG("Footer: %p\n", (Footer*)((void*)block + block->size - sizeof(Footer)));
}

/* Rounds a size up to the correct alignment value */
inline static size_t round_up(size_t size, size_t alignment) {
  const size_t mask = alignment - 1;
  return (size + mask) & ~mask;
}

bool isAllocated(Header* h){
  return (0x1 & (h->size));
}

Footer *getFooter(Header* h){
  if (isAllocated(h)){
    return (Footer *) (((size_t) h) + (h->size) - sizeof(Footer) - 1);
  }
  return (Footer *) (((size_t) h) + (h->size) - sizeof(Footer));
}

/* Sets a block to allocated if it is free and vice versa */
void toggleAllocated(Header* h){
  Footer *f = getFooter(h);
  if (isAllocated(h)){
    h->size &= ~(1u);
    f->size &= ~(1u);
  }
  else {
    h->size |= 1u;
    f->size |= 1u;
  }
  return;
}

/* Requests chunk of memory from the OS. Returns NULL on failure */
static Header *getMemory(unsigned long long mb){
  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  int fd = 0;
  off_t offset = 0;

  size_t allocatedSize;
  void *p = mmap(NULL, mb, prot, flags, fd, offset);

  /* Can't allocate a new chunk from the OS :( */
  if (p == MAP_FAILED){
    errno = ENOMEM;
    return NULL;
  }

  /* Set up fence posts */
  Footer *f1 = (Footer*) p;
  f1->size = 1;
  Header *h1 = (Header *) (((void *)f1) + mb - (sizeof(Header)));
  h1->size = 1;

  /* Set up header and footer on this block */
  Header *h2 = (Header*) (((size_t)f1) + sizeof(Footer));
  h2->size =  mb - (kBlockMetadataSize);
  Footer *f2 = getFooter(h2);
  f2->size = mb - (kBlockMetadataSize);
  
  /* Return the block to the user */
  h2->prev = NULL;
  h2->next = NULL;

  return h2;
}

/* Returns a block of size (size + kBlockMetaDataSize - 2*sizeof(Header*)) and ensures the linked list is maintained*/
Header *split_block(Header *block, size_t size){
  return NULL;
}

/* Appends a block to the front of the linked list */
void addToList(Header *block){
  if (root == NULL){
    root = block;
    return;
  }

  Header *temp = root;
  root = block;
  root->prev = NULL;
  root->next = temp;
  temp->prev = root;
  return;
}

/* Remove a block from the linked list */
void removeFromList(Header *block){
  if (block->prev == NULL){
    root = block->next;
    return;
  }
  block->prev->next = block->next;
  if (block->next != NULL){
    block->next->prev = block->prev;
  }
  return;
}

/* Find the first block of at least this size */
Header *traverseListForSize(size_t size){
  if (root == NULL){
    return NULL;
  }
  else if (root->size >= size + kBlockMetadataSize - 2*sizeof(Header*)){
    return root;
  }
  Header *curr = root;
  while (curr->next != NULL){
    if (curr->size >= size + kBlockMetadataSize - 2*sizeof(Header*)){
      return curr;
    }
    curr = curr->next;
  }
  return NULL;
}

/* returns a block of precisely size (size + kBlockMetaDataSize - 2*sizeof(header))*/
Header *splitBlock(Header *block, size_t size){
  size_t totalSize = block->size;
  Header* nextVal = block->next;
  Header* prevVal = block->prev;

  Header* left = block;
  left->size = totalSize - (size + kBlockMetadataSize - 2*sizeof(Header*));
  Footer* leftFooter = getFooter(left);
  leftFooter->size = left->size;

  Header *right = (Header*) (((void *) left) + left->size);
  right->size = totalSize - left->size;
  Footer *rightFooter = getFooter(right);
  rightFooter->size = right->size;

  left->prev = prevVal;
  left->next = right; 
  right->prev = left;
  right->next = nextVal;
  
  return right;
}

void coalesce(Header *block){

}

void *my_malloc(size_t size) {
  if (size == 0 || size > kMaxAllocationSize){
    return NULL;
  }

  size = round_up(size, kAlignment);

  /* Traverse the list */
  if (root == NULL){
    Header* h;

    /* Round up getMemory to the nearest 8mb*/
    h = getMemory(round_up(size, ARENA_SIZE));
    addToList(h);
  }

  Header* block = traverseListForSize(size);
  if (block == NULL){
    Header *h = getMemory(round_up(size, ARENA_SIZE));
    if (h->size >= 2*(kBlockMetadataSize - 2*sizeof(Header*) + size + kMinAllocationSize)){
      h = splitBlock(h, size);
    }
    toggleAllocated(h);
    
    return ((void *) h + sizeof(Header));
  }

  if (block->size >= 2*(kBlockMetadataSize - 2*sizeof(Header*) + size + kMinAllocationSize)){
    block = splitBlock(block, size);
  }

  removeFromList(block);
  toggleAllocated(block);
  return ((void *) block + sizeof(Header));
}

void my_free(void *ptr) {
  if (ptr == NULL || (uintptr_t) ptr % 8 != 0){
    return;
  }

  Header *h = (Header *) (((size_t) ptr) - sizeof(size_t)); /* The block is above the next and prev pointers that we set up */
  if (isAllocated(h)){
    toggleAllocated(h);
    coalesce(h);
  }
}


