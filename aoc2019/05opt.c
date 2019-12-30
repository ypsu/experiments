// note this solution omits the range and overflow checks.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { limit = 10000 };
enum { valuelimit = 100000000 };

int8_t opmap[limit];
int mem[limit + 3];
static inline int get(int ip, int offset, int immmask) {
  if (((immmask >> offset) & 1) == 1) return mem[ip + offset];
  return mem[mem[ip + offset]];
}
int diagnose(int *cmem, int n, int *input) {
  memcpy(mem, cmem, n * sizeof(mem[0]));
  int ip = 0;
  int lastoutput = 0;
  while (0 <= ip && ip < n && mem[ip] != 99) {
    int m = opmap[mem[ip]];
    int immmask = m & 7;
    int op = m >> 3;
    switch (op) {
    case 0:
      assert(false);
    case 1:
      // add
      mem[mem[ip + 3]] = get(ip, 1, immmask) + get(ip, 2, immmask);
      ip += 4;
      break;
    case 2:
      // mul
      mem[mem[ip + 3]] = get(ip, 1, immmask) * get(ip, 2, immmask);
      ip += 4;
      break;
    case 3:
      // input
      mem[mem[ip + 1]] = *input++;
      ip += 2;
      break;
    case 4:
      // output
      assert(lastoutput == 0);
      lastoutput = get(ip, 1, immmask);
      ip += 2;
      break;
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
      mem[mem[ip + 3]] = get(ip, 1, immmask) < get(ip, 2, immmask);
      ip += 4;
      break;
    case 8:
      // equals
      mem[mem[ip + 3]] = get(ip, 1, immmask) == get(ip, 2, immmask);
      ip += 4;
      break;
    default:
      break;
    }
  }
  assert(0 <= ip && ip < n && mem[ip] == 99);
  return lastoutput;
}

int main(void) {
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
  assert(freopen("05.joe", "r", stdin) != NULL);
  int n = 0;
  int mem[limit];
  assert(scanf("%d", &mem[n++]) == 1);
  while (scanf(",%d", &mem[n]) == 1) {
    assert(-valuelimit <= mem[n] && mem[n] <= valuelimit);
    n++;
    assert(n < limit);
  }
  scanf(" ");
  assert(feof(stdin));
  int input[] = { 100000000, valuelimit + 1 };
  printf("part1: %d\n", diagnose(mem, n, input));
  return 0;
}
