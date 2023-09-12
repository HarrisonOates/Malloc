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

Header *lists[N_LISTS];

size_t leftBound = SIZE_MAX;  /* The memory must exist somewhere in the heap */
size_t rightBound = 0; /* The memory must exist somewhere in the heap */


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

bool isAllocatedF(Footer* h){
  return (0x1 & (h->size));
}

int getIndex(size_t size){
  int index = (size / 8) - 1 < N_LISTS - 1 ? (size / 8) - 1 : N_LISTS - 1;
  return index;
}

bool inHeap(size_t h){
  return (h >= leftBound && h <= rightBound);
}


Footer *getFooter(Header* h){
  if (isAllocated(h)){
    return (Footer *) (((size_t) h) + (h->size) - sizeof(Footer) - 1);
  }
  return (Footer *) (((size_t) h) + (h->size) - sizeof(Footer));
}

void setAllocated(Header *h){
  Footer *f = getFooter(h);
  h->size |= 1u;
  f->size |= 1u;
  return;
}

void setUnAllocated(Header *h){
  Footer *f = getFooter(h);
  h->size &= ~(1u);
  f->size &= ~(1u);
  return;
}

/* Sets a block to allocated if it is free and vice versa */
void toggleAllocated(Header* h){
  if (isAllocated(h)){
    setUnAllocated(h);
  }
  else {
    setAllocated(h);
  }
  return;
}

