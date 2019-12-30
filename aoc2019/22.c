#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { decksize = 10007 };
enum { gdecksize = 119315717514047LL };
enum { gtimes = 101741582076661LL };

struct mat22 {
  long long m[2][2];
};

struct mat22 mul(struct mat22 a, struct mat22 b) {
  struct mat22 r;
  r.m[0][0] = (a.m[0][0] * (__int128)b.m[0][0] + a.m[0][1] * (__int128)b.m[1][0]) % gdecksize;
  r.m[0][1] = (a.m[0][0] * (__int128)b.m[0][1] + a.m[0][1] * (__int128)b.m[1][1]) % gdecksize;
  r.m[1][0] = (a.m[1][0] * (__int128)b.m[0][0] + a.m[1][1] * (__int128)b.m[1][0]) % gdecksize;
  r.m[1][1] = (a.m[1][0] * (__int128)b.m[0][1] + a.m[1][1] * (__int128)b.m[1][1]) % gdecksize;
  return r;
}

long long trackpos(long long pos) {
  assert(freopen("22.in", "r", stdin) != NULL);
  char linebuf[60];
  while (fgets(linebuf, 50, stdin) != NULL) {
    assert(strlen(linebuf) < 40);
    long long cutsize = -1;
    int increment = -1;
    if (strcmp(linebuf, "deal into new stack\n") == 0) {
      pos = gdecksize - pos - 1;
    } else if (sscanf(linebuf, "cut %lld", &cutsize) == 1) {
      if (cutsize < 0) cutsize = gdecksize + cutsize;
      if (pos < cutsize) {
        pos += gdecksize - cutsize;
      } else {
        pos -= cutsize;
      }
    } else if (sscanf(linebuf, "deal with increment %d", &increment) == 1) {
      pos = (pos * increment) % gdecksize;
    }
  }
  return pos;
}

long long modinv(long long x) {
  long long r = 1;
  long long k = gdecksize - 2;
  while (k > 0) {
    if (k % 2 == 1) {
      r = (r * (__int128)x) % gdecksize;
      k--;
    } else {
      x = (x * (__int128)x) % gdecksize;
      k /= 2;
    }
  }
  return r;
}

int main(void) {
  assert(freopen("22.in", "r", stdin) != NULL);
  char linebuf[60];
  int deck[decksize];
  int tmp[decksize];
  for (int i = 0; i < decksize; i++) deck[i] = i;
  while (fgets(linebuf, 50, stdin) != NULL) {
    assert(strlen(linebuf) < 40);
    int cutsize = -1;
    int increment = -1;
    if (strcmp(linebuf, "deal into new stack\n") == 0) {
      int a = 0, b = decksize - 1;
      while (a < b) {
        int t = deck[a];
        deck[a] = deck[b];
        deck[b] = t;
        a++;
        b--;
      }
    } else if (sscanf(linebuf, "cut %d", &cutsize) == 1) {
      memcpy(tmp, deck, sizeof(tmp));
      const int s = sizeof(deck[0]);
      if (cutsize < 0) cutsize = decksize + cutsize;
      assert(cutsize < decksize);
      int o = decksize - cutsize;
      memcpy(deck + o, tmp, cutsize * s);
      memcpy(deck, tmp + cutsize, o * s);
    } else if (sscanf(linebuf, "deal with increment %d", &increment) == 1) {
      assert(increment > 0);
      memcpy(tmp, deck, sizeof(tmp));
      int offset = 0;
      for (int i = 0; i < decksize; i++) {
        deck[offset] = tmp[i];
        offset = (offset + increment) % decksize;
      }
    }
  }
  for (int i = 0; i < decksize; i++) {
    if (deck[i] == 2019) printf("part1: %d\n", i);
  }

  long long p0 = trackpos(0);
  long long p1 = trackpos(1);
  if (p1 < p0) p1 += gdecksize;
  // to get pn after k iterations:
  // [1, pn] = [1, p] * [1, a; 0, b] ^ k
  long long a = p0;
  long long b = p1 - p0;
  struct mat22 id = { { { 1, 0 }, { 0, 1 } } };
  struct mat22 base = { { { 1, a }, { 0, b } } };
  struct mat22 r = id;
  long long k = 101741582076661;
  while (k > 0) {
    if (k % 2 == 1) {
      r = mul(r, base);
      k -= 1;
    } else {
      base = mul(base, base);
      k /= 2;
    }
  }
  // [1, x] * r = [1, 2020]; x == ?
  // 2020 = 1 * r[0,1] + x * r[1,1]
  // x * r[1,1] = 2020 - r[0,1]
  // x = (2020 - r[0,1]) * r[1,1]^-1
  long long card = ((2020 - r.m[0][1] + gdecksize) * (__int128)modinv(r.m[1][1])) % gdecksize;
  printf("part2: %lld\n", card);
  return 0;
}
