#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum { limit = 1000 };
enum { valuelimit = 100000000 };
enum { arglimit = 3 };

int diagnose(int *cmem, int n, int *input) {
  int mem[limit];
  memcpy(mem, cmem, n * sizeof(mem[0]));
  int ip = 0;
  int lastoutput = 0;
  while (true) {
    int arg[arglimit];
    int op = mem[ip] % 100;
    int mode = mem[ip] / 100;
    ip++;
    assert((1 <= op && op <= 8) || op == 99);
    int args = 0;
    if (op == 3 || op == 4) args = 1;
    if (op == 5 || op == 6) args = 2;
    if (op == 1 || op == 2 || op == 7 || op == 8) args = 3;
    assert(args <= arglimit);
    assert(ip + args <= n);
    for (int a = 0; a < args; a++) {
      arg[a] = mem[ip++];
      if (a == args - 1 && op != 4 && op != 5 && op != 6) {
        // skip preloading values for output parameters.
        break;
      }
      if (mode % 10 == 0) {
        assert(0 <= arg[a] && arg[a] < n);
        arg[a] = mem[arg[a]];
      }
      mode /= 10;
    }
    assert(mode == 0);
    if (op == 99) break;
    if (op != 4 && op != 5 && op != 6) {
      // check that an output arg is within bounds.
      assert(0 <= arg[args - 1] && arg[args - 1] < n);
    }
    if (op == 1) {
      // add
      int r = arg[0] + arg[1];
      assert(-valuelimit <= r && r <= valuelimit);
      mem[arg[2]] = r;
    } else if (op == 2) {
      // mul
      int r;
      assert(!__builtin_mul_overflow(arg[0], arg[1], &r));
      assert(-valuelimit <= r && r <= valuelimit);
      mem[arg[2]] = r;
    } else if (op == 3) {
      // input
      assert(*input != valuelimit + 1);
      mem[arg[0]] = *input++;
    } else if (op == 4) {
      // output
      assert(lastoutput == 0);
      lastoutput = arg[0];
    } else if (op == 5) {
      // jump-if-true
      if (arg[0] != 0) ip = arg[1];
    } else if (op == 6) {
      // jump-if-false
      if (arg[0] == 0) ip = arg[1];
    } else if (op == 7) {
      // less-than
      mem[arg[2]] = arg[0] < arg[1];
    } else if (op == 8) {
      // equals
      mem[arg[2]] = arg[0] == arg[1];
    } else {
      assert(false);
    }
    assert(0 <= ip && ip < n);
  }
  return lastoutput;
}

int main(void) {
  assert(freopen("05.in", "r", stdin) != NULL);
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
  int input[] = { 1, valuelimit + 1 };
  printf("part1: %d\n", diagnose(mem, n, input));
  input[0] = 5;
  printf("part2: %d\n", diagnose(mem, n, input));
  return 0;
}
