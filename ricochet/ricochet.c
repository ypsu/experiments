// ricochet robots (v2) solver. it uses the a star path finding algorithm. the
// problem works on a 16x16 grid so it encodes the position as uint8 (0..255).
// there are 5 robots so a "state" is completely described by 5 positions. there
// are additional data it needs to track: how far each state is from the initial
// position, what is the parent state we came from. all this is represented
// quite tightly to fit into 8 bytes per state.
//
// the most important concept here is the solver's hashtable found at
// board.hashtable. it is an open addressed, linearly probed, flat hashtable
// where each bucket fully describes a state. the solver hashes into the
// hashtable via treating the 5 robot positions as a 5 byte long integer and
// mods it by the hashtable's size.
//
// there is one (probably unnecessary) trick to the hashtable. rather than using
// the struct state directly as its type, the solver stores it along with an
// union with an uint64. it only does it so that it is easy to access the whole
// state as a number and thus to do a memcmp of the 5 bytes simply via binary
// and-ing with 0xffffff ff000000. this way the solver relies less on a
// "sufficiently smart compiler" doing its job. compile with
// -fno-strict-aliasing if type aliasing gives you nightmares.
//
// the a star's queue (board.q) is a linked list of work items bucketed per
// distance to avoid maintaining a logarithmic heap. the only other data other
// than the linked list pointers is just an index into the hashtable that
// describes the state the queue item refers to.
//
// the solver preallocates both the queue and the hashtable arrays and then
// never does any new allocation.

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <signal.h>
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

// pstate: process state sampler. this implements a utility that periodically
// wakes up via the alrm signal (~1000 hz) and samples the "pstate" global
// variable which is just an enum. the solver uses this to estimate which
// sections take up the bulk of the runtime. think of this as a poor man's linux
// perf.
enum { sampleslimit = 1000000 };

enum pstate {
  pstateunknown,
  pstateinit,
  pstatecalcmove,
  pstatefirsthashlookup,
  pstatehashiteration,
  pstateenqueue,
  pstateother,
  pstateend,
};
static volatile enum pstate pstatecurrent;
static int pstatesamplecnt;
static enum pstate pstatesample[sampleslimit];

static const char pstatestr[][32] = {
  "unknown",
  "init",
  "calcmove",
  "firsthashlookup",
  "hashiteration",
  "enqueue",
  "other",
  "end",
};

static void pstatetakesample(int sig) {
  (void)sig;
  check(pstatesamplecnt < sampleslimit);
  pstatesample[pstatesamplecnt++] = pstatecurrent;
}

// since i want to skip implementing wraparound during hashtable iteration when
// resolving a collision, allocate a few extra buckets at the end.
enum { extrabuckets = 1000 };
enum { moveslimit = 30 };

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
  uint8_t parentpos;
  uint8_t parentbot : 3;
  uint8_t dist : 5;
  // estimated distance via the astar heuristic.
  uint8_t estimateddist;
};
// make it possible to treat the hash buckets as a simple integer so that we can
// very quickly iterate through it when working through collisions. the position
// itself will always be the first 5 bytes but all you need to access it is to
// bit-and with posmask.
static const uint64_t posmask = 0xffffffffffll;
union stateorlong {
  uint64_t aslong;
  struct state asstate;
};
static_assert(sizeof(union stateorlong) == 8, "stateorlong must be 8 bytes.");

struct queueitem {
  // index into hashtable where the queued item is.
  int hashidx;
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

  // the size of the hashtable and queue that this solver preallocated.
  int hashlimit;
  int queuelimit;

  union stateorlong *hashtable;
  struct queueitem *q;
  int qb[moveslimit], qe;

