#define _GNU_SOURCE
#include <assert.h>

#ifdef NDEBUG
  #error "NDEBUG must be off so my asserts continue to work."
#endif

static_assert(sizeof(int) == 4, "int must be 4 bytes long");

int main(void) {
  return 0;
}
