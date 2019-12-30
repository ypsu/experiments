#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

bool hasbug(int area, int r, int c) {
  if (r < 0 || 5 <= r) return false;
  if (c < 0 || 5 <= c) return false;
  int offset = r * 5 + c;
  return ((area >> offset) & 1) == 1;
}

int simulate(int area) {
  int newarea = 0;
  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 5; c++) {
      int cnt = 0;
      cnt += hasbug(area, r - 1, c);
      cnt += hasbug(area, r + 1, c);
      cnt += hasbug(area, r, c - 1);
      cnt += hasbug(area, r, c + 1);
      if (cnt == 1 || (cnt == 2 && !hasbug(area, r, c))) {
        newarea |= 1 << (r * 5 + c);
      }
    }
  }
  return newarea;
}

void print(int area) {
  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 5; c++) {
      int mask = 1 << (r * 5 + c);
      putchar((area & mask) != 0 ? '#' : '.');
    }
    putchar('\n');
  }
  putchar('\n');
}

enum { levels = 250 };

struct areainf {
  int area[levels];
};

bool seen[1 << 26];
int main(void) {
  assert(freopen("24.in", "r", stdin) != NULL);
  int initarea = 0;
  for (int r = 0; r < 5; r++) {
    char row[10];
    scanf("%8s", row);
    assert(strlen(row) == 5);
    for (int c = 0; c < 5; c++) {
      if (row[c] == '#') initarea |= 1 << (r * 5 + c);
    }
  }
  int area = initarea;
  int it = 0;
  while (!seen[initarea]) {
    area = simulate(area);
    it++;
    if (seen[area]) {
      printf("part1: %d\n", area);
      break;
    }
    seen[area] = true;
  }

  struct areainf a = {};
  a.area[levels / 2] = initarea;
  for (int it = 0; it < 200; it++) {
    struct areainf n = {};
    for (int level = 1; level < levels - 1; level++) {
      if (a.area[level] == 0) {
        if (a.area[level - 1] == 0 && a.area[level + 1] == 0) {
          continue;
        }
      }
      int cnt[5][5] = {};
      for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 5; c++) {
          if (r == 2 && c == 2) continue;
          cnt[r][c] += hasbug(a.area[level], r - 1, c);
          cnt[r][c] += hasbug(a.area[level], r + 1, c);
          cnt[r][c] += hasbug(a.area[level], r, c - 1);
          cnt[r][c] += hasbug(a.area[level], r, c + 1);
        }
      }
      for (int i = 0; i < 5; i++) cnt[0][i] += hasbug(a.area[level - 1], 1, 2);
      for (int i = 0; i < 5; i++) cnt[4][i] += hasbug(a.area[level - 1], 3, 2);
      for (int i = 0; i < 5; i++) cnt[i][0] += hasbug(a.area[level - 1], 2, 1);
      for (int i = 0; i < 5; i++) cnt[i][4] += hasbug(a.area[level - 1], 2, 3);
      for (int i = 0; i < 5; i++) cnt[1][2] += hasbug(a.area[level + 1], 0, i);
      for (int i = 0; i < 5; i++) cnt[3][2] += hasbug(a.area[level + 1], 4, i);
      for (int i = 0; i < 5; i++) cnt[2][1] += hasbug(a.area[level + 1], i, 0);
      for (int i = 0; i < 5; i++) cnt[2][3] += hasbug(a.area[level + 1], i, 4);
      for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 5; c++) {
          if (r == 2 && c == 2) continue;
          bool h = hasbug(a.area[level], r, c);
          if (cnt[r][c] == 1 || (!h && cnt[r][c] == 2)) {
            n.area[level] |= 1 << (r * 5 + c);
          }
        }
      }
    }
    a = n;
  }
  int bugs = 0;
  for (int d = 0; d < levels - 1; d++) {
    if (a.area[d] == 0) continue;
    bugs += __builtin_popcount(a.area[d]);
    //printf("depth %d:\n", d - levels / 2);
    //print(a.area[d]);
  }
  printf("part2: %d\n", bugs);
  return 0;
}
