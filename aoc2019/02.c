#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { limit = 400 };

// run and return value at position 0. doesn't modify cmem.
int runprog(int *cmem, int n, int noun, int verb) {
  int mem[limit];
  memcpy(mem, cmem, n * sizeof(mem[0]));
  mem[1] = noun;
  mem[2] = verb;
  int ip;
  for (ip = 0; ip < n; ip += 4) {
    int op = mem[ip];
    if (op == 99) break;
    assert(op == 1 || op == 2);
    int a = mem[ip + 1], b = mem[ip + 2], c = mem[ip + 3];
    long long r;
    assert(a < n && b < n && c < n);
    if (op == 1) {
      r = mem[a] + mem[b];
    } else {
      r = mem[a] * (long long)mem[b];
    }
    assert(r >= 0 && r < 1000000000);
    mem[c] = r;
  }
  assert(ip < n);
  return mem[0];
}

int main(void) {
  assert(freopen("02.in", "r", stdin) != NULL);
  int n = 0;
  int mem[limit];
  assert(scanf("%d", &mem[n++]) == 1);
  while (scanf(",%d", &mem[n]) == 1) {
    assert(mem[n] >= 0 && mem[n] < 1000);
    n++;
    assert(n < limit);
  }
  scanf(" ");
  assert(feof(stdin));
  printf("part 1: %d\n", runprog(mem, n, 12, 2));
  for (int a = 0; a <= 99; a++) {
    for (int b = 0; b <= 99; b++) {
      if (runprog(mem, n, a, b) == 19690720) {
        printf("part 2: %d\n", 100 * a + b);
      }
    }
  }
  return 0;
}
