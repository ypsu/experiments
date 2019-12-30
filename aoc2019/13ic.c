#define _GNU_SOURCE
#include "intcode.c"

enum { gridlimit = 1000 };
enum { screensize = 50 };

int main(void) {
  assert(freopen("13.in", "r", stdin) != NULL);
  struct vm vm;
  icinitfromstdin(&vm);
  char screen[screensize][screensize] = {};
  while (ichasoutput(&vm)) {
    long long x = icgetnum(&vm);
    long long y = icgetnum(&vm);
    long long tile = icgetnum(&vm);
    assert(0 <= tile && tile <= 4);
    assert(0 <= x && x < screensize);
    assert(0 <= y && y < screensize);
    screen[y][x] = tile;
  }
  int blocks = 0;
  for (int y = 0; y < screensize; y++) {
    for (int x = 0; x < screensize; x++) {
      if (screen[y][x] == 2) blocks++;
    }
  }

  icreset(&vm);
  vm.mem[0] = 2;
  memset(screen, 0, sizeof(screen));
  long long score = -1;
  long long ballx = 0, paddlex = 0;
  bool print = false;
  while (true) {
    while (ichasoutput(&vm)) {
      long long x = icgetnum(&vm);
      long long y = icgetnum(&vm);
      long long tile = icgetnum(&vm);
      if (x == -1 && y == 0) {
        score = tile;
      } else {
        assert(0 <= x && x < 40);
        assert(0 <= y && y < 25);
        assert(0 <= tile && tile <= 4);
        screen[y][x] = tile;
        if (tile == 3) paddlex = x;
        if (tile == 4) ballx = x;
      }
      if (tile == 4) break;
    }
    int input = 0;
    if (paddlex < ballx) input = +1;
    if (paddlex > ballx) input = -1;
    icputnum(&vm, input);
    if (print) printf("\e[H\e[J");
    int curblocks = 0;
    for (int y = 0; y < 25; y++) {
      for (int x = 0; x < 40; x++) {
        if (screen[y][x] == 2) curblocks++;
        if (print) {
          switch (screen[y][x]) {
          case 1:
            putchar('#');
            break;
          case 2:
            putchar('.');
            break;
          case 3:
            putchar('_');
            break;
          case 4:
            putchar('*');
            break;
          default:
            putchar(' ');
            break;
          }
        }
      }
      if (print) putchar('\n');
    }
    if (print) {
      printf("score: %lld\n", score);
      printf("blocks: %d\n", curblocks);
      usleep(250000);
    }
    if (curblocks == 0) break;
  }

  printf("part1: %d\n", blocks);
  printf("part2: %lld\n", score);
  return 0;
}
