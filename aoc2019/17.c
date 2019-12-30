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

enum { gridlimit = 60 };
char grid[gridlimit + 1][gridlimit + 1];

int main(void) {
  assert(freopen("17.in", "r", stdin) != NULL);
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

  long long inputbuf[1000] = { inputsentinel };
  long long *input = inputbuf;
  long long output;
  int curr = 1, curc = 1;
  while (getoutput(&vm, &output, &input)) {
    assert('\n' <= output && output <= 'v');
    if (output == '\n') {
      curr++;
      curc = 1;
    } else {
      assert(curr < gridlimit && curc < gridlimit);
      if (output == '<' || output == '>' || output == '^' || output == 'v') {
        output = '#';
      }
      grid[curr][curc++] = output;
    }
  }
  int asum = 0;
  for (int r = 1; r < gridlimit; r++) {
    for (int c = 1; c < gridlimit; c++) {
      if (grid[r][c] != '#') continue;
      if (grid[r][c - 1] != '#') continue;
      if (grid[r][c + 1] != '#') continue;
      if (grid[r - 1][c] != '#') continue;
      if (grid[r + 1][c] != '#') continue;
      asum += (r - 1) * (c - 1);
    }
  }
  printf("part1: %d\n", asum);

  init(&vm, mem);
  vm.mem[0] = 2;
  char moves[] = "A,B,A,B,C,B,A,C,B,C\nL,12,L,8,R,10,R,10\nL,6,L,4,L,12\nR,10,L,8,L,4,R,10\nn\n";
  int isz;
  for (isz = 0; moves[isz] != 0; isz++) inputbuf[isz] = moves[isz];
  inputbuf[isz] = inputsentinel;
  input = inputbuf;
  bool hadmain = false;
  while (getoutput(&vm, &output, &input)) {
    if ('\n' > output || output > 127) break;
    if (output == 'M') hadmain = true;
    if (hadmain) putchar(output);
  }
  printf("part2: %lld\n", output);
  assert(!getoutput(&vm, &output, &input));
  return 0;
}
