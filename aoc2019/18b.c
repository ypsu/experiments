// Meh, I'm not proud of this. Oh well.

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int64_t i64;
typedef int8_t i8;

enum { limit = 128 };
enum { statelimit = 10000000 };
int rows, cols;
char map[limit][limit];
int keypos[30][2];
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
const i64 keybits = 46;
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
  assert(value < 8000);
  assert(cbtnodes < statelimit);
  i64 p = cbtroot;
  if (p == 0) {
    cbtroot = key | (value << keybits);
    return;
  }
  i64 *pp = &cbtroot;
  while (p < 0) {
    struct cbtnode *n = &cbtnode[-p];
    int direction = (1 + (n->otherbits | key)) >> keybits;
    pp = &n->child[direction];
    p = n->child[direction];
  }
  if ((p & keymask) == key) {
    // allow lowering value.
    assert(value < (p >> keybits));
    *pp = key | (value << keybits);
    return;
  }
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

struct state {
  i8 pos[4];
  int keys;
  int dist;
} state[statelimit];
int states;

int heapsz;
int heap[statelimit];

void heappush(int stateid) {
  assert(heapsz < statelimit);
  int pos = ++heapsz;
  int dist = state[stateid].dist;
  while (pos > 1 && dist < state[heap[pos / 2]].dist) {
    heap[pos] = heap[pos / 2];
    pos /= 2;
  }
  heap[pos] = stateid;
}
void heappop(void) {
  assert(heapsz > 0);
  heap[1] = heap[heapsz--];
  int pos = 1;
  while (pos * 2 <= heapsz) {
    int npos = pos;
    if (state[heap[pos * 2]].dist < state[heap[pos]].dist) {
      npos = pos * 2;
    }
    if (pos * 2 + 1 <= heapsz) {
      if (state[heap[pos * 2 + 1]].dist < state[heap[npos]].dist) {
        npos = pos * 2 + 1;
      }
    }
    if (npos == pos) break;
    int t = heap[npos];
    heap[npos] = heap[pos];
    heap[pos] = t;
    pos = npos;
  }
}
void checkheap(void) {
  for (int i = 1; i <= heapsz / 2; i++) {
    assert(state[heap[i]].dist <= state[heap[i * 2]].dist);
    if (i * 2 + 1 <= heapsz) {
      assert(state[heap[i]].dist <= state[heap[i * 2 + 1]].dist);
    }
  }
}

i64 stateencode(struct state *state) {
  i64 code = state->keys;
  for (int i = 0; i < 4; i++) {
    code |= ((long long)state->pos[i]) << (26 + i * 5);
  }
  return code;
}
void statedecode(struct state *state, i64 code) {
  state->keys = code & ((1 << 26) - 1);
  code >>= 26;
  for (int i = 0; i < 4; i++) {
    state->pos[i] = code & 31;
    code >>= 5;
  }
}

int curtime;
int lastvisit[limit][limit];
int foundcnt;
int currobot;
struct state basestate;
struct state found[100];
// TODO: This should be bfs. :(
void dfs(int r, int c, int keys, int dist) {
  if (lastvisit[r][c] == curtime) return;
  lastvisit[r][c] = curtime;
  if (map[r][c] == '#') return;
  if (isupper(map[r][c])) {
    int door = 1 << (map[r][c] - 'A');
    if ((keys & door) == 0) return;
  }
  if (islower(map[r][c])) {
    int key = 1 << (map[r][c] - 'a');
    if ((key & keys) == 0) {
      keys |= key;
      struct state s = basestate;
      s.keys |= keys;
      s.pos[currobot] = map[r][c] - 'a';
      s.dist = dist;
      assert(foundcnt < 100);
      found[foundcnt++] = s;
      return;
    }
  }
  static const int dr[] = { -1, 0, 1, 0 };
  static const int dc[] = { 0, 1, 0, -1 };
  for (int d = 0; d < 4; d++) {
    dfs(r + dr[d], c + dc[d], keys, dist + 1);
  }
}

int main(void) {
  assert(freopen("18.in", "r", stdin) != NULL);
  assert(scanf("%85s ", map[0]) == 1);
  cols = strlen(map[0]);
  rows = 1;
  int sr = -1, sc = -1;
  while (scanf("%85s ", map[rows]) == 1) {
    assert((int)strlen(map[rows]) == cols);
    for (int c = 0; c < cols; c++) {
      if (map[rows][c] == '@') {
        sr = rows;
        sc = c;
      }
      if (islower(map[rows][c])) {
        int key = map[rows][c] - 'a';
        allkeys |= 1 << key;
        keypos[key][0] = rows;
        keypos[key][1] = c;
      }
    }
    rows++;
  }
  assert(feof(stdin));
  assert(sr != -1 && sc != -1);
  map[sr][sc] = '#';
  map[sr - 1][sc] = '#';
  map[sr][sc - 1] = '#';
  map[sr + 1][sc] = '#';
  map[sr][sc + 1] = '#';
  keypos[26][0] = sr - 1;
  keypos[26][1] = sc - 1;
  keypos[27][0] = sr + 1;
  keypos[27][1] = sc - 1;
  keypos[28][0] = sr - 1;
  keypos[28][1] = sc + 1;
  keypos[29][0] = sr + 1;
  keypos[29][1] = sc + 1;

  struct state s = {};
  s.pos[0] = 26;
  s.pos[1] = 27;
  s.pos[2] = 28;
  s.pos[3] = 29;
  state[states] = s;
  addentry(stateencode(&s), 0);
  heappush(states++);
  while (true) {
    assert(heapsz >= 1);
    curtime++;
    s = state[heap[1]];
    if (s.keys == allkeys) break;
    heappop();
    i64 statecode = stateencode(&s);
    //printf("checking state: d:%d k:%d\n", s.dist, s.keys);
    if (getvalue(statecode) != s.dist) continue;
    //printf("processing state: d:%d k:%d\n", s.dist, s.keys);
    basestate = s;
    foundcnt = 0;
    for (currobot = 0; currobot < 4; currobot++) {
      int pos = s.pos[currobot];
      dfs(keypos[pos][0], keypos[pos][1], s.keys, s.dist);
    }
    //printf("found %d states\n", foundcnt);
    for (int f = 0; f < foundcnt; f++) {
      struct state ns = found[f];
      i64 nsc = stateencode(&ns);
      int nsd = getvalue(nsc);
      if (nsd != -1 && nsd <= ns.dist) continue;
      addentry(nsc, ns.dist);
      state[states] = ns;
      heappush(states++);
    }
  }
  printf("part2: %d\n", state[heap[1]].dist);
  return 0;
}
