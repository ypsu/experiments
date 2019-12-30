#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum { limit = 200 };
int dr[] = { -1, 1, 0, 0 };
int dc[] = { 0, 0, -1, 1 };

char map[limit][limit];
int portalmap[limit][limit];
int portal[30 * 30][2];

int main(void) {
  assert(freopen("20.in", "r", stdin) != NULL);
  int rows = 0, cols = 0;
  while (fgets(map[rows], limit, stdin) != NULL) {
    int len = strlen(map[rows]);
    assert(map[rows][len - 1] == '\n');
    len--;
    if (len > cols) cols = len;
    rows++;
  }
  printf("rows:%d cols:%d\n", rows, cols);
  for (int r = 1; r < rows; r++) {
    for (int c = 1; c < cols; c++) {
      for (int d = 0; d < 4; d++) {
        if (!isupper(map[r][c])) continue;
        int pr = r + dr[d ^ 1];
        int pc = c + dc[d ^ 1];
        if (!isupper(map[pr][pc])) continue;
        int nr = r + dr[d];
        int nc = c + dc[d];
        if (map[nr][nc] != '.') continue;
        int p1 = map[r][c] - 'A';
        int p2 = map[pr][pc] - 'A';
        if (p2 > p1) {
          int t = p1;
          p1 = p2;
          p2 = t;
        }
        int por = p1 * 30 + p2;
        int pos = nr * limit + nc;
        if (portal[por][0] == 0) {
          portal[por][0] = pos;
        } else {
          assert(portal[por][pos] == 0);
          portal[por][1] = pos;
          int or = portal[por][0] / limit;
          int oc = portal[por][0] % limit;
          portalmap[or][oc] = portal[por][1];
          portalmap[nr][nc] = portal[por][0];
          printf("[%d,%d] <-> [%d,%d]\n", nr, nc, or, oc);
        }
      }
    }
  }
  int startpos = portal[0][0];
  int endpos = portal[25 * 30 + 25][0];
  assert(startpos != 0 && endpos != 0);

  int q[limit * limit];
  int qb = 0, qe = 0;
  int dist[limit * limit];
  memset(dist, -1, sizeof(dist));
  q[qe++] = startpos;
  dist[startpos] = 0;
  while (qb != qe && dist[endpos] == -1) {
    int curr = q[qb] / limit;
    int curc = q[qb] % limit;
    int curdist = dist[q[qb]];
    qb++;
    for (int d = 0; d < 5; d++) {
      int nr, nc;
      if (d < 4) {
        nr = curr + dr[d];
        nc = curc + dc[d];
      } else {
        if (portalmap[curr][curc] == 0) continue;
        nr = portalmap[curr][curc] / limit;
        nc = portalmap[curr][curc] % limit;
      }
      if (map[nr][nc] != '.') continue;
      int npos = nr * limit + nc;
      if (dist[npos] != -1) continue;
      dist[npos] = curdist + 1;
      q[qe++] = npos;
    }
  }
  assert(dist[endpos] != -1);
  printf("part1: %d\n", dist[endpos]);
  return 0;
}
