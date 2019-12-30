#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum { digitlimit = 16000 };

int main(void) {
  assert(freopen("08.in", "r", stdin) != NULL);
  int rows, cols;
  char buf[digitlimit];
  assert(scanf("%15500s ", buf) == 1);
  assert(feof(stdin));
  int len = strlen(buf);
  if (len == 12) {
    rows = 2;
    cols = 3;
  } else if (len == 15000) {
    rows = 6;
    cols = 25;
  } else {
    assert(false);
  }

  int part1zeroes = 99999;
  int part1product = -1;
  int part1layers = -1;

  char pic[30][30] = {};

  int idx = 0;
  while (idx < len) {
    assert(idx + rows * cols <= len);
    int zeroes = 0, ones = 0, twos = 0;
    for (int r = 0; r < rows; r++) {
      for (int c = 0; c < cols; c++, idx++) {
        switch (buf[idx]) {
        case '0':
          zeroes++;
          break;
        case '1':
          ones++;
          break;
        case '2':
          twos++;
          break;
        default:
          assert(false);
        }
        if (pic[r][c] != 0 || buf[idx] == '2') continue;
        if (buf[idx] == '0') {
          pic[r][c] = ' ';
        } else {
          pic[r][c] = '#';
        }
      }
    }
    if (zeroes > part1zeroes) continue;
    if (zeroes != part1zeroes) {
      part1layers = 0;
    }
    part1layers++;
    part1zeroes = zeroes;
    part1product = ones * twos;
  }
  assert(part1layers == 1);
  printf("part1: %d\n", part1product);
  puts("part2:");
  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      if (pic[r][c] == '#') {
        putchar('#');
      } else {
        putchar(' ');
      }
    }
    putchar('\n');
  }
  return 0;
}
