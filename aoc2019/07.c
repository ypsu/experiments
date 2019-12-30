#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { limit = 10000 };
enum { valuelimit = 200000000 };

int8_t opmap[limit];

int n;
struct vm {
  int ip;
  int mem[limit + 3];
};

static inline void checkrange(int x) {
  assert(0 <= x && x < n);
}

// cvm = current virtual machine to save passing args.
struct vm *cvm;
static inline int get(int ip, int offset, int immmask) {
  if (((immmask >> offset) & 1) == 1) return cvm->mem[ip + offset];
  checkrange(cvm->mem[ip + offset]);
  return cvm->mem[cvm->mem[ip + offset]];
}
void init(struct vm *vm, int *cmem) {
  vm->ip = 0;
  memcpy(vm->mem, cmem, n * sizeof(cmem[0]));
}
bool getoutput(struct vm *vm, int *output, int *input) {
  int ip = vm->ip;
  int *mem = vm->mem;
  cvm = vm;
  while (0 <= ip && ip < n && mem[ip] != 99) {
    int m = opmap[mem[ip]];
    int immmask = m & 7;
    int op = m >> 3;
    long long r;
    switch (op) {
    case 0:
      assert(false);
    case 1:
      // add
      checkrange(mem[ip + 3]);
      r = get(ip, 1, immmask) + get(ip, 2, immmask);
      assert(-valuelimit < r && r < valuelimit);
      mem[mem[ip + 3]] = r;
      ip += 4;
      break;
    case 2:
      // mul
      checkrange(mem[ip + 3]);
      r = get(ip, 1, immmask) * get(ip, 2, immmask);
      assert(-valuelimit < r && r < valuelimit);
      mem[mem[ip + 3]] = r;
      ip += 4;
      break;
    case 3:
      // input
      checkrange(mem[ip + 1]);
      mem[mem[ip + 1]] = *input++;
      assert(-valuelimit < mem[mem[ip + 3]] && mem[mem[ip + 3]] < valuelimit);
      ip += 2;
      break;
    case 4:
      // output
      *output = get(ip, 1, immmask);
      ip += 2;
      vm->ip = ip;
      return true;
    case 5:
      // jump-if-true
      if (get(ip, 1, immmask) != 0) {
        ip = get(ip, 2, immmask);
      } else {
        ip += 3;
      }
      break;
    case 6:
      // jump-if-true
      if (get(ip, 1, immmask) == 0) {
        ip = get(ip, 2, immmask);
      } else {
        ip += 3;
      }
      break;
    case 7:
      // less-than
      checkrange(mem[ip + 3]);
      mem[mem[ip + 3]] = get(ip, 1, immmask) < get(ip, 2, immmask);
      ip += 4;
      break;
    case 8:
      // equals
      checkrange(mem[ip + 3]);
      mem[mem[ip + 3]] = get(ip, 1, immmask) == get(ip, 2, immmask);
      ip += 4;
      break;
    default:
      break;
    }
  }
  vm->ip = ip;
  assert(0 <= ip && ip < n && mem[ip] == 99);
  return false;
}

int main(void) {
  assert(freopen("07.in", "r", stdin) != NULL);
  for (int i = 0; i < limit; i++) {
    int m = 0;
    int x = i;
    if (x > 1000) {
      m |= 4;
      x -= 1000;
    }
    if (x > 100) {
      m |= 2;
      x -= 100;
    }
    m |= x << 3;
    opmap[i] = m;
  }
  int mem[limit];
  assert(scanf("%d", &mem[n++]) == 1);
  while (scanf(",%d", &mem[n]) == 1) {
    assert(-valuelimit <= mem[n] && mem[n] <= valuelimit);
    n++;
    assert(n < limit);
  }
  scanf(" ");
  assert(feof(stdin));
  struct vm vm[5];
  int order[5] = {0, 1, 2, 3, 4};
  int maxval1 = 0;
  int maxval2 = 0;
  while (true) {
    int input[] = { 0, 0, valuelimit + 1 };
    for (int i = 0; i < 5; i++) {
      input[0] = order[i];
      int output;
      init(&vm[i], mem);
      assert(getoutput(&vm[i], &output, input));
      input[1] = output;
    }
    if (input[1] > maxval1) maxval1 = input[1];
    int laste = -1;
    input[1] = 0;
    int i;
    for (i = 0; ; i++) {
      int output;
      int *inputptr = input;
      if (i < 5) {
        *inputptr = 9 - order[i];
        init(&vm[i], mem);
      } else {
        inputptr++;
      }
      if (!getoutput(&vm[i % 5], &output, inputptr)) break;
      input[1] = output;
      if (i % 5 == 4) laste = output;
    }
    assert(i % 5 == 0);
    if (laste > maxval2) maxval2 = laste;

    // generate the next permutation.
    int k, l;
    for (k = 4; k > 0 && order[k - 1] > order[k]; k--);
    if (k == 0) break;
    k--;
    for (l = k + 1; l < 5 && order[l] > order[k]; l++);
    l--;
    int t = order[k];
    order[k] = order[l];
    order[l] = t;
    l = 4;
    k++;
    while (k < l) {
      t = order[k];
      order[k] = order[l];
      order[l] = t;
      k++;
      l--;
    }
  }
  printf("part1: %d\n", maxval1);
  printf("part2: %d\n", maxval2);
  return 0;
}
