#include "intcode.c"

int dr[] = { -1, 1, 0, 0 };
int dc[] = { 0, 0, -1, 1 };
struct vm vm;

enum { gridlimit = 50 };
char grid[gridlimit][gridlimit];
int dist[gridlimit][gridlimit];
int oxyr, oxyc;

void explore(int r, int c) {
  assert(0 < r && r + 1 < gridlimit);
  assert(0 < c && c + 1 < gridlimit);
  for (int d = 0; d < 4; d++) {
    int nr = r + dr[d];
    int nc = c + dc[d];
    if (grid[nr][nc] != 0) continue;
    icputnum(&vm, d + 1);
    long long output = icgetnum(&vm);
    if (output == 0) {
      grid[nr][nc] = 1;
      continue;
    } else if (output == 1) {
      grid[nr][nc] = 2;
    } else {
      assert(output == 2);
      grid[nr][nc] = 3;
      oxyr = nr;
      oxyc = nc;
    }
    explore(nr, nc);
    // move back.
    icputnum(&vm, (d ^ 1) + 1);
    output = icgetnum(&vm);
    assert(output == 1 || output == 2);
  }
}

int main(void) {
  assert(freopen("15.in", "r", stdin) != NULL);
  icinitfromstdin(&vm);
  explore(gridlimit / 2, gridlimit / 2);
  for (int r = 0; r < gridlimit; r++) {
    for (int c = 0; c < gridlimit; c++) {
      switch (grid[r][c]) {
      case 1:
        putchar('#');
        break;
      case 2:
        putchar('.');
        break;
      case 3:
        putchar('O');
        break;
      default:
        putchar(' ');
        break;
      }
    }
    putchar('\n');
  }

  // bfs the grid.
  int qb = 0, qe = 1;
  int q[gridlimit * gridlimit];
  q[0] = oxyr * gridlimit + oxyc;
  memset(dist, -1, sizeof(dist));
  dist[oxyr][oxyc] = 0;
  int curdist = -1;
  while (qb != qe) {
    int curr = q[qb] / gridlimit;
    int curc = q[qb] % gridlimit;
    curdist = dist[curr][curc];
    qb++;
    for (int d = 0; d < 4; d++) {
      int nr = curr + dr[d];
      int nc = curc + dc[d];
      assert(grid[nr][nc] != 0);
      if (grid[nr][nc] == 1) continue;
      if (dist[nr][nc] != -1) continue;
      dist[nr][nc] = curdist + 1;
      q[qe++] = nr * gridlimit + nc;
    }
  }
  printf("part1: %d\n", dist[gridlimit / 2][gridlimit / 2]);
  printf("part2: %d\n", curdist);
  return 0;
}
