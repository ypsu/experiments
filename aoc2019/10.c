#define _GNU_SOURCE
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { limit = 30 };

int gcd(int a, int b) {
  if (b == 0) return a;
  return gcd(b, a % b);
}

int maxdetect = -1;
int maxr = -1, maxc = -1;
double angle(int pos) {
  int r = maxr - pos % 100;
  int c = maxc - pos / 100;
  double a = atan2(r, c);
  a -= M_PI / 2;
  while (a < 0) a += 2 * M_PI;
  while (a >= 2 * M_PI) a -= 2 * M_PI;
  return a;
}
int astcmp(const void *va, const void *vb) {
  int a = *(const int *)va;
  int b = *(const int *)vb;
  int ar = a % 100 - maxr, ac = a / 100 - maxc;
  int br = b % 100 - maxr, bc = b / 100 - maxc;
  double aa = angle(a);
  double ba = angle(b);
  if (aa == ba) return ar * ar + ac * ac - br * br - bc * bc;
  if (aa < ba) return -1;
  return +1;
}

int main(void) {
  assert(freopen("10.in", "r", stdin) != NULL);
  int rows = 1, cols;
  char grid[limit][limit];
  assert(scanf("%25s ", grid[0]) == 1);
  cols = strlen(grid[0]);
  assert(cols < limit);
  while (scanf("%25s ", grid[rows]) == 1) {
    assert((int)strlen(grid[rows]) == cols);
    rows++;
    assert(rows < limit);
  }
  for (int monr = 0; monr < rows; monr++) {
    for (int monc = 0; monc < cols; monc++) {
      if (grid[monr][monc] == '.') continue;
      int cnt = 0;
      char seen[limit][limit] = {};
      for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
          if (grid[r][c] == '.') continue;
          if (r == monr && c == monc) continue;
          int bitoffset = 0;
          if (r > monr) bitoffset += 1;
          if (c > monc) bitoffset += 2;
          int bitmask = 1 << bitoffset;
          int dr = abs(r - monr);
          int dc = abs(c - monc);
          int d = gcd(dr, dc);
          dr /= d;
          dc /= d;
          if ((seen[dr][dc] & bitmask) != 0) continue;
          cnt++;
          seen[dr][dc] |= bitmask;
        }
      }
      if (cnt > maxdetect) {
        maxdetect = cnt;
        maxr = monr;
        maxc = monc;
      }
    }
  }
  assert(maxdetect != -1);
  printf("part1: %d\n", maxdetect);

  int astcnt = 0;
  int astorder[limit * limit];
  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      if (grid[r][c] == '.') continue;
      if (r == maxr && c == maxc) continue;
      astorder[astcnt++] = c * 100 + r;
    }
  }
  qsort(astorder, astcnt, sizeof(astorder[0]), astcmp);
  bool vaporized[limit * limit] = {};
  int vapcnt = 0;
  while (vapcnt < astcnt) {
    double lastangle = -1;
    for (int i = 0; i < astcnt; i++) {
      if (vaporized[i]) continue;
      double a = angle(astorder[i]);
      if (a == lastangle) continue;
      vaporized[i] = true;
      lastangle = a;
      vapcnt++;
      if (vapcnt == 200) {
        printf("part2: %d\n", astorder[i]);
      }
    }
  }
  return 0;
}
