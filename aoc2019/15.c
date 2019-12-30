#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum { limit = 100000 };
enum { inputsentinel = -47 };

enum addressmode {
  addressmodeposition,
  addressmodeimmediate,
  addressmoderelative,
};

int n;
struct vm {
  int ip;
  int basep;
  long long mem[limit + 3];
};

static inline void checkrange(int x) {
  assert(0 <= x && x < limit);
}

// cvm = current virtual machine to save passing args.
struct vm *cvm;
static inline long long *get(int ptr, enum addressmode amode) {
  checkrange(ptr);
  if (amode == addressmodeimmediate) return &cvm->mem[ptr];
  if (amode == addressmodeposition) {
    checkrange(cvm->mem[ptr]);
    return &cvm->mem[cvm->mem[ptr]];
  }
  assert(amode == addressmoderelative);
  long long p;
  assert(!__builtin_add_overflow(cvm->basep, cvm->mem[ptr], &p));
  checkrange(p);
  return &cvm->mem[p];
}
void init(struct vm *vm, long long *cmem) {
  vm->ip = 0;
  vm->basep = 0;
  memcpy(vm->mem, cmem, n * sizeof(cmem[0]));
}
bool getoutput(struct vm *vm, long long *output, long long **input) {
  int ip = vm->ip;
  long long *mem = vm->mem;
  cvm = vm;
  while (0 <= ip && ip < limit && mem[ip] != 99) {
    enum addressmode mode1 = addressmodeposition;
    enum addressmode mode2 = addressmodeposition;
    enum addressmode mode3 = addressmodeposition;
    long long instr = mem[ip];
    if (instr > 20000) {
      instr -= 20000;
      mode3 = addressmoderelative;
    } else if (instr > 10000) {
      instr -= 10000;
      mode3 = addressmodeimmediate;
    }
    if (instr > 2000) {
      instr -= 2000;
      mode2 = addressmoderelative;
    } else if (instr > 1000) {
      instr -= 1000;
      mode2 = addressmodeimmediate;
    }
    if (instr > 200) {
      instr -= 200;
      mode1 = addressmoderelative;
    } else if (instr > 100) {
      instr -= 100;
      mode1 = addressmodeimmediate;
    }
    int op = instr;

    long long a, b, r;
    switch (op) {
    case 0:
      assert(false);
    case 1:
      // add
      a = *get(ip + 1, mode1);
      b = *get(ip + 2, mode2);
      assert(!__builtin_add_overflow(a, b, &r));
      *get(ip + 3, mode3) = r;
      ip += 4;
      break;
    case 2:
      // mul
      a = *get(ip + 1, mode1);
      b = *get(ip + 2, mode2);
      assert(!__builtin_mul_overflow(a, b, &r));
      *get(ip + 3, mode3) = r;
      ip += 4;
      break;
    case 3:
      // input
      checkrange(mem[ip + 1]);
      r = **input;
      (*input)++;
      assert(r != inputsentinel);
      *get(ip + 1, mode1) = r;
      ip += 2;
      break;
    case 4:
      // output
      *output = *get(ip + 1, mode1);
      ip += 2;
      vm->ip = ip;
      return true;
    case 5:
      // jump-if-true
      if (*get(ip + 1, mode1) != 0) {
        ip = *get(ip + 2, mode2);
      } else {
        ip += 3;
      }
      break;
    case 6:
      // jump-if-true
      if (*get(ip + 1, mode1) == 0) {
        ip = *get(ip + 2, mode2);
      } else {
        ip += 3;
      }
      break;
    case 7:
      // less-than
      *get(ip + 3, mode3) = *get(ip + 1, mode1) < *get(ip + 2, mode2);
      ip += 4;
      break;
    case 8:
      // equals
      *get(ip + 3, mode3) = *get(ip + 1, mode1) == *get(ip + 2, mode2);
      ip += 4;
      break;
    case 9:
      // adjust the relative base
      vm->basep += *get(ip + 1, mode1);
      checkrange(vm->basep);
      ip += 2;
      break;
    default:
      assert(false);
    }
  }
  vm->ip = ip;
  assert(0 <= ip && ip < limit && mem[ip] == 99);
  return false;
}

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
    long long inputbuf[] = { d + 1, inputsentinel };
    long long *input = inputbuf;
    long long output;
    assert(getoutput(&vm, &output, &input));
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
    inputbuf[0] = (d ^ 1) + 1;
    input = inputbuf;
    assert(getoutput(&vm, &output, &input));
    assert(output == 1 || output == 2);
  }
}

int main(void) {
  assert(freopen("15.in", "r", stdin) != NULL);
  long long mem[limit];
  assert(scanf("%lld", &mem[n++]) == 1);
  while (scanf(",%lld", &mem[n]) == 1) {
    n++;
    assert(n < limit);
  }
  scanf(" ");
  assert(feof(stdin));
  init(&vm, mem);
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
