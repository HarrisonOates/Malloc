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

  size_t prevBT;
  struct Block* prev;
  struct Block* next;
} Block;

typedef struct Fencepost {
  Block* neighbour;
} Fencepost;


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
static Block* start = NULL;


void *my_malloc(size_t size) {
  if (size == 0 || size > kMaxAllocationSize){
    return NULL;
  }


  return NULL;
}

void my_free(void *ptr) {

}

/* Coalesces contiguous free blocks */
void coalesce(Block* toCoalesce){

}

/* Rounds a size up to the correct alignment value */
inline static size_t round_up(size_t size, size_t alignment) {
  const size_t mask = alignment - 1;
  return (size + mask) & ~mask;
}

/* Requests chunk of memory from the OS and inserts fenceposts */
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

  /* add fence posts either side of the new chunk */
  Fencepost *f1 = p;
  f1->neighbour = (void *) (f1 + sizeof(Fencepost));
  Block* b = f1->neighbour;
  b->size = ARENA_SIZE - 2 * (sizeof(Fencepost));
  Fencepost *f2 = (void *) (b + b->size - sizeof(Fencepost));
  f2->neighbour = b;

  return b;
}

/* returns the correct size block */
Block *split_block(Block *block, size_t size){
  return NULL;
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