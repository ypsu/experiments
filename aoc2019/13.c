#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum { limit = 100000 };
enum { inputsentinel = -47 };
enum { gridlimit = 1000 };
enum { screensize = 50 };

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

int main(void) {
  assert(freopen("13.in", "r", stdin) != NULL);
  long long mem[limit];
  assert(scanf("%lld", &mem[n++]) == 1);
  while (scanf(",%lld", &mem[n]) == 1) {
    n++;
    assert(n < limit);
  }
  scanf(" ");
  assert(feof(stdin));
  struct vm vm;
  init(&vm, mem);
  char screen[screensize][screensize] = {};
  while (true) {
    long long input = inputsentinel;
    long long *inp = &input;
    long long x, y, tile;
    if (!getoutput(&vm, &x, &inp)) break;
    assert(getoutput(&vm, &y, &inp));
    assert(getoutput(&vm, &tile, &inp));
    assert(0 <= tile && tile <= 4);
    assert(0 <= x && x < screensize);
    assert(0 <= y && y < screensize);
    screen[y][x] = tile;
  }
  int blocks = 0;
  for (int y = 0; y < screensize; y++) {
    for (int x = 0; x < screensize; x++) {
      if (screen[y][x] == 2) blocks++;
    }
  }

  init(&vm, mem);
  vm.mem[0] = 2;
  memset(screen, 0, sizeof(screen));
  long long score = -1;
  long long ballx = 0, paddlex = 0;
  bool print = false;
  while (true) {
    long long input[] = {0, inputsentinel};
    long long *inp = input;
    if (paddlex < ballx) input[0] = +1;
    if (paddlex > ballx) input[0] = -1;
    while (true) {
      long long x, y, tile;
      if (!getoutput(&vm, &x, &inp)) break;
      assert(getoutput(&vm, &y, &inp));
      assert(getoutput(&vm, &tile, &inp));
      if (x == -1 && y == 0) {
        score = tile;
      } else {
        assert(0 <= x && x < 40);
        assert(0 <= y && y < 25);
        assert(0 <= tile && tile <= 4);
        screen[y][x] = tile;
        if (tile == 3) paddlex = x;
        if (tile == 4) ballx = x;
      }
      if (tile == 4) break;
    }
    if (print) printf("\e[H\e[J");
    int curblocks = 0;
    for (int y = 0; y < 25; y++) {
      for (int x = 0; x < 40; x++) {
        if (screen[y][x] == 2) curblocks++;
        if (print) {
          switch (screen[y][x]) {
          case 1:
            putchar('#');
            break;
          case 2:
            putchar('.');
            break;
          case 3:
            putchar('_');
            break;
          case 4:
            putchar('*');
            break;
          default:
            putchar(' ');
            break;
          }
        }
      }
      if (print) putchar('\n');
    }
    if (print) {
      printf("score: %lld\n", score);
      printf("blocks: %d\n", curblocks);
      usleep(250000);
    }
    if (curblocks == 0) break;
  }

  printf("part1: %d\n", blocks);
  printf("part2: %lld\n", score);
  return 0;
}
