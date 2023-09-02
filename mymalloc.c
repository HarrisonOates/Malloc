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

/* Each block of memory is sandwiched between a header and a footer */
typedef struct Header {
  size_t size; /* if bit 0 is hot, the block is allocated */
  struct Header* prev;
  struct Header* next;
} Header;

typedef struct Footer {
  size_t size; /* if bit 0 is hot, the block is allocated */
} Footer;



const size_t kBlockMetadataSize = sizeof(Header) + sizeof(Footer);
const size_t kMaxAllocationSize = (32ull << 20) - kBlockMetadataSize; // 32MB - size metadata

/* Function prototypes */
inline static size_t round_up(size_t size, size_t alignment);
static Header *getMemory();
Header *split_block(Header *block, size_t size);
void coalesce(Header* toCoalesce);
bool isAllocated(Header* b);
void toggleAllocated(Header* b);
Footer *getFooter(Header* h);
void wipeBlock(Header* h, size_t size);
void clearBit(Header* h);


Header *root = NULL;

// TODO - add the size of the block into the block & wipe the memory

void *my_malloc(size_t size) {
  if (size == 0 || size > kMaxAllocationSize){
    return NULL;
  }

  // Need to add in metadata into the equation

  
  size = round_up(size, kAlignment);
  /* For a block size, if larger than this we should split */
  size_t splitCon = 2*(size + sizeof(Header) - 2*sizeof(Header*) + 1) + 1;
  if (root == NULL){
    Header *h = getMemory();
    if (h == NULL){
      errno = ENOMEM;
      return NULL;
    }
    if (size > splitCon){
      h = split_block(h, size);
    }
    wipeBlock(h, size);
    toggleAllocated(h);
    return ((void*) h) + sizeof(size_t);
  }
 
  
  Header* traverse = root;
  while (traverse->next != NULL && traverse->size < size){
    traverse = traverse->next;
  }
  // We have one of two possibilities now 
  if (traverse->size >= size){
    if (size >= splitCon){
      traverse = split_block(traverse, size);
    }
    wipeBlock(traverse, size);
    toggleAllocated(traverse);
    return ((void*) traverse) + sizeof(size_t);
  }

  Header *h = getMemory();
    if (h == NULL){
      errno = ENOMEM;
      return NULL;
    }
    if (size > splitCon){
      h = split_block(h, size);
    }
  
  wipeBlock(h, size);
  toggleAllocated(h);
  return ((void *) h) + sizeof(size_t);
}

void my_free(void *ptr) {
  if (ptr == NULL){
    return;
  }
  Header *h = (Header *) (((size_t) ptr) - sizeof(size_t)); /* The block is above the next and prev pointers that we set up */
  toggleAllocated(h);
  // coalesce(h);
  

}

/* Coalesces contiguous free blocks */
/* This is super buggy atm - need to fix */
void coalesce(Header* toCoalesce){
  /* Right block */
  bool rightCoalesceFlag = false;
  Header* rightBlock = toCoalesce + toCoalesce->size;
  if (!isAllocated(rightBlock)){
    toCoalesce->next = rightBlock->next;
    toCoalesce->prev = rightBlock->prev;
    toCoalesce->size += rightBlock->size;
    getFooter(toCoalesce)->size = toCoalesce->size;
    rightCoalesceFlag = true;
  }

  /* Left block */
  Header* leftBlock = (Header * )(toCoalesce - (((Header*) (toCoalesce - sizeof(Footer)))->size));
  if (!isAllocated(leftBlock)){
    /* We haven't set whether toCoalesce has a 'next' necessarily */
    if (rightCoalesceFlag){
      /* connect the left block's next and prev neighbours to each other, and then connect it to the so far contiguous block */
      if (leftBlock->prev != NULL){
        (leftBlock->prev)->next = leftBlock->next;
      }
      if (leftBlock->next != NULL){
        (leftBlock->next)->prev = leftBlock->prev;
      }
    }
    leftBlock->size += toCoalesce->size;
    leftBlock->prev = toCoalesce->prev;
    leftBlock->next = toCoalesce->next;
    getFooter(leftBlock)->size = leftBlock->size;
  }


}

/* Rounds a size up to the correct alignment value */
inline static size_t round_up(size_t size, size_t alignment) {
  const size_t mask = alignment - 1;
  return (size + mask) & ~mask;
}

/* Requests chunk of memory from the OS. Inserts into chunk list,
 * adds fenceposts, and returns a pointer to the block.
 */
static Header *getMemory(){
  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  int fd = 0;
  off_t offset = 0;

  void *p = mmap(NULL, ARENA_SIZE, prot, flags, fd, offset);
  /* Can't allocate a new chunk from the OS :( */
  if (p == MAP_FAILED){
    errno = ENOMEM;
    return NULL;
  }

  /* Set up the fenceposts of the block */
  Footer *f1 = p;
  f1->size = 1; /* This doesn't have to be proper; just enough to prevent coalesce from coalescing outside bounds*/
  Footer *f2 = (Footer *) (((void *)f1) + ARENA_SIZE - sizeof(size_t));
  f2->size = 1;

  /* Set up the header and footer on this block */
  Header *h = p + sizeof(size_t);
  h->size = ARENA_SIZE - 2*sizeof(size_t);
  // Allocate the footer
  Footer *f = getFooter(h);
  f->size = h->size;
  /* Append to front of the linked list */

  if (root == NULL){
    root = h;
  }
  else {
   // Come back to this - it's potentially buggy
    Header* n = root->next;
    root = h;
    h->next = n;
  }
  h->prev = NULL;

 // return c->blockChunk;
 return h;
}

/* Returns a block of the correct size, considering the saving of the next and prev pointers.
 * Assume that there is sufficient room in the block, checked by the calling function.
 * Size is the amount of space for allocation requested.
 */
Header *split_block(Header *block, size_t size){
  size_t total_size = block->size;

  /* Saving value of next block as it will get overwritten shortly */
  Header* nextVal = block->next;

  /* Left block */
  Header *left = block;
  left->size = total_size - size - kBlockMetadataSize + 2*sizeof(Header*);
  Footer *leftFooter = getFooter(left);
  leftFooter->size = left->size;

  Header *right = left + left->size;
  right->size = total_size - left->size - 2*sizeof(Header*);
  Footer *rightFooter = getFooter(right);
  rightFooter->size = right->size;

  /* Setting up the linked list again */
  left->next = right;
  right->next = nextVal;
  right->prev = left;

  return right;
}

/* Returns if the block is allocated or not */
bool isAllocated(Header* h){
  return (0x1 & (h->size));
}

/* This was fixed by casting h to size_t */
Footer *getFooter(Header* h){
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

void clearBit(Header* h){
  Footer *f = getFooter(h);
  h->size &= ~(1u);
  f->size &= ~(1u);
}

/* Clears a block ready for the program to use it */
// TODO - come back and work on it as it fails under 'exact'
void wipeBlock(Header* h, size_t size){
  // memset((void*) (h + sizeof(size_t)), 0, size);
}