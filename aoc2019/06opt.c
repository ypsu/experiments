#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

enum { filelimit = 10000000 };
enum { limit = 300000 };

char *p;
int readid(void) {
  int id = 0;
  while ('0' <= *p && *p <= 'z') {
    id *= 62;
    if ('0' <= *p && *p <= '9') {
      id += *p - '0';
    } else if ('A' <= *p && *p <= 'Z') {
      id += *p - 'A' + 10;
    } else if ('a' <= *p && *p <= 'z') {
      id += *p - 'a' + 10 + 26;
    } else {
      assert(false);
    }
    assert(id < limit - 1);
    p++;
  }
  return id + 1;
}

char buf[filelimit];
int orbit[limit];
long long orbits[limit];

int main(void) {
  assert(freopen("06.in", "r", stdin) != NULL);
  p = buf;
  int sz = fread(buf, 1, filelimit, stdin);
  assert(sz < filelimit);
  assert(feof(stdin));
  buf[sz] = 0;
  while (*p != 0) {
    int a = readid();
    assert(*p++ == ')');
    int b = readid();
    assert(orbit[b] == 0);
    orbit[b] = a;
    if (*p == 0) break;
    assert(*p++ == '\n');
  }
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
  int you = 132215;
  int san = 108276;
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