  // the solution appears here.
  int moves;
  int solution[moveslimit];
};
static struct board board;

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
  board.queuelimit = 50000000;
  board.hashlimit = board.queuelimit * 2 + 19;
  int hashsz = board.hashlimit + extrabuckets;
  board.hashtable = calloc(hashsz, sizeof(board.hashtable[0]));
  board.q = calloc(board.queuelimit, sizeof(board.q[0]));
  if (board.hashtable == NULL || board.q == NULL) {
    free(board.hashtable);
    free(board.q);
    puts("warning: not much ram available, running the small scale solver.");
    board.queuelimit /= 10;
    board.hashlimit = board.queuelimit * 2 + 19;
    hashsz = board.hashlimit + extrabuckets;
    board.hashtable = calloc(hashsz, sizeof(board.hashtable[0]));
    board.q = calloc(board.queuelimit, sizeof(board.q[0]));
    if (board.hashtable == NULL || board.q == NULL) {
      free(board.hashtable);
      free(board.q);
      puts("error: not enough ram for that either, giving up.");
      return;
    }
  }
  memset(board.hashtable, 0, hashsz * sizeof(board.hashtable[0]));
  memset(board.q, 0, board.queuelimit * sizeof(board.q[0]));

  // run the a*. note that this solution does not use a priority queue but
  // rather has a separate queue for each distance bucket. each bucket's queue
  // is a linked list of states (linked via state.nextqueued).
  // cs means "current state";
  union stateorlong cs = {};
  memcpy(&cs.asstate.pos, board.robotpos, sizeof(board.robotpos));
  int hashpos = cs.aslong % board.hashlimit;
  board.hashtable[hashpos] = cs;
  board.q[1].hashidx = hashpos;
  board.qb[0] = 1;
  board.qe = 2;
  int maxchain = 0;
  int currentdist = 0;
  int visited = 0;
  // soi - state of interest (use for debugging). it's a hashidx.
  int soi = -1;  // hashpos;
  while (currentdist < moveslimit) {
    if (board.qb[currentdist] == 0) {
      currentdist++;
      continue;
    }
    visited++;
    // csidx - current state's hashidx.
    int csidx = board.q[board.qb[currentdist]].hashidx;
    cs = board.hashtable[csidx];
    board.qb[currentdist] = board.q[board.qb[currentdist]].nextqueued;
    if (cs.asstate.estimateddist != currentdist) {
      // processed this state earlier at a lower distance (this is the cost of
      // using singly linked lists).
      continue;
    }
    if (csidx == soi) {
      printf("\e[H\e[Jparent state %d (dist=%d):\n", soi, cs.asstate.dist);
      dumpstate(&cs.asstate);
    }
    for (int robot = 0; robot < 5; robot++) {
      board.wall[cs.asstate.pos[robot]] |= wallrobot;
    }
    for (int robot = 0; robot < 5; robot++) {
      pstatecurrent = pstatecalcmove;
      board.wall[cs.asstate.pos[robot]] &= ~wallrobot;
      for (int dir = 0; dir < 4; dir++) {
        // move the robot in the direction while possible.
        int curdir = dir;
        int wallcheck = 1 << curdir;
        int newpos = cs.asstate.pos[robot];
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
        if (newpos == cs.asstate.pos[robot]) continue;
        if ((board.wall[newpos] & wallmirror) != 0) continue;
        // add the new state to the queue.
        if (board.qe == board.queuelimit) {
          printf("problem too hard for this solver, quitting.\n");
          printf("gave up at %d dist.\n", cs.asstate.dist);
          exit(1);
        }
        union stateorlong ns = cs;
        ns.asstate.pos[robot] = newpos;
        int hashpos = (ns.aslong & posmask) % board.hashlimit;
        ns.asstate.dist = cs.asstate.dist + 1;
        int distleft = 999;
        for (int rob = 0; rob < 5; rob++) {
          if (board.targetid != -1 && board.targetid != rob) continue;
          int rd = board.dist[rob][ns.asstate.pos[rob]];
          if (rd < distleft) {
            distleft = rd;
          }
        }
        if (ns.asstate.dist + distleft >= moveslimit) continue;
        ns.asstate.estimateddist = ns.asstate.dist + distleft;
        bool visited = false;
        int chain = 0;
        uint64_t nposbits = ns.aslong & posmask;
        uint64_t *posptr = &board.hashtable[hashpos].aslong;
        pstatecurrent = pstatefirsthashlookup;
        while (*posptr != 0) {
          pstatecurrent = pstatehashiteration;
          check(hashpos < board.hashlimit + extrabuckets);
          if ((*posptr & posmask) == nposbits) {
            union stateorlong *hs = &board.hashtable[hashpos];
            if (hs->asstate.estimateddist <= ns.asstate.estimateddist) {
              visited = true;
            }
            break;
          }
          hashpos++;
          chain++;
          posptr++;
        }
        pstatecurrent = pstateenqueue;
        if (chain > maxchain) maxchain = chain;
        if (visited) continue;
        ns.asstate.parentbot = (uint8_t)robot;
        ns.asstate.parentpos = cs.asstate.pos[robot];
        board.hashtable[hashpos] = ns;
        board.q[board.qe].hashidx = hashpos;
        board.q[board.qe].nextqueued = board.qb[ns.asstate.estimateddist];
        board.qb[ns.asstate.estimateddist] = board.qe;
        board.qe++;
        if (csidx == soi) {
          int edist = ns.asstate.estimateddist;
          printf("new state %d (edist=%d):\n", hashpos, edist);
          dumpstate(&ns.asstate);
        }
        pstatecurrent = pstateother;
        if (newpos == board.targetpos) {
          if (board.targetid == -1 || board.targetid == robot) {
            goto foundtarget;
          }
        }
      }
      board.wall[cs.asstate.pos[robot]] |= wallrobot;
    }
    for (int robot = 0; robot < 5; robot++) {
      board.wall[cs.asstate.pos[robot]] &= ~wallrobot;
    }
  }
