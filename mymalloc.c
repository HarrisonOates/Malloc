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

/* Each block of memory */
typedef struct Block {
  size_t size; /* if bit 0 is hot, the block is allocated */

  size_t prevSize; /* For constant time coalescing */
  struct Block* prev;
  struct Block* next;
} Block;

/* Fenceposts for a given chunk of memory */
typedef struct Fencepost {
  int flag;
} Fencepost;

/* Defining a linked list to get additional memory from the OS */
typedef struct Chunk {
  struct Chunk* prev;
  struct Chunk* next;
  Block* blockChunk;
} Chunk;


const size_t kBlockMetadataSize = sizeof(Block);
const size_t kMaxAllocationSize = (32ull << 20) - kBlockMetadataSize; // 32MB - size metadata

/* Function prototypes */
inline static size_t round_up(size_t size, size_t alignment);
static Block *getMemory();
Block *split_block(Block *block, size_t size);
void *getDataFromBlock(Block *block);
void *getBlockFromData(void *ptr);
void coalesce(Block* toCoalesce);
bool isAllocated(Block* b);
void toggleAllocated(Block* b);

/* Our list start */
static Chunk* chunkList = NULL;


void *my_malloc(size_t size) {
  if (size == 0 || size > kMaxAllocationSize){
    return NULL;
  }

  size = round_up(size, kAlignment);
  /* For a block size, if larger than this we should split */
  size_t splitCon = 2*(size + sizeof(Block) - 2*sizeof(Block*)) + 1;

  Block *b;

  /* Set up the chunklist */
  if (chunkList == NULL){
    b = getMemory(); /* We set up the free list */
    /* We can allocate immediately and save time */
    if (b == NULL){
      return NULL;
    }
    if (b->size > splitCon){
      b = split_block(b, size); /* Returns a block of size `size + kMetadatasize - 2*sizeof(Block*)` */
    }
    toggleAllocated(b);
    return (void *) (b + sizeof(b->size) + sizeof(b->prevSize));
  }

  b = NULL;

  /* Traverses the chunk list to find a block of the correct size */
  Chunk *traverse = chunkList;
  while (traverse->next != NULL){
    /* Traverse the list of blocks looking for a free bit */
    /* Issue here is setting the block pointer to a free element - this can be done by detecting if they share the same pointer */
    Block *p = traverse->blockChunk;
    while ((p-> next != NULL)  && p->size < size + sizeof(Block) - 2*sizeof(Block*)){ /* We save the memory from the */
      p = p->next;
    }

    // We get to the point where either we have enough size or we get to move to the new chunk
    if (p->size >= size + sizeof(Block) - 2*sizeof(Block*)){
      // We have our value for b;
      b = p;
      break;
    }
    else {
      traverse = traverse->next;
    }
  }

  if (b == NULL){
    /* Allocate more memory, in an identical fashion to the first setup */
    b = getMemory(); /* We set up the free list */
    /* We can allocate immediately and save time */
    if (b == NULL){
      // We throw the error we got from the previous value
      return NULL;
    }
    if (b->size > splitCon){
      b = split_block(b, size); /* Returns a block of size `size + kMetadatasize - 2*sizeof(Block*)` */
    }
   
  }
  else {
    /* Give b its value*/
    /* Move the next and prev pointers around */
    /* Previous pointer to hand stuff to each other */
    ((b - (b->prevSize)) -> next) = (Block*) b + (b->size); /* previous next*/
    (b + (b->size)) -> prev = b - (b->prevSize); /* next previous */
  }

  toggleAllocated(b);
  return (void *) (b + sizeof(b->size) + sizeof(b->prevSize));
}

void my_free(void *ptr) {

}

/* Coalesces contiguous free blocks */
void coalesce(Block* toCoalesce){

  /* right block */
  if (!isAllocated((Block*) (toCoalesce +(toCoalesce->size)))){
    /* Transfer next pointers and absorb sizes */
    toCoalesce->next = (toCoalesce +(toCoalesce->size))->next;
    toCoalesce->size += (toCoalesce +(toCoalesce->size))->size;
  }
  /* Left block */
  if (!isAllocated((Block*) (toCoalesce - (toCoalesce->prevSize)))){
    (toCoalesce - (toCoalesce->prevSize))->prev = toCoalesce->prev;
    (toCoalesce - (toCoalesce->prevSize))->size += toCoalesce->size;
  }
  return;
}

/* Rounds a size up to the correct alignment value */
inline static size_t round_up(size_t size, size_t alignment) {
  const size_t mask = alignment - 1;
  return (size + mask) & ~mask;
}

/* Requests chunk of memory from the OS. Inserts into chunk list,
 * adds fenceposts, and returns a pointer to the block.
 */
static Block *getMemory(){
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

  /* Create chunk metadata and add it to the chunklist*/
  Chunk *c = p;

  if (chunkList == NULL){
    chunkList = c;
    c->prev = NULL;
    c->next = NULL;
  }
  else { /* We traverse the list to place the new chunk at the end */
    Chunk *traverse = chunkList;
    while (traverse->next != NULL){
      traverse = traverse->next;
    }
    traverse->next = c;
    c->prev = chunkList;
    c->next = NULL;
  }

  /* add fence posts either side of the new block */
  Fencepost *f1 = (void *) (c + sizeof(Chunk));
  f1->flag = 1;
  Block* b = (void *) (f1 + sizeof(Fencepost));
  b->size = ARENA_SIZE - 2 * (sizeof(Fencepost)) - sizeof(Chunk);
  Fencepost *f2 = (void *) (b + b->size - sizeof(Fencepost));
  f2->flag = 1;
  b->prev = (void *) f1;
  b->next = (void *) f2;

  c->blockChunk = b;

  return b;
}

/* returns a block of the correct size, considering the saving of the next and prev pointers */
Block *split_block(Block *block, size_t size){
  size_t total_size = block->size;
  Block *nextVal = block->next; /* Saving this as it will get overwritten shortly */
  size_t blockPrevSize = block->prevSize;
  Block *left = block;
  left->size = total_size - size - kBlockMetadataSize + 2*sizeof(Block*);
  left->prev = block->prev;
  left->prevSize = blockPrevSize;
  Block *right = (size_t) left + left->size;
  left->next = right;
  right->prev = left;
  right->prevSize = left->size;
  right->next = nextVal;
  right->size = size + kBlockMetadataSize - 2*sizeof(Block*);

  return right;
}

/* Converts a block to a pointer to its data */
void *getDataFromBlock(Block *block) {
  return (void *)(((size_t)block) + kBlockMetadataSize);
}

/* Converts a pointer to a block's data to its block tag */
void *getBlockFromData(void *ptr) {
  return (Block *)(((size_t)ptr) - kBlockMetadataSize);
}

/* Returns if the block is allocated or not */
bool isAllocated(Block* b){
  return (0x1 & (b->size));
}

/* Sets a block to allocated if it is free and vice versa */
void toggleAllocated(Block* b){
  if (isAllocated(b)){
    b->size &= ~(1u);
  }
  else {
    b->size |= 1u;
  }
  return;
}