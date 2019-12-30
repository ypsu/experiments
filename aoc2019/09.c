#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
void run(struct vm *vm, long long *input) {
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
      r = *input++;
      assert(r != inputsentinel);
      *get(ip + 1, mode1) = r;
      ip += 2;
      break;
    case 4:
      // output
      printf("%lld\n", *get(ip + 1, mode1));
      ip += 2;
      vm->ip = ip;
      break;
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
}

int main(void) {
  assert(freopen("09.in", "r", stdin) != NULL);
  long long mem[limit];
  assert(scanf("%lld", &mem[n++]) == 1);
  while (scanf(",%lld", &mem[n]) == 1) {
    n++;
    assert(n < limit);
  }
  scanf(" ");
  assert(feof(stdin));
  struct vm vm;
  printf("part1: ");
  init(&vm, mem);
  long long input[] = { 1, inputsentinel };
  run(&vm, input);
  printf("part2: ");
  init(&vm, mem);
  input[0] = 2;
  run(&vm, input);
  return 0;
}
