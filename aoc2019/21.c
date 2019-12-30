#include "intcode.c"

int main(void) {
  assert(freopen("21.in", "r", stdin) != 0);
  struct vm vm;
  icinitfromstdin(&vm);
  puts(icgetline(&vm));
  icputstring(&vm, "OR D J\n");
  icputstring(&vm, "NOT T T\n");
  icputstring(&vm, "AND A T\n");
  icputstring(&vm, "AND B T\n");
  icputstring(&vm, "AND C T\n");
  icputstring(&vm, "NOT T T\n");
  icputstring(&vm, "AND T J\n");
  icputstring(&vm, "WALK\n");
  puts(icgetstring(&vm));
  printf("part1: %lld\n", icgetnum(&vm));

  icreset(&vm);
  puts(icgetline(&vm));
  icputstring(&vm, "OR D J\n");
  icputstring(&vm, "NOT T T\n");
  icputstring(&vm, "AND A T\n");
  icputstring(&vm, "AND B T\n");
  icputstring(&vm, "AND C T\n");
  icputstring(&vm, "NOT T T\n");
  icputstring(&vm, "AND T J\n");

  icputstring(&vm, "NOT J T\n");
  icputstring(&vm, "OR E T\n");
  icputstring(&vm, "OR H T\n");
  icputstring(&vm, "AND T J\n");
  icputstring(&vm, "RUN\n");
  puts(icgetstring(&vm));
  printf("part2: %lld\n", icgetnum(&vm));
  return 0;
}
