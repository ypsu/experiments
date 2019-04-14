#define _GNU_SOURCE
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

// check is like an assert but always enabled.
#define check(cond) checkfunc(cond, #cond, __FILE__, __LINE__)
void checkfunc(bool ok, const char *s, const char *file, int line) {
  if (ok) return;
  printf("checkfail at %s %d %s\n", file, line, s);
  exit(1);
}

enum { is64 = sizeof(long) == 8 };
enum { queuelimit = 10000000 * (is64 ? 10 : 1) };
enum { hashlimit = queuelimit * 3 + 1 };
enum { moveslimit = 100 };

enum {
  wallright = 1,
  wallup = 2,
  wallleft = 4,
  walldown = 8,
  wallmirror = 16,
  wallrobot = 32,
};

static const int deltapos[4] = { 1, -16, -1, 16 };

struct state {
  uint8_t pos[5];
  uint8_t dist;
  // index into board.q to a state with the same hash value as the current
  // state. basically this is the chain pointer for a hash bucket.
  int samehash;
  // index into board.q.
  int parent;
};

struct board {
  uint8_t wall[256];
  // even means /, odd means \. rest of the bits represent the robot id.
  uint8_t mirror[256];
  uint8_t robotpos[5];
  uint8_t targetpos;
  int targetid;

  struct state *q;
  int qb, qe;

  // the buckets are indices into the q array.
  int *visitedhash;

  // the solution appears here.
  int moves;
  int solution[moveslimit];
} board;

static int hashstate(const struct state *s) {
  int64_t h = 0;
  memcpy(&h, s->pos, 5);
  return h % hashlimit;
}

static void dumpstate(const struct state *s) {
  for (int r = 0; r < 16; r++) {
    for (int c = 0; c < 16; c++) {
      putchar('+');
      if ((board.wall[r * 16 + c] & wallup) != 0) {
        putchar('-');
      } else {
        putchar(' ');
      }
    }
    putchar('+');
    putchar('\n');
    for (int c = 0; c < 16; c++) {
      if ((board.wall[r * 16 + c] & wallleft) != 0) {
        putchar('|');
      } else {
        putchar(' ');
      }
      bool foundrobot = false;
      for (int i = 0; i < 5; i++) {
        int row = s->pos[i] / 16;
        int col = s->pos[i] % 16;
        if (row == r && col == c) {
          putchar('0' + i);
          foundrobot = true;
          break;
        }
      }
      if (!foundrobot) {
        int pos = r * 16 + c;
        if (board.targetpos == pos) {
          putchar('t');
        } else if ((board.wall[pos] & wallmirror) != 0) {
          putchar(board.mirror[pos] % 2 == 0 ? '/' : '\\');
        } else {
          putchar(' ');
        }
      }
    }
    putchar('|');
    putchar('\n');
  }
  for (int c = 0; c < 16; c++) {
    putchar('+');
    putchar('-');
  }
  putchar('+');
  putchar('\n');
}

