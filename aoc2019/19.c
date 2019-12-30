#include "intcode.c"

enum { shapelimit = 50 };
enum { fullshape = 2000 };
enum { santasize = 100 };
bool grid[fullshape][fullshape];
struct vm initvm;

bool affected(int r, int c) {
  struct vm vm = initvm;
  icputnum(&vm, c);
  icputnum(&vm, r);
  return icgetnum(&vm) == 1;
}

int main(void) {
  assert(freopen("19.in", "r", stdin) != NULL);
  icinitfromstdin(&initvm);
  int startr = 0, startc = 0;
  grid[0][0] = affected(0, 0);
  for (int r = 1; startr == 0 && r < 10; r++) {
    for (int c = 1; startc == 0 && c < 10; c++) {
      if (affected(r, c)) {
        startr = r;
        startc = c;
      }
    }
  }

  assert(startr != 0 && startc != 0);
  int upc = startc, bottomc = startc;
  for (int r = startr; r < fullshape && upc < fullshape; r++) {
    while (!affected(r, bottomc)) bottomc++;
    while (affected(r, upc)) upc++;
    for (int c = bottomc; c < upc; c++) grid[r][c] = true;
  }

  int cnt = 0;
  for (int r = 0; r < shapelimit; r++) {
    for (int c = 0; c < shapelimit; c++) {
      putchar(grid[r][c] ? '#' : '.');
      if (grid[r][c]) cnt++;
    }
    putchar('\n');
  }
  printf("part1: %d\n", cnt);

  int sr = 9999, sc = 9999;
  for (int r = 0; r < fullshape - santasize; r++) {
    for (int c = 0; c < fullshape - santasize; c++) {
      if (!grid[r][c]) continue;
      if (grid[r][c + santasize - 1] && grid[r + santasize - 1][c]) {
        if (r + c < sr + sc) {
          sr = r;
          sc = c;
        }
      }
    }
  }
  printf("part2: %d\n", sc * 10000 + sr);
  return 0;
}
