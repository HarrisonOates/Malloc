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



const size_t kBlockMetadataSize = sizeof(Header) + sizeof(Footer);
const size_t kMaxAllocationSize = (32ull << 20) - kBlockMetadataSize; // 32MB - size metadata

/* Function prototypes */
inline static size_t round_up(size_t size, size_t alignment);
static Header *getMemory(unsigned long long mb);
Header *split_block(Header *block, size_t size);
void coalesce(Header* toCoalesce);
bool isAllocated(Header* b);
void toggleAllocated(Header* b);
Footer *getFooter(Header* h);
void wipeBlock(Header* h, size_t size);
void clearBit(Header* h);
void *retrieveAndFormatNewBlock(size_t size);
void setUpSizeClasses(size_t size);

Header* lists[N_LISTS]; /* Contains the root of each size class. We combine 8 and 16 bytes as they share the same block size */
bool setUpList = false;

void *my_malloc(size_t size) {
  if (size == 0 || size > kMaxAllocationSize){
    return NULL;
  }

  size = round_up(size, kAlignment);
  /* For a block size, if larger than this we should split */
  size_t splitCon = 2*(kBlockMetadataSize - 2*sizeof(Header*)) + size + sizeof(size_t);

  /* Intialize the linked list */
  if (!setUpList){
      setUpSizeClasses(size);
      setUpList = true;
  }
  
  /* Otherwise we are going to have to traverse the list */
  Header *root = NULL;
  int sizeClass = 1;
  if (size <= 16){
    root = lists[1];
  }
  else {
    for (int i = 1; i < N_LISTS; i++){
      if ((i + 1) * 8 >= size && lists[i] != NULL){
        root = lists[i];
        sizeClass = i;
        break;
      }
    }
  }
  

  if (root == NULL){  /* We couldn't find an appropriate block */
    return retrieveAndFormatNewBlock(size);
  }

  Header *trav = root;
  while (trav->next != NULL){
    if (!isAllocated(trav) && trav->size >= kBlockMetadataSize + size - 2*sizeof(Header*)){
      break;
    }
    trav = trav->next;
  }


  /* We found our element! */
  if (!isAllocated(trav) && trav->size >= kBlockMetadataSize + size - 2*sizeof(Header*)){
    Header *h = trav;
    if (trav->size > splitCon){
      h = split_block(h, size);
    }
    /* Fix up the linked list */
    if (h->next != NULL){
      h->next->prev = h->prev;
    }
    if (h->prev != NULL){
      h->prev->next = h->next;
    }
    lists[sizeClass]=h->next;


    // wipeBlock(h);
    toggleAllocated(h);
    return (void*) (((size_t) h) + sizeof(size_t));
  }

  return retrieveAndFormatNewBlock(size);
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

/* Coalesces contiguous free blocks */
void coalesce(Header* toCoalesce){
  /* Right block */
  Header* rightBlock = (Header *) (((size_t) toCoalesce) + toCoalesce->size);
  bool rightCoalesceFlag = false; /* When we get to left block, lets us know if we need to muck around with pointers */
  if (!isAllocated(rightBlock)){
    toCoalesce->next = rightBlock->next;
    toCoalesce->prev = rightBlock->prev;
    toCoalesce->size += rightBlock->size;
    Footer* f = getFooter(toCoalesce);
    f->size = toCoalesce->size;
    rightCoalesceFlag = true;
  }

  /* Left block */
  Footer* leftBlockFooter = (size_t) toCoalesce - sizeof(Footer);
  if (!isAllocated((Header* ) leftBlockFooter)){ // In case we hit a fencepost, which is a footer on the left hand side!

    Header* leftBlock = (Header *)(((size_t) toCoalesce) - ((size_t) leftBlockFooter->size));
    /* We haven't set whether toCoalesce has a 'next' necessarily */
    if (rightCoalesceFlag){
        /* connect the right block's next and prev neighbours to each other, so we keep left where it is in the linked list */
        if (toCoalesce->prev != NULL){
          (toCoalesce->prev)->next = toCoalesce->next;
        }
      if (toCoalesce->next != NULL){
          (toCoalesce->next)->prev = toCoalesce->prev;
        }
    }
    
    leftBlock->size += (toCoalesce->size);
    Footer* f = getFooter(leftBlock);
    f->size = leftBlock->size;
  }


}

/* Rounds a size up to the correct alignment value */
inline static size_t round_up(size_t size, size_t alignment) {
  const size_t mask = alignment - 1;
  return (size + mask) & ~mask;
}

/*
 * Initializes the size classes with a single large block for each class. 
 * Input is a pointer to the block from getMemory(). Should be run once.
 * 
 */
void setUpSizeClasses(size_t size){
  int numElements = 10;
  size_t runningSizeCount = 0;

  Header *h;
    if (size <= (8 << 20) - kBlockMetadataSize){
      h = getMemory(0);
    }
    else {
      h = getMemory(size / ( 1 << 20));
    }
    
    if (h == NULL){
      errno = ENOMEM;
      abort();
    }

  /* Handle special case of the 8 and 16 byte blocks */
  size_t ultimateSize = h->size;
  h->size = 2 * (numElements * (16 + kBlockMetadataSize - 16)); /* 8 and 16 byte blocks */
  Footer* f = getFooter(h);
  f->size = h->size;
  lists[1] = h;
  runningSizeCount = h->size;
  h = (h + h->size); 
  h->prev = NULL;
  h->next = NULL;
  /* We whurr through 8mb, and anything beyond that we plug onto the end of the last list */
  for (int i = 2; i < N_LISTS; i++){
    /* For the algo to work we need to set up the first in each list before entering the loop */
    Header *root = h;

    size_t freeListBlockSize = 8*(i + 1) + kBlockMetadataSize - 16;
    h->size = freeListBlockSize;
    h->prev = NULL;
    h->next = NULL;
    f = getFooter(h);
    f->size = h->size;
    runningSizeCount += h->size;
    h = h + h -> size;

    Header *prev = root;
    for (int j = 1; j < numElements; j++){
      h->size = freeListBlockSize;
      h->prev = prev;
      h->next = NULL;
      prev->next = h;
      
      f = getFooter(h);
      f->size = h->size;
      runningSizeCount += h->size;
      prev = h;
      h = h + h -> size;

    }
    lists[i] = root;
  
  }
  /* We end up with a huge block unused, so we'll tag it on the end of the last list */
  /*
  h->size = ultimateSize - runningSizeCount;
  f = getFooter(h);
  f->size = h->size;
  lists[58]->next = h;
  h->prev = lists[58];
  */
}


/*
 * Abstracts the checking of pointers and redundant code away from malloc()
 */
void *retrieveAndFormatNewBlock(size_t size){
  size_t splitCon = 2*(kBlockMetadataSize - 2*sizeof(Header*)) + size + sizeof(size_t);
  Header *h;
    if (size <= (8 << 20) - kBlockMetadataSize){
      h = getMemory(0);
    }
    else {
      h = getMemory(size / ( 1 << 20));
    }
    
    if (h == NULL){
      errno = ENOMEM;
      return NULL;
    }
    if (h->size > splitCon){
      h = split_block(h, size);
    }
    /* Fix up the linked list structure */
    if (h->next != NULL){
      h->next->prev = h->prev;
    }
    if (h->prev != NULL){
      h->prev->next = h->next;
    }

    wipeBlock(h, size);
    toggleAllocated(h);
    return (void*) (((size_t) h) + sizeof(size_t));
}


/* Requests chunk of memory from the OS. Inserts into chunk list,
 * adds fenceposts, and returns a pointer to the block.
 */
static Header *getMemory(unsigned long long mb){
  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  int fd = 0;
  off_t offset = 0;

  void *p;
  if (mb == 0){ /* Default */
    p = mmap(NULL, ARENA_SIZE, prot, flags, fd, offset);
  }
  else {
    p = mmap(NULL, (size_t) (mb << 20), prot, flags, fd, offset);
  }

  /* Can't allocate a new chunk from the OS :( */
  if (p == MAP_FAILED){
    errno = ENOMEM;
    return NULL;
  }

  /* Set up the fenceposts of the block */
  Footer *f1 = (Footer*) p;
  f1->size = 1;

   /* For right side  */
  Header *h2 = (Header *) (((size_t)f1) + ARENA_SIZE - (sizeof(Header)));
  h2->size = 1;


  /* Set up the header and footer on this block */
  Header *h = (Header *) ((size_t) p + sizeof(Footer));
  h->size = ARENA_SIZE - kBlockMetadataSize;
  // Allocate the footer
  Footer *f = getFooter(h);
  f->size = h->size;
  /* Append to front of the linked list */

  /* AFTER DINNER - COME BACK TO THIS */
  /*
  if (root == NULL){
    root = h;
  }
  else {
   // Come back to this - it's potentially buggy
    Header* n = root;
    root = h;
    h->next = n;
    n->prev = h;
  }
  */
  h->prev = NULL;
  h->next = NULL;

 // return c->blockChunk;
 return h;
}

/* Returns a block of the correct size, considering the saving of the next and prev pointers.
 * Assume that there is sufficient room in the block, checked by the calling function.
 * Size is the amount of space for allocation requested.
 */

/* Issue with split block - not enough for the footer */
Header *split_block(Header *block, size_t size){
  size_t total_size = block->size;

  /* Saving value of next block as it will get overwritten shortly */
  Header* nextVal = block->next;

  /* Left block */
  Header *left = block;
  left->size = total_size - (size + kBlockMetadataSize);
  Footer *leftFooter = getFooter(left);
  leftFooter->size = left->size;

  Header *right = (((size_t) leftFooter) + sizeof(leftFooter));
  right->size = total_size - left->size;
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