// solve the input, write the output to stdout.
void solve(char *inputbuf) {
  struct timeval starttime;
  check(gettimeofday(&starttime, NULL) == 0);

  // read the input.
  memset(&board, 0, sizeof(board));
  FILE *f = fmemopen(inputbuf, strlen(inputbuf), "r");
  check(f != NULL);
  char word[16];
  int row, col;
  check(fscanf(f, " %15s 16 16", word) == 1);
  check(strcmp(word, "board") == 0);
  for (int i = 0; i < 16; i++) {
    board.wall[i] |= wallup;
    board.wall[i * 16] |= wallleft;
    board.wall[(i + 1) * 16 - 1] |= wallright;
    board.wall[255 - i] |= walldown;
  }
  for (int i = 0; i < 256; i++) {
    row = i / 16;
    col = i % 16;
    char c;
    check(fscanf(f, " %c", &c) == 1);
    if (c == 'N' || c == 'B') {
      if (row > 0) board.wall[(row - 1) * 16 + col] |= walldown;
      board.wall[i] |= wallup;
    }
    if (c == 'W' || c == 'B') {
      if (col > 0) board.wall[row * 16 + col - 1] |= wallright;
      board.wall[i] |= wallleft;
    }
  }
  fscanf(f, " %15s", word);
  if (strcmp(word, "mirrors") == 0) {
    int m;
    check(fscanf(f, "%d", &m) == 1);
    for (int i = 0; i < m; i++) {
      int row, col, robot;
      char type;
      check(fscanf(f, "%d %d %c %d", &col, &row, &type, &robot) == 4);
      int pos = row * 16 + col;
      board.wall[pos] |= wallmirror;
      board.mirror[pos] = (type == '\\') | (robot << 1);
    }
    check(fscanf(f, " %15s", word) == 1);
  }
  check(strcmp(word, "robots") == 0);
  fscanf(f, " 5");
  for (int i = 0; i < 5; i++) {
    check(fscanf(f, "%d %d", &col, &row) == 2);
    board.robotpos[i] = row * 16 + col;
  }
  check(fscanf(f, " %15s 0", word) == 1);
  check(strcmp(word, "target") == 0);
  check(fscanf(f, "%d %d", &col, &row) == 2);
  board.targetpos = row * 16 + col;
  check(fscanf(f, "%d", &board.targetid) == 1);
  check(fclose(f) == 0);

  // allocate all the memory for the data structures.
  board.q = calloc(queuelimit, sizeof(board.q[0]));
  check(board.q != NULL);
  board.visitedhash = calloc(hashlimit, sizeof(board.visitedhash[0]));
  check(board.visitedhash != NULL);

  // run the bfs.
  board.qb = 1;
  board.qe = 2;
  memcpy(board.q[1].pos, board.robotpos, sizeof(board.robotpos));
  board.visitedhash[hashstate(&board.q[1])] = 1;
  int maxchain = 0;
  // soi - state of interest (use for debugging).
  int soi = -1;
  while (board.qb != board.qe) {
    // cs - current state.
    struct state cs = board.q[board.qb++];
    if (board.qb - 1 == soi) {
      printf("\e[H\e[Jparent state %d:\n", soi);
      dumpstate(&cs);
    }
    for (int robot = 0; robot < 5; robot++) {
      board.wall[cs.pos[robot]] |= wallrobot;
    }
    for (int robot = 0; robot < 5; robot++) {
      for (int dir = 0; dir < 4; dir++) {
        // move the robot in the direction while possible.
        int curdir = dir;
        int wallcheck = 1 << curdir;
        int newpos = cs.pos[robot];
        while (true) {
          if ((board.wall[newpos] & wallcheck) != 0) break;
          if ((board.wall[newpos + deltapos[curdir]] & wallrobot) != 0) break;
          newpos += deltapos[curdir];
          if ((board.wall[newpos] & wallmirror) != 0) {
            if (robot == board.mirror[newpos] / 2) continue;
            // right 0
            // up 1
            // left 2
            // down 3
            // /: 0->1, 1->0, 2->3, 3->2
            // \: 0->3, 1->2, 2->1, 3->0
            if (board.mirror[newpos] % 2 == 0) {
              int map[4] = { 1, 0, 3, 2 };
              curdir = map[curdir];
            } else {
              int map[4] = { 3, 2, 1, 0 };
              curdir = map[curdir];
            }
            wallcheck = 1 << curdir;
          }
        }
        if (newpos == cs.pos[robot]) continue;
        // add the new state to the queue.
        if (board.qe == queuelimit) {
          printf("problem too hard for this solver, quitting.\n");
          printf("gave up at %d dist.\n", cs.dist);
          exit(1);
        }
        struct state ns = cs;
        ns.pos[robot] = newpos;
        ns.dist = cs.dist + 1;
        int h = hashstate(&ns);
        bool visited = false;
        int stateid = board.visitedhash[h];
        int chain = 0;
        while (stateid != 0) {
          chain++;
          struct state *hs = &board.q[stateid];
          if (memcmp(hs->pos, ns.pos, 5) == 0) {
            visited = true;
            break;
          }
          stateid = hs->samehash;
        }
        if (chain > maxchain) maxchain = chain;
        if (visited) continue;
        if (board.qb - 1 == soi) {
          printf("new state %d:\n", board.qe);
          dumpstate(&ns);
        }
        ns.samehash = board.visitedhash[h];
        ns.parent = board.qb - 1;
        board.visitedhash[h] = board.qe;
        board.q[board.qe++] = ns;
        if (newpos == board.targetpos) {
          if (board.targetid == -1 || board.targetid == robot) {
            goto foundtarget;
          }
        }
      }
    }
    for (int robot = 0; robot < 5; robot++) {
      board.wall[cs.pos[robot]] &= ~wallrobot;
    }
  }
foundtarget:
  if (board.qb == board.qe) {
    printf("did not find a solution.\n");
  } else {
    board.moves = board.q[board.qe - 1].dist;
    printf("solution %d\n", board.moves);
    check(board.q[board.qe - 1].dist < moveslimit);
    int s = board.qe - 1;
    int i;
    for (i = 0; s >= 1 && i <= board.moves; i++) {
      board.solution[board.moves - i] = s;
      s = board.q[s].parent;
    }
    check(i == board.moves + 1 && s == 0);
    //dumpstate(&board.q[board.solution[0]]);
    for (i = 1; i <= board.moves; i++) {
      struct state *os = &board.q[board.solution[i - 1]];
      struct state *ns = &board.q[board.solution[i]];
      int r;
      for (r = 0; r < 5; r++) {
        if (os->pos[r] == ns->pos[r]) continue;
        printf("move %d %d %d\n", r, ns->pos[r] % 16, ns->pos[r] / 16);
        break;
      }
      check(r < 5);
      //dumpstate(ns);
    }
  }
  printf("\n");
  printf("nodes visited: %9.6f M\n", board.qb / 1e6);
  printf("total nodes:   %9.6f M\n", board.qe / 1e6);
  printf("max bucket chain: %d\n", maxchain);
  free(board.q);
  free(board.visitedhash);
  struct timeval endtime, deltatime;
  check(gettimeofday(&endtime, NULL) == 0);
  timersub(&endtime, &starttime, &deltatime);
  double seconds = deltatime.tv_sec + deltatime.tv_usec / 1e6;
  printf("elapsed wall time: %0.1f seconds\n", seconds);
}

#ifndef __EMSCRIPTEN__
int main(void) {
  char input[1024];
  int len = fread(input, 1, 1023, stdin);
  check(len >= 0 && len < 1023);
  input[len] = 0;
  solve(input);
  return 0;
}
#endif  // !__EMSCRIPTEN__
