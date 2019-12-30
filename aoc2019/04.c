#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

int main(void) {
  assert(freopen("04.in", "r", stdin) != NULL);
  int lo, hi;
  assert(scanf("%d-%d ", &lo, &hi) == 2);
  assert(feof(stdin));
  assert(100000 <= lo && hi <= 999999);
  int cnt1 = 0;
  int cnt2 = 0;
  for (int i = lo; i <= hi; i++) {
    char num[9];
    sprintf(num, "%d", i);
    bool doubledigit = false;
    bool doubleonly = false;
    bool increasing = true;
    for (int j = 1; increasing && j < 6; j++) {
      if (num[j] == num[j - 1]) doubledigit = true;
      if (num[j] < num[j - 1]) increasing = false;
      if (!doubleonly && num[j] == num[j - 1]) {
        if (j >= 2 && num[j] == num[j - 2]) continue;
        if (j <= 4 && num[j] == num[j + 1]) continue;
        doubleonly = true;
      }
    }
    if (doubledigit && increasing) cnt1++;
    if (doubleonly && increasing) cnt2++;
  }
  printf("part1: %d\n", cnt1);
  printf("part2: %d\n", cnt2);
  return 0;
}
