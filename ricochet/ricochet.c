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

enum { hashlimit = 50000000 * 3 + 1 };
enum { moveslimit = 50 };

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
  // estimated distance via the astar heuristic.
  uint8_t estimateddist;
  // index into board.q to a state with the same hash value as the current
  // state. basically this is the chain pointer for a hash bucket.
  int samehash;
  // index into board.q.
  int parent;
  // index into board.q. it is the next queued state that has the same distance
  // as the current state.
  int nextqueued;
};

struct board {
  uint8_t wall[256];
  // even means /, odd means \. rest of the bits represent the robot id.
  uint8_t mirror[256];
  // distance from target with minimum turns (used for the a* heuristic) for
  // each robot separately (due to mirrors).
  uint8_t dist[5][256];
  uint8_t robotpos[5];
  uint8_t targetpos;
  int targetid;

  // the size of the queue that this solver preallocated.
  int queuelimit;

  struct state *q;
  int qb[moveslimit], qe;

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

void dumpstate(const struct state *s) {
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

  // calculate the a* heuristic distance.
  memset(&board.dist, 255, sizeof(board.dist));
  for (int robot = 0; robot < 5; robot++) {
    int q[256];
    int qb = 0, qe = 1;
    q[0] = board.targetpos;
    board.dist[robot][board.targetpos] = 0;
    while (qb != qe) {
      int pos = q[qb++];
      for (int dir = 0; dir < 4; dir++) {
        // this logic is similar to the one below in the main loop.
        int curdir = dir;
        int wallcheck = 1 << curdir;
        int newpos = pos;
        while (true) {
          if ((board.wall[newpos] & wallcheck) != 0) break;
          newpos += deltapos[curdir];
          if ((board.wall[newpos] & wallmirror) != 0) {
            if (robot != board.mirror[newpos] / 2) {
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
          if (board.dist[robot][newpos] != 255) continue;
          board.dist[robot][newpos] = board.dist[robot][pos] + 1;
          q[qe++] = newpos;
        }
      }
    }
  }

  // allocate all the memory for the data structures.
  board.visitedhash = calloc(hashlimit, sizeof(board.visitedhash[0]));
  if (board.visitedhash == NULL) {
    puts("error: not enough ram for running the solver, giving up.");
    return;
  }
  board.queuelimit = 50000000;
  board.q = calloc(board.queuelimit, sizeof(board.q[0]));
  if (board.q == NULL) {
    puts("warning: not much ram available, running the small scale solver.");
    board.queuelimit /= 10;
    board.q = calloc(board.queuelimit, sizeof(board.q[0]));
    board.visitedhash = calloc(hashlimit, sizeof(board.visitedhash[0]));
    if (board.q == NULL) {
      free(board.q);
      free(board.visitedhash);
      puts("error: not enough ram for that either, giving up.");
      return;
    }
  }

  // run the a*. note that this solution does not use a priority queue but
  // rather has a separate queue for each distance bucket. each bucket's queue
  // is a linked list of states (linked via state.nextqueued).
  board.qb[0] = 1;
  board.qe = 2;
  memcpy(board.q[1].pos, board.robotpos, sizeof(board.robotpos));
  board.visitedhash[hashstate(&board.q[1])] = 1;
  int maxchain = 0;
  int currentdist = 0;
  int visited = 0;
  // soi - state of interest (use for debugging).
  int soi = -1;
  while (currentdist < moveslimit) {
    if (board.qb[currentdist] == 0) {
      currentdist++;
      continue;
    }
    // cs - current state.
    visited++;
    int csid = board.qb[currentdist];
    struct state cs = board.q[csid];
    board.qb[currentdist] = cs.nextqueued;
    if (cs.estimateddist != currentdist) {
      // processed this state earlier at a lower distance (this is the cost of
      // using singly linked lists).
      continue;
    }
    if (csid == soi) {
      printf("\e[H\e[Jparent state %d (dist=%d):\n", soi, cs.dist);
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
        if (board.qe == board.queuelimit) {
          printf("problem too hard for this solver, quitting.\n");
          printf("gave up at %d dist.\n", cs.dist);
          exit(1);
        }
        struct state ns = cs;
        ns.pos[robot] = newpos;
        ns.dist = cs.dist + 1;
        int distleft = 999;
        for (int rob = 0; rob < 5; rob++) {
          if (board.targetid != -1 && board.targetid != rob) continue;
          int rd = board.dist[rob][ns.pos[rob]];
          if (rd < distleft) {
            distleft = rd;
          }
        }
        ns.estimateddist = ns.dist + distleft;
        if (ns.estimateddist >= moveslimit) continue;
        int h = hashstate(&ns);
        bool visited = false;
        int chain = 0;
        int *stateidp = &board.visitedhash[h];
        while (*stateidp != 0) {
          chain++;
          struct state *hs = &board.q[*stateidp];
          if (memcmp(hs->pos, ns.pos, 5) == 0) {
            if (hs->estimateddist <= ns.estimateddist) {
              visited = true;
            } else {
              // this state is currently queued albeit with higher cost. simply
              // ignore that state in the main queue but do remove it from the
              // hashmap by unlinking it from the bucket chain.
              *stateidp = hs->samehash;
            }
            break;
          }
          stateidp = &hs->samehash;
        }
        if (chain > maxchain) maxchain = chain;
        if (visited) continue;
        ns.samehash = board.visitedhash[h];
        ns.parent = csid;
        board.visitedhash[h] = board.qe;
        ns.nextqueued = board.qb[ns.estimateddist];
        board.qb[ns.estimateddist] = board.qe;
        board.q[board.qe] = ns;
        if (csid == soi) {
          printf("new state %d (edist=%d):\n", board.qe, ns.estimateddist);
          dumpstate(&ns);
        }
        board.qe++;
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
  if (currentdist == moveslimit) {
    printf("did not find a solution in %d steps.\n", moveslimit - 1);
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
  printf("nodes visited: %9.6f M\n", visited / 1e6);
  printf("total nodes:   %9.6f M\n", (board.qe - 1) / 1e6);
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
