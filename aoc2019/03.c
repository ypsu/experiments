#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum { rangelimit = 1000000 };
enum { wirelimit = 1000000 };

struct wirepiece {
  int r, c;
  int steps;
} wire[2][wirelimit];
int wirelen[2];

int wirecmp(const void *lhs, const void *rhs) {
  const struct wirepiece *a = lhs;
  const struct wirepiece *b = rhs;
  if (a->r != b->r) return a->r - b->r;
  if (a->c != b->c) return a->c - b->c;
  return a->steps - b->steps;
}

int main(void) {
  assert(freopen("03.in", "r", stdin) != NULL);

  // read input and sort the wire pieces by coordinate.
  for (int w = 0; w < 2; w++) {
    int len = 0;
    int r = 0;
    int c = 0;
    char dir;
    int dist;
    assert(scanf("%c%d", &dir, &dist) == 2);
    assert(dist >= 0);
    while (true) {
      int dr = 0, dc = 0;
      if (dir == 'L') dc = -1;
      if (dir == 'R') dc = +1;
      if (dir == 'U') dr = -1;
      if (dir == 'D') dr = +1;
      assert(dr != 0 || dc != 0);
      assert(-rangelimit < r && r < rangelimit);
      assert(-rangelimit < c && c < rangelimit);
      for (int i = 0; i < dist; i++) {
        assert(len < wirelimit);
        r += dr;
        c += dc;
        wire[w][len].r = r;
        wire[w][len].c = c;
        wire[w][len].steps = len + 1;
        len++;
      }
      char x = getchar();
      if (x == '\n') break;
      assert(x == ',');
      assert(scanf("%c%d", &dir, &dist) == 2);
      assert(dist >= 0);
    };
    scanf(" ");
    wirelen[w] = len;
    qsort(wire[w], wirelen[w], sizeof(struct wirepiece), wirecmp);
  }
  assert(feof(stdin));

  // merge the two wires and inspect the identical pieces.
  int bestdist = rangelimit;
  int beststeps = rangelimit;
  int a = 0, b = 0;
  while (a < wirelen[0] && b < wirelen[1]) {
    struct wirepiece *x = &wire[0][a];
    struct wirepiece *y = &wire[1][b];
    if (x->r == y->r && x->c == y->c) {
      int dist = abs(x->r) + abs(x->c);
      if (dist < bestdist) bestdist = dist;
      int steps = x->steps + y->steps;
      if (steps < beststeps) beststeps = steps;
      a++;
      b++;
    } else if (x->r < y->r || (x->r == y->r && x->c < y->c)) {
      a++;
    } else {
      b++;
    }
  }

  // print the results.
  assert(bestdist != rangelimit);
  assert(beststeps != rangelimit);
  printf("part1: %d\n", bestdist);
  printf("part2: %d\n", beststeps);
  return 0;
}
