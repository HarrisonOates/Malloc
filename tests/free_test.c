#include "testing.h"

int main(void) {
  void *ptr = mallocing(99);
  freeing(ptr);
}
