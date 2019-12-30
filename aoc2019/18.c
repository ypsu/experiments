#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int64_t i64;
typedef int8_t i8;
typedef uint8_t u8;

enum { limit = 128 };
enum { statelimit = 10000000 };
int rows, cols;
char map[limit][limit];
int allkeys;

// i64 map implement using critbit trees per
// https://www.imperialviolet.org/binary/critbit.pdf.
// instead of pointers the child[x] can contain the actual integer. negative
// numbers mean internal nodes (an index into the cbtnode array). the lower
// keybits bits are used for the key (that's used for traversing the tree), rest
// of the bits is used for the values.
struct cbtnode {
  i64 child[2];
  i64 otherbits;
} cbtnode[statelimit];
int cbtnodes = 1;
i64 cbtroot;
const i64 keybits = 42;
const i64 keymask = (1LL << keybits) - 1;
// returns -1 if not found.
int getvalue(i64 key) {
  assert(key < keymask);
  i64 p = cbtroot;
  if (p == 0) return -1;
  while (p < 0) {
    struct cbtnode n = cbtnode[-p];
    int direction = (1 + (n.otherbits | key)) >> keybits;
    p = n.child[direction];
  }
  if ((p & keymask) == key) return p >> keybits;
  return -1;
}
void addentry(i64 key, i64 value) {
  assert(key < keymask);
  assert(cbtnodes < statelimit);
  i64 p = cbtroot;
  if (p == 0) {
    cbtroot = key | (value << keybits);
    return;
  }
  while (p < 0) {
    struct cbtnode n = cbtnode[-p];
    int direction = (1 + (n.otherbits | key)) >> keybits;
    p = n.child[direction];
  }
  assert((p & keymask) != key);
  i64 newotherbits = (p & keymask) ^ key;
  while (((newotherbits & (newotherbits - 1))) != 0) {
    newotherbits &= newotherbits - 1;
  }
  newotherbits ^= keymask;
  int newdirection = (1 + (newotherbits | (p & keymask))) >> keybits;
  struct cbtnode *newnode = &cbtnode[cbtnodes++];
  newnode->otherbits = newotherbits;
  newnode->child[1 - newdirection] = key | (value << keybits);
  i64 *wherep = &cbtroot;
  while (true) {
    i64 p = *wherep;
    if (p > 0) break;
    struct cbtnode *q = &cbtnode[-p];
    if (q->otherbits > newotherbits) break;
    int direction = (1 + (q->otherbits | key)) >> keybits;
    wherep = &q->child[direction];
  }
  newnode->child[newdirection] = *wherep;
  *wherep = -(cbtnodes - 1);
}

int stateqb, stateqe;
i64 stateq[statelimit];
i64 stateencode(int r, int c, int keys) {
  i64 rc = r * limit + c;
  return keys | (rc << 27);
}
void statedecode(int *r, int *c, int *keys, i64 state) {
  *keys = state & ((1 << 27) - 1);
  state >>= 27;
  *c = state % limit;
  *r = state / limit;
}

int main(void) {
  assert(freopen("18.in", "r", stdin) != NULL);
  int startr = -1, startc = -1;
  assert(scanf("%85s ", map[0]) == 1);
  cols = strlen(map[0]);
  rows = 1;
  while (scanf("%85s ", map[rows]) == 1) {
    assert((int)strlen(map[rows]) == cols);
    for (int c = 0; c < cols; c++) {
      if (map[rows][c] == '@') {
        startr = rows;
        startc = c;
        map[rows][c] = '.';
      }
      if (islower(map[rows][c])) allkeys |= 1 << (map[rows][c] - 'a');
    }
    rows++;
  }
  assert(feof(stdin));

  int r, c, keys, dist;
  stateq[stateqe++] = stateencode(startr, startc, 0);
  addentry(stateq[0], 0);
  while (stateqb != stateqe) {
    statedecode(&r, &c, &keys, stateq[stateqb]);
    dist = getvalue(stateq[stateqb++]);
    if (keys == allkeys) break;
    static const int dr[] = { -1, 0, 1, 0 };
    static const int dc[] = { 0, 1, 0, -1 };
    for (int d = 0; d < 4; d++) {
      int nr = r + dr[d];
      int nc = c + dc[d];
      int nkeys = keys;
      int ndist = dist + 1;
      if (map[nr][nc] == '#') continue;
      if (islower(map[nr][nc])) nkeys |= 1 << (map[nr][nc] - 'a');
      if (isupper(map[nr][nc])) {
        int door = 1 << (map[nr][nc] - 'A');
        if ((keys & door) == 0) continue;
      }
      i64 nstate = stateencode(nr, nc, nkeys);
      if (getvalue(nstate) != -1) continue;
      addentry(nstate, ndist);
      assert(stateqe < statelimit);
      stateq[stateqe++] = nstate;
    }
  }
  printf("part1: %d (%d nodes used)\n", dist, cbtnodes);
  return 0;
}
