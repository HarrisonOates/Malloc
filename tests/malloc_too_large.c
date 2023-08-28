#include "testing.h"

/**
 * This test checks that `my_malloc` doesn't fulfill an allocation request for
 * more than `kMaxAllocationSize` bytes.
 *
 * Reason(s) you might be failing this test:
 * - `my_malloc` isn't correctly handling requests for more than kMaxAllocationSize
 *   bytes.
 */

int main() {
  void *ptr = my_malloc(kMaxAllocationSize + 1);
  if (ptr != NULL)
    printf("Expected an error for an allocation larger than the maximum allocable amount\n");
  assert(ptr == NULL);
  void *ptr2 = mallocing(8);
  freeing(ptr2);
}
