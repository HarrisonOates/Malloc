#include "mymalloc.h"
#include "mygc.h"

#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

static void *start_of_stack = NULL;

// Call this function in your test code (at the start of main)
void set_start_of_stack(void *start_addr) {
  start_of_stack = start_addr;
}

/* 
 * We're storing the allocated addresses in a red-black tree.
 * We need to define all the helper functions to get the tree working and rotating correctly.
 * Inspired by [1], with modifications made to better suit my needs
 */
/**************************************************************************************************/
enum Colour{BLACK, RED};

typedef struct Node {
  size_t address;
  enum Colour colour;
  struct Node *parent;
  struct Node *left;
  struct Node *right;
} Node;

Node *addressTree = NULL;

Node* insert(Node* root, Node* toInsert){
  /* Return a new tree */
  if (root == NULL){
    return toInsert;
  }
  /* Else we go down the tree */
  if (toInsert->address < root->address){
    root->left = insert(root->left, toInsert);
    root->left->parent = root;
  }
  else if (toInsert->address > root->address){
    root->right = insert(root->right, toInsert);
    root->right->parent = root;
  }
  return root;
}

void rotateRight(Node* node){
  Node* l = node->left;
  node->left = l->right;
  if (node->left != NULL){
    node->left->parent = node;
  }
  l->parent = node->parent;
  if (node->parent == NULL){
    addressTree = l;
  }
  else if (node == node->parent->left){
    node->parent->left = l;
  }
  else {
    node->parent->right = l;
  }
  l->right = node;
  node->parent = l;
  return;
}

void rotateLeft(Node* node){
  Node* r = node->right;
  node->right = r->left;
  if (node->right != NULL){
    node->right->parent = node;
  }
  r->parent = node->parent;
  if (node->parent == NULL){
    addressTree = r;
  }
  else if (node == node->parent->left){
    node->parent->left = r;
  }
  else {
    node->parent->right = r;
  }
  r->left = node;
  node->parent = r;
}

void balanceAndColour(Node* root, Node* node){
  Node* parentOfNode = NULL;
  Node* grandParentOfNode = NULL;

  while ((node != root) && (root->colour != BLACK) && (node->parent->colour == RED)){
    parentOfNode = node->parent;
    grandParentOfNode = node->parent->parent;
    /* Parent is left child of grandparent */
    if (parentOfNode == grandParentOfNode->left){
      Node* uncleOFNode = grandParentOfNode->right;
      /* Uncle of node is also red, so we only need to recolour */
      if (uncleOFNode != NULL && uncleOFNode->colour == RED){
        grandParentOfNode->colour = RED;
        parentOfNode->colour = BLACK;
        uncleOFNode->colour = BLACK;
        node = grandParentOfNode;
      }
      else {
        /* Node is right child of parent, so left-rotation is required */
        if (node == parentOfNode->right){
          rotateLeft(parentOfNode);
          node = parentOfNode;
          parentOfNode = node->parent;
        }
        else {
          /* Node is left child of its parent so right-rotation is required*/
          rotateRight(grandParentOfNode);
          enum Colour temp = parentOfNode->colour;
          parentOfNode->colour = grandParentOfNode->colour;
          grandParentOfNode->colour = temp;
          node = parentOfNode;
        }
        

      }
    }

    else {
      Node* uncleOfNode = grandParentOfNode->left;
      /* Uncle is also red, so we only need to recolour */
      if (uncleOfNode != NULL && uncleOfNode->colour == RED){
        grandParentOfNode->colour = RED;
        parentOfNode->colour = BLACK;
        uncleOfNode->colour = BLACK;
        node = grandParentOfNode;
      }
      else {
        /* Node is left child of its parent, so right-rotation required */
        if (node == parentOfNode->left){
          rotateRight(parentOfNode);
          node = parentOfNode;
          parentOfNode = node->parent;
        }
        else {
        /* Node is right child, so left rotation required */
          rotateLeft(grandParentOfNode);
          enum Colour temp = parentOfNode->colour;
          parentOfNode->colour = grandParentOfNode->colour;
          grandParentOfNode->colour = temp;
          node = parentOfNode;
        }
      }
    }
  }
}

/* To complete */
void delete(Node* node){
  return;
}