foundtarget:
  if (currentdist == moveslimit) {
    printf("did not find a solution in %d steps.\n", moveslimit - 1);
  } else {
    int hashpos = board.q[board.qe - 1].hashidx;
    struct state s = board.hashtable[hashpos].asstate;
    board.moves = s.dist;
    printf("solution %d\n", board.moves);
    board.solution[board.moves] = hashpos;
    for (int i = 0; i < board.moves; i++) {
      union stateorlong parent = board.hashtable[hashpos];
      struct state current = parent.asstate;
      parent.asstate.pos[current.parentbot] = current.parentpos;
      uint64_t cmp = parent.aslong & posmask;
      hashpos = cmp % board.hashlimit;
      while ((board.hashtable[hashpos].aslong & posmask) != cmp) {
        hashpos++;
        check(hashpos < board.hashlimit + extrabuckets);
      }
      board.solution[board.moves - 1 - i] = hashpos;
    }
    check(board.q[1].hashidx == hashpos);
    for (int i = 1; i <= board.moves; i++) {
     struct state os = board.hashtable[board.solution[i - 1]].asstate;
     struct state ns = board.hashtable[board.solution[i]].asstate;
     int r;
     for (r = 0; r < 5; r++) {
       if (os.pos[r] == ns.pos[r]) continue;
       printf("move %d %d %d\n", r, ns.pos[r] % 16, ns.pos[r] / 16);
       break;
     }
     check(r < 5);
    }
  }
  printf("\n");
  printf("nodes visited: %9.6f M\n", visited / 1e6);
  printf("total nodes:   %9.6f M\n", (board.qe - 1) / 1e6);
  printf("max hash chain: %d\n", maxchain);
  free(board.hashtable);
  free(board.q);
  struct timeval endtime, deltatime;
  check(gettimeofday(&endtime, NULL) == 0);
  timersub(&endtime, &starttime, &deltatime);
  double seconds = deltatime.tv_sec + deltatime.tv_usec / 1e6;
  printf("elapsed wall time: %0.1f seconds\n", seconds);
}

#ifndef __EMSCRIPTEN__
int main(void) {
  // set up the pstate sampler.
  int hz = 1000;
  pstatecurrent = pstateinit;
  check(signal(SIGALRM, pstatetakesample) != SIG_ERR);
  struct itimerval itval;
  itval.it_interval.tv_sec = 0;
  itval.it_interval.tv_usec = 1000000 / hz;
  itval.it_value = itval.it_interval;
  check(setitimer(ITIMER_REAL, &itval, NULL) == 0);

  // read the input and run the solver.
  char input[1024];
  int len = fread(input, 1, 1023, stdin);
  check(len >= 0 && len < 1023);
  input[len] = 0;
  solve(input);

  // disable the pstate sampler and dump its statistics.
  itval = (struct itimerval){};
  check(setitimer(ITIMER_REAL, &itval, NULL) == 0);
  printf("stats from %d samples (%d hz):\n", pstatesamplecnt, hz);
  int cnt[pstateend] = {};
  for (int i = 0; i < pstatesamplecnt; i++) cnt[pstatesample[i]]++;
  for (int i = pstateinit; i < pstateend; i++) {
    const char *str = pstatestr[i];
    double seconds = cnt[i] * 1.0 / hz;
    printf("%20s: %5d (%4.1f s)\n", str, cnt[i], seconds);
  }
  return 0;
}
#endif  // !__EMSCRIPTEN__
