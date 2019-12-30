#include "intcode.c"

enum { gridlimit = 60 };
char grid[gridlimit + 1][gridlimit + 1];

int main(void) {
  assert(freopen("17.in", "r", stdin) != NULL);
  struct vm vm;
  icinitfromstdin(&vm);
  int rows, cols;
  cols = strlen(icgetline(&vm));
  icreset(&vm);

  for (int r = 1; r < gridlimit; r++) {
    char *p = icgetline(&vm);
    if (p[0] == 0) {
      rows = r - 1;
      break;
    }
    for (int c = 1; c <= cols; c++) {
      grid[r][c] = p[c - 1];
    }
  }
  int asum = 0;
  for (int r = 1; r < gridlimit; r++) {
   for (int c = 1; c < gridlimit; c++) {
     if (grid[r][c] != '#') continue;
     if (grid[r][c - 1] != '#') continue;
     if (grid[r][c + 1] != '#') continue;
     if (grid[r - 1][c] != '#') continue;
     if (grid[r + 1][c] != '#') continue;
     asum += (r - 1) * (c - 1);
   }
  }

  icreset(&vm);
  vm.mem[0] = 2;
  icputstring(&vm, "A,B,A,B,C,B,A,C,B,C\n");
  icputstring(&vm, "L,12,L,8,R,10,R,10\n");
  icputstring(&vm, "L,6,L,4,L,12\n");
  icputstring(&vm, "R,10,L,8,L,4,R,10\n");
  icputstring(&vm, "n\n");
  puts(icgetstring(&vm));
  printf("rows = %d, cols = %d\n", rows, cols);
  printf("part1: %d\n", asum);
  printf("part2: %lld\n", icgetnum(&vm));
  iccheckhalted(&vm);
  return 0;
}