/* Find the element in the tree that fits here */
Node* traverse(Node* node, size_t key){
  /* Not in tree */
  if (node == NULL){
    return NULL;
  }
  else if (node->address == key){
    return node;
  }
  else if (key < node->address){
    return traverse(node->left, key);
  }
  else {
    return traverse(node->right, key);
  }
}

/**************************************************************************************************/

/***************************************************************************/
/* 
 * Everything I've copied-pasted from my standard mymalloc inplementation
*/

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

/* The memory must exist somewhere in the heap */
size_t leftBound = SIZE_MAX;  
size_t rightBound = 0;


const size_t kBlockMetadataSize = sizeof(Header) + sizeof(Footer);
const size_t kMaxAllocationSize = (32ull << 20) - kBlockMetadataSize; /* 32MB - size metadata */

/* Logging block, for testing purposes only */
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


/* These both return if a block is allocated or not */
bool isAllocated(Header* h){
  return (0x1 & (h->size));
}
bool isAllocatedF(Footer* h){
  return (0x1 & (h->size));
}

/* 
 * Gets the index of an appropriate free list for the size.
 */
int getIndex(size_t size){
  int index = (size / 8) - 1 < N_LISTS - 1 ? (size / 8) - 1 : N_LISTS - 1;
  return index;
}

/*
 * Checks if a pointer is in the heap
 */
bool inHeap(size_t h){
  return (h >= leftBound && h <= rightBound);
}


Footer *getFooter(Header* h){
  if (isAllocated(h)){
    return (Footer *) (((size_t) h) + (h->size) - sizeof(Footer) - 1);
  }
  return (Footer *) (((size_t) h) + (h->size) - sizeof(Footer));
}

/* 
 * Sets a block to allocated
 */
void setAllocated(Header *h){
  Footer *f = getFooter(h);
  if ((size_t) h < (size_t) f){
    return;
  }
  h->size |= 1u;
  f->size |= 1u;
  return;
}

/* 
 * Sets a block to unallocated
 */
