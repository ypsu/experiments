#include <assert.h>
#include <stdio.h>

enum { limit = 10000000 };

int main(void) {
  assert(freopen("01.in", "r", stdin) != NULL);
  int f1 = 0, f2 = 0, m;
  while (scanf("%d", &m) == 1) {
    assert(m < limit);
    if (m <= 6) continue;
    m = m / 3 - 2;
    f1 += m;
    while (m > 0) {
      f2 += m;
      assert(f2 < limit);
      m = m / 3 - 2;
    }
  }
  scanf(" ");
  assert(feof(stdin));
  printf("part 1: %d\n", f1);
  printf("part 2: %d\n", f2);
  return 0;
}
