#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum { limit = 300000 };

int toid(char *str) {
  int id = 0;
  while (*str != 0) {
    id *= 62;
    if ('0' <= *str && *str <= '9') {
      id += *str - '0';
    } else if ('A' <= *str && *str <= 'Z') {
      id += *str - 'A' + 10;
    } else if ('a' <= *str && *str <= 'z') {
      id += *str - 'a' + 10 + 26;
    } else {
      assert(false);
    }
    assert(id < limit - 1);
    str++;
  }
  return id + 1;
}

int orbit[limit];
long long orbits[limit];

int main(void) {
  assert(freopen("06.in", "r", stdin) != NULL);
  char as[6], bs[6];
  while (scanf("%3[A-Za-z0-9])%3[A-Za-z0-9] ", as, bs) == 2) {
    int b = toid(bs);
    assert(orbit[b] == 0);
    orbit[b] = toid(as);
  }
  assert(feof(stdin));
  long long cnt = 0;
  memset(orbits, -1, sizeof(orbits));
  orbits[0] = 0;
  for (int i = 1; i < limit; i++) {
    if (orbit[i] == 0) continue;
    int obj = i;
    long long ancestors = 0;
    while (orbit[obj] != 0 && orbits[obj] == -1) {
      ancestors++;
      obj = orbit[obj];
    }
    if (obj != 0 && orbits[obj] == -1) orbits[obj] = 0;
    ancestors += orbits[obj];
    obj = i;
    while (orbit[obj] != 0 && orbits[obj] == -1) {
      cnt += ancestors;
      orbits[obj] = ancestors--;
      obj = orbit[obj];
    }
    assert(cnt < 1LL << 40);
  }
  printf("part1: %lld\n", cnt);
  int you = toid("YOU");
  int san = toid("SAN");
  int transfers = 0;
  while (you != san) {
    transfers++;
    if (orbits[you] > orbits[san]) {
      you = orbit[you];
    } else {
      san = orbit[san];
    }
  }
  printf("part2: %d\n", transfers - 2);
  return 0;
}