void setUnAllocated(Header *h){
  Footer *f = getFooter(h);
  h->size &= ~(1u);
  f->size &= ~(1u);
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

  /* Update the bounds */
  leftBound = (size_t) p < leftBound ? (size_t) p : leftBound;
  rightBound = (size_t) p + mb > rightBound ? (size_t) p + mb : rightBound;

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

/* Gets data block from a Header* 
 * Assumes saved metadata optimization
 */
void *get_data(Header *h){
  return (void *) (((size_t) h) + sizeof(size_t));
}

/* Gets block from a data header
 * Assumes saved metadata optimization
 */
Header *get_block(void *p){
  return (Header *) (((size_t) p) - sizeof(size_t));
}

/* Remove a block from the linked list */
void removeFromList(Header *block, int index){
  if (isAllocated(block)){
    return;
  }
  if (block->prev == NULL){
    lists[index] = block->next;
    return;
  }
  else if (inHeap((size_t)(block->prev))){
      block->prev->next = block->next;
  }

  if (block->next != NULL && inHeap((size_t)(block->next))){
    block->next->prev = block->prev;
  }
  return;
}

/* Finds a block of the given size
 * Returns NULL if no block is found
 */
Header *findBlock(size_t size){
  int index = getIndex(size);
  while (index < N_LISTS){
    /* We traverse the list */
    Header *curr = lists[index];
    if (!(curr == NULL)){
      if (curr->size >= size + kBlockMetadataSize - 2*sizeof(Header*)){
        removeFromList(curr, index);
        return curr;
      }
      while (curr->next != NULL){
        if (curr->size >= size + kBlockMetadataSize - 2*sizeof(Header*)){
          removeFromList(curr, index);
          return curr;
        }
        curr = curr->next;
      }
    }
    index++;
  }
  return NULL;
}

/* Adds a block of memory to the appropriately-sized free list */
void addToList(Header* block){
  int index = getIndex(block->size);
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

/* 
 *Splits a block into two, returning a block of size (size + kBlockMetadataSize - 2*sizeof(Header*)
 * We add the left block to the appropriately sized free list.
*/
Header *split_block(Header *block, size_t size){
  if (block->size < 2*(kBlockMetadataSize - 2*sizeof(Header*)) + size + kMinAllocationSize){
    return block;
  }
  /* Fix up the free list */
  if (block->next != NULL){
    block->next->prev = block->prev;
  }
  if (block->prev != NULL){
    block->prev->next = block->next;
  }
  size_t totalSize = block->size;
  Header *left = block;
  left->size = totalSize - (size + kBlockMetadataSize - 2*sizeof(Header*));
  Footer *f = getFooter(left);
  if (!inHeap((size_t) f)){
    return NULL;
  }
  f->size = ((size_t) f) + left->size <= rightBound ? left->size : (left->size - (rightBound - ((size_t) f)));
  left->next = NULL;
  left->prev = NULL;

  Header *right = (Header*) ((size_t) left + left->size);
  right->size = totalSize - left->size;
  Footer *rightFooter = getFooter(right);
  rightFooter->size = right->size;
  setUnAllocated(left);
  setUnAllocated(right);

  addToList(left);
  return right;
}

/* Coalesces a newly free block and makes sure it is in the right size class */
void coalesce(Header *block){
  Footer *leftFooter = (Footer *) ((size_t) block - sizeof(Footer));
  bool leftCoalesceFlag = false;
  if (!isAllocatedF(leftFooter)){
    Header *left = (Header *) ((size_t) block - leftFooter->size);
    left->size += block->size;
    Footer *f = getFooter(left);
    if (!(left->size > rightBound - leftBound)){
      /* Making sure we don't go out of bounds */
      f->size = ((size_t) f) + left->size <= rightBound ? left->size : (left->size - (rightBound - ((size_t) f)));
      rightBound;
      left->size = f->size;
      leftCoalesceFlag = true;
      block = left;
    }
  }

  Header *right = (Header *) ((size_t) block + block->size);
  bool rightCoalesceFlag = false;
  if  (!isAllocated(right)){
    if (!leftCoalesceFlag){
      block->next = right->next;
      block->prev = right->prev;
      block->size += right->size;
      rightCoalesceFlag = true;
    }
    else if (!(right->next != NULL && !inHeap((size_t)(right->next)))){
      if (right->next != NULL){
        right->next->prev = right->prev;
      }
      if (right->prev != NULL){
        right->prev->next = right->next;
      }
      block->size += right->size;
      rightCoalesceFlag = true;
    }
  }

  if (!(leftCoalesceFlag || rightCoalesceFlag)){
    block->next = NULL;
    block->prev = NULL;
    addToList(block);
  }
  return;
}


void *my_malloc(size_t size){
  if (size == 0 || size > kMaxAllocationSize){
    return NULL;
  }
  size = round_up(size, kAlignment);
  Header *block = findBlock(size);
  if (block == NULL){
    block = getMemory(round_up(size, ARENA_SIZE));
  }
  Header* toReturn = split_block(block, size);
  if (!inHeap((size_t) toReturn)){
    toReturn = getMemory(round_up(size, ARENA_SIZE));
    toReturn = split_block(toReturn, size);
  }
  setAllocated(toReturn);
  return get_data(toReturn);
}

void my_free(void *ptr){
  if (ptr == NULL || (uintptr_t) ptr % 8 != 0 || !inHeap((size_t) ptr)){
    return;
  }

  Header* h = get_block(ptr);

  if (isAllocated(h) && inHeap((size_t) h)){
    setUnAllocated(h);
    coalesce(h);
  }
}

/* End of my standard malloc implementation */
/***************************************************************************/

/* We get memory for the search tree  */
void addToTree(size_t address){
  Node *new = (Node *) my_malloc(sizeof(Node));
  new->address = address;
  insert(addressTree, new);
  balanceAndColour(addressTree, new);
  addressTree->colour = BLACK;
  return;
}

void removeFromTree(size_t ptr){
  Node* toDelete = traverse(addressTree, ptr);
  if (toDelete == NULL){
    return;
  }

  delete(toDelete);
  /* Rebalance */

}


void *my_malloc_gc(size_t size) {
  /* We call mymalloc twice - once for the data,
  *  the second time to get memory for the tree
   */
  void* p = my_malloc(size);
  addToTree((size_t) get_block(p));
  return p;
}

void my_free_gc(void *ptr) {
  my_free(ptr);
  /* Traverse the list and remove the pointer from the address tree */

}

void *get_end_of_stack() {
  return __builtin_frame_address(1);
}

void my_gc() {
  void *end_of_stack = get_end_of_stack();

  // TODO
}
