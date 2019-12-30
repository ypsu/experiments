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

int q[limit * limit * limit];
int dist[limit * limit * limit];

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
  int startpos = portal[0][0] * limit;
  int endpos = portal[25 * 30 + 25][0] * limit;
  assert(startpos != 0 && endpos != 0);

  int qb = 0, qe = 0;
  memset(dist, -1, sizeof(dist));
  q[qe++] = startpos;
  dist[startpos] = 0;
  while (qb != qe && dist[endpos] == -1) {
    int curr = q[qb] / limit / limit;
    int curc = q[qb] / limit % limit;
    int curlevel = q[qb] % limit;
    assert(curlevel + 1 < limit);
    int curdist = dist[q[qb]];
    qb++;
    for (int d = 0; d < 5; d++) {
      int nr, nc;
      int nlevel = curlevel;
      if (d < 4) {
        nr = curr + dr[d];
        nc = curc + dc[d];
      } else {
        if (portalmap[curr][curc] == 0) continue;
        nr = portalmap[curr][curc] / limit;
        nc = portalmap[curr][curc] % limit;
        if (nr == 2 || nr == rows - 3 || nc == 2 || nc == cols - 3) {
          nlevel = curlevel + 1;
        } else {
          nlevel = curlevel - 1;
        }
        if (nlevel < 0) continue;
      }
      if (map[nr][nc] != '.') continue;
      int npos = (nr * limit + nc) * limit + nlevel;
      if (dist[npos] != -1) continue;
      dist[npos] = curdist + 1;
      q[qe++] = npos;
    }
  }
  assert(dist[endpos] != -1);
  printf("part2: %d\n", dist[endpos]);
  return 0;
}
