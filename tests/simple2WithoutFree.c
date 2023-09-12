#include "testing.h"

int sizes[] = {123, 456, 1, 8, 8, 8, 56, 8, 12, 67, 32497, 123, 456, 8, 8, 8, 6, 6, 6, 12, 12};
void *pointers[21];

int main() {
  for (int i = 1; i <= 21; i++) {
    for (int j = 0; j < i; j++)
      pointers[j] = mallocing(sizes[j]);
  }
  return 0;
}
