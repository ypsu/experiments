#include <stdbool.h>
#include <stdio.h>

struct vm;
void icinitfromstdin(struct vm *vm);
void icinitfromfile(struct vm *vm, FILE *file);
void icreset(struct vm *vm);
bool ichalted(struct vm *vm);
void iccheckhalted(struct vm *vm);
bool ichasoutput(struct vm *vm);
long long icgetnum(struct vm *vm);
// the contents of the static buffer for these two functions is only guaranteed
// until the next call. the two functions share the static buffer.
char *icgetline(struct vm *vm);
char *icgetstring(struct vm *vm);
void icputnum(struct vm *vm, long long num);
void icputstring(struct vm *vm, const char *str);
void icrun(struct vm *vm);


#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

enum { limit = 100000 };

enum addressmode {
  addressmodeposition,
  addressmodeimmediate,
  addressmoderelative,
};

int n;
struct vm {
  int ip;
  int basep;
  int instart, inend;
  int outstart, outend;
  long long input[limit];
  long long output[limit];
  long long mem[limit + 3];
  long long savedmem[limit];
};

void icinitfromstdin(struct vm *vm) {
  icinitfromfile(vm, stdin);
}

void icinitfromfile(struct vm *vm, FILE *file) {
  assert(file != NULL);
  memset(vm, 0, sizeof(*vm));
  int n = 0;
  assert(fscanf(file, "%lld", &vm->mem[n++]) == 1);
  while (fscanf(file, ",%lld", &vm->mem[n]) == 1) {
    n++;
    assert(n < limit);
  }
  fscanf(file, " ");
  assert(feof(file));
  memcpy(vm->savedmem, vm->mem, sizeof(vm->savedmem));
}

void icreset(struct vm *vm) {
  vm->ip = 0;
  vm->basep = 0;
  vm->instart = vm->inend = 0;
  vm->outstart = vm->outend = 0;
  memcpy(vm->mem, vm->savedmem, sizeof(vm->savedmem));
}

bool ichalted(struct vm *vm) {
  icrun(vm);
  if (vm->instart != vm->inend) return false;
  if (vm->outstart != vm->outend) return false;
  return vm->mem[vm->ip] == 99;
}

void iccheckhalted(struct vm *vm) {
  icrun(vm);
  assert(vm->instart == vm->inend);
  assert(vm->outstart == vm->outend);
  assert(vm->mem[vm->ip] == 99);
}

static inline void iccheckrange(int x) {
  assert(0 <= x && x < limit);
}

// cvm = current virtual machine to save passing args.
struct vm *cvm;
static inline long long *icget(int ptr, enum addressmode amode) {
  iccheckrange(ptr);
  if (amode == addressmodeimmediate) return &cvm->mem[ptr];
  if (amode == addressmodeposition) {
    iccheckrange(cvm->mem[ptr]);
    return &cvm->mem[cvm->mem[ptr]];
  }
  assert(amode == addressmoderelative);
  long long p;
  assert(!__builtin_add_overflow(cvm->basep, cvm->mem[ptr], &p));
  iccheckrange(p);
  return &cvm->mem[p];
}
void icrun(struct vm *vm) {
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
      a = *icget(ip + 1, mode1);
      b = *icget(ip + 2, mode2);
      assert(!__builtin_add_overflow(a, b, &r));
      *icget(ip + 3, mode3) = r;
      ip += 4;
      break;
    case 2:
      // mul
      a = *icget(ip + 1, mode1);
      b = *icget(ip + 2, mode2);
      assert(!__builtin_mul_overflow(a, b, &r));
      *icget(ip + 3, mode3) = r;
      ip += 4;
      break;
    case 3:
      // input
      if (vm->instart == vm->inend) {
        vm->ip = ip;
        return;
      }
      *icget(ip + 1, mode1) = vm->input[vm->instart++];
      ip += 2;
      break;
    case 4:
      // output
      assert(vm->outend < limit);
      vm->output[vm->outend++] = *icget(ip + 1, mode1);
      ip += 2;
      break;
    case 5:
      // jump-if-true
      if (*icget(ip + 1, mode1) != 0) {
        ip = *icget(ip + 2, mode2);
      } else {
        ip += 3;
      }
      break;
    case 6:
      // jump-if-true
      if (*icget(ip + 1, mode1) == 0) {
        ip = *icget(ip + 2, mode2);
      } else {
        ip += 3;
      }
      break;
    case 7:
      // less-than
      *icget(ip + 3, mode3) = *icget(ip + 1, mode1) < *icget(ip + 2, mode2);
      ip += 4;
      break;
    case 8:
      // equals
      *icget(ip + 3, mode3) = *icget(ip + 1, mode1) == *icget(ip + 2, mode2);
      ip += 4;
      break;
    case 9:
      // adjust the relative base
      vm->basep += *icget(ip + 1, mode1);
      iccheckrange(vm->basep);
      ip += 2;
      break;
    default:
      assert(false);
    }
  }
  vm->ip = ip;
  assert(0 <= ip && ip < limit && mem[ip] == 99);
}

bool ichasoutput(struct vm *vm) {
  icrun(vm);
  return vm->outstart != vm->outend;
}

long long icgetnum(struct vm *vm) {
  icrun(vm);
  assert(vm->outstart < vm->outend);
  long long r = vm->output[vm->outstart++];
  if (vm->outstart == vm->outend) {
    vm->outstart = 0;
    vm->outend = 0;
  }
  return r;
}

static char outbuf[limit + 1];

char *icgetline(struct vm *vm) {
  icrun(vm);
  assert(vm->outstart < vm->outend);
  for (int i = 0; vm->outstart != vm->outend; i++) {
    assert(0 < vm->output[vm->outstart] && vm->output[vm->outstart] <= 127);
    if (vm->output[vm->outstart] == '\n') {
      outbuf[i] = 0;
      vm->outstart++;
      if (vm->outstart == vm->outend) {
        vm->outstart = 0;
        vm->outend = 0;
      }
      return outbuf;
    }
    outbuf[i] = vm->output[vm->outstart++];
  }
  assert(false);
}

char *icgetstring(struct vm *vm) {
  icrun(vm);
  assert(vm->outend < limit);
  int i;
  assert(0 < vm->output[vm->outstart] && vm->output[vm->outstart] <= 127);
  for (i = 0; vm->outstart != vm->outend; i++) {
    if (0 >= vm->output[vm->outstart] || vm->output[vm->outstart] > 127) break;
    outbuf[i] = vm->output[vm->outstart++];
  }
  outbuf[i] = 0;
  if (vm->outstart == vm->outend) {
    vm->outstart = 0;
    vm->outend = 0;
  }
  return outbuf;
}

void icputnum(struct vm *vm, long long num) {
  if (vm->instart == vm->inend) {
    vm->instart = 0;
    vm->inend = 0;
  }
  assert(vm->inend < limit);
  vm->input[vm->inend++] = num;
}

void icputstring(struct vm *vm, const char *str) {
  if (vm->instart == vm->inend) {
    vm->instart = 0;
    vm->inend = 0;
  }
  int len = strlen(str);
  assert(vm->inend + len < limit);
  for (int i = 0; i < len; i++) vm->input[vm->inend++] = str[i];
}