/* Requests chunk of memory from the OS. Returns NULL on failure */
static Header *getMemory(unsigned long long mb){
  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  int fd = 0;
  off_t offset = 0;

  void *p = mmap(NULL, mb, prot, flags, fd, offset);

  /* Can't allocate a new chunk from the OS :( */
  if (p == MAP_FAILED){
    errno = ENOMEM;
    return NULL;
  }

  /* Set heap pointers */

    leftBound = leftBound < (size_t) p ? leftBound : (size_t) p;
    rightBound = rightBound > (size_t) p + mb ? rightBound : (size_t) p + mb;


  /* Set up fence posts */
  Footer *f1 = (Footer*) p;
  f1->size = 1;
  Header *h1 = (Header *) (((size_t)f1) + mb - (sizeof(Header)));
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

/* Appends a block to the front of the linked list */
void addToList(Header *block, int index){
  if (isAllocated(block)){
    return;
  }
  if (lists[index] == NULL){
    lists[index] = block;
    return;
  }

  Header *temp = lists[index];
  lists[index] = block;
  lists[index]->prev = NULL;
  lists[index]->next = temp;
  temp->prev = lists[index];
  return;
}

/* Remove a block from the linked list */
void removeFromList(Header *block, int index){
  // printBlock(block);
  if (isAllocated(block)){
    return;
  }
  if (block->prev == NULL){
    lists[index] = block->next;
    return;
  }
  if (inHeap((size_t)(block->prev)) && inHeap((size_t) block->prev->next)){
    block->prev->next = block->next;
  }

  
  if (block->next != NULL){
    if (inHeap((size_t) (block->next)) && inHeap((size_t) (block->next->prev))){
      block->next->prev = block->prev;
    }

        
  }
  return;
}

/* Find the first block of at least this size */
Header *traverseListForSize(size_t size, int index){
  if (lists[index] == NULL || !inHeap(((size_t) lists[index]))){
    return NULL;
  }
  else if (lists[index]->size >= size + kBlockMetadataSize - 2*sizeof(Header*)){
    return lists[index];
  }
  Header *curr = lists[index];
  while (curr->next != NULL){
    if (curr->size >= size + kBlockMetadataSize - 2*sizeof(Header*)){
      return curr;
    }
    curr = curr->next;
  }
  return NULL;
}

/* returns a block of precisely size (size + kBlockMetaDataSize - 2*sizeof(header)) 
 * The left split block is left hanging, to be inserted in the correct size class 
 */
Header* splitBlock(Header *block, size_t size){
  size_t totalSize = block->size;
  Header* nextVal = block->next;
  Header* prevVal = block->prev;
  if (prevVal != NULL){
    prevVal->next = nextVal;
  }
  if (nextVal != NULL){
    nextVal->prev = prevVal;
  }

  Header* left = block;
  left->size = totalSize - (size + kBlockMetadataSize - 2*sizeof(Header*));
  Footer* leftFooter = getFooter(left);
  leftFooter->size = left->size;

  Header *right = (Header*) (((size_t) left) + left->size);
  right->size = totalSize - left->size;
  Footer *rightFooter = getFooter(right);
  rightFooter->size = right->size;

  left->prev = NULL;
  left->next = NULL; 
  right->prev = NULL;
  right->next = NULL;
  return right;
}

void coalesce(Header *block){
  Header *temp = block; // So I can overwrite the initial block further down 
  Footer *leftFooter = (Footer *) (((size_t) block) - sizeof(Footer));
  bool leftCoalesceFlag = false;
  if (!isAllocatedF(leftFooter)){
    Header *leftBlock = (Header *) (((size_t) block) - leftFooter->size);
    leftBlock->size += block->size;
    Footer* f = getFooter(leftBlock);
    f->size = leftBlock->size;
    leftCoalesceFlag = true;
    block = leftBlock;
  }


  Header *rightBlock = (Header *) (((size_t) temp) + temp->size);
  bool rightCoalesceFlag = false;
  if (!isAllocated(rightBlock)){
    if (leftCoalesceFlag){
      if (rightBlock->next != NULL){
        rightBlock->next->prev = rightBlock->prev;
      }
      if (rightBlock->prev != NULL){
        rightBlock->prev->next = rightBlock->next;
      }
      block->size += rightBlock->size;
      Footer* f = getFooter(block);
      f->size = block->size;
    }
    else {
      block->next = rightBlock->next;
      block->prev = rightBlock->prev;
      block->size += rightBlock->size;
      Footer* f = getFooter(block);
      f->size = block->size;
    }
   
    rightCoalesceFlag = true; 
  }



  if (!leftCoalesceFlag && !rightCoalesceFlag){
    block->next = NULL;
    block->prev = NULL;
    
  }
 // addToList(block, getIndex(block->size));
  return;
}

Header *findBlock(size_t size){
  int index = getIndex(size);

  while (index < N_LISTS){
    Header *block = traverseListForSize(size, index);
    if (block != NULL && !isAllocated(block)){
      removeFromList(block, index);
      return block;
    }
    index++;
  }

  return NULL;
}

/* We request some memory, partition it, and insert the split block into the free list at the correct place */
Header *RequestAndPartition(size_t size){
  Header *block = getMemory(round_up(size, ARENA_SIZE));
  if (block == NULL){
    return NULL;
  }
  if (block->size > 2*(kBlockMetadataSize - 2*sizeof(Header*)) + size + kMinAllocationSize){
    block = splitBlock(block, size);
    // Left is hanging on purpose
    Footer *leftFooter = (Footer *) (((size_t) block) - sizeof(Footer));
    Header *left = (Header *) (((size_t) block) - leftFooter->size);
    addToList(left, getIndex(left->size));
  }
  
  return block;

}

void *my_malloc(size_t size) {
  if (size == 0 || size > kMaxAllocationSize){
    return NULL;
  }

  size = round_up(size, kAlignment);

  Header* block = findBlock(size);

  if (block == NULL){
    block = RequestAndPartition(size);
  }

  setAllocated(block);
  return (void *) ((size_t) block + sizeof(size_t));

}

void my_free(void *ptr) {
  if (ptr == NULL || (uintptr_t) ptr % 8 != 0){
    return;
  }

  Header *h = (Header *) (((size_t) ptr) - sizeof(size_t)); /* The block is above the next and prev pointers that we set up */
  if (isAllocated(h)){
    setUnAllocated(h);
    coalesce(h);
  }

  return;
}


