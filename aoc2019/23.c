#include "intcode.c"

enum { cards = 50 };

struct vm vm[cards];

int main(void) {
  assert(freopen("23.in", "r", stdin) != NULL);
  icinitfromstdin(&vm[0]);
  for (int i = 1; i < cards; i++) vm[i] = vm[0];
  for (int i = 0; i < cards; i++) icputnum(&vm[i], i);
  bool done = false;
  long long lastnatx = -1, lastnaty = -1;
  long long lastdeliverednaty = -1;
  bool lastfoundoutput = true;
  while (!done) {
    bool foundoutput = false;
    for (int i = 0; i < cards; i++) {
      while (ichasoutput(&vm[i])) {
        foundoutput = true;
        long long dst = icgetnum(&vm[i]);
        long long x = icgetnum(&vm[i]);
        long long y = icgetnum(&vm[i]);
        if (dst == 255) {
          assert(y != -1);
          if (lastnaty == -1) printf("part1: %lld\n", y);
          lastnatx = x;
          lastnaty = y;
        } else {
          assert(0 <= dst && dst < cards);
          icputnum(&vm[dst], x);
          icputnum(&vm[dst], y);
        }
      }
    }
    if (!foundoutput) {
      if (!lastfoundoutput) {
        assert(lastnatx != -1 && lastnaty != -1);
        if (lastnaty == lastdeliverednaty) {
          printf("part2: %lld\n", lastnaty);
          done = true;
        }
        lastdeliverednaty = lastnaty;
        icputnum(&vm[0], lastnatx);
        icputnum(&vm[0], lastnaty);
        lastfoundoutput = true;
      } else {
        for (int i = 0; i < cards; i++) {
          icputnum(&vm[i], -1);
        }
        lastfoundoutput = false;
      }
    } else {
      lastfoundoutput = true;
    }
  }
  return 0;
}
