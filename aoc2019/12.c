#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { moons = 4 };

struct vec3 {
  int x, y, z;
};

int cmp(int a, int b) {
  if (a < b) return -1;
  if (a > b) return 1;
  return 0;
}

long long gcd(long long a, long long b) {
  if (b == 0) return a;
  return gcd(b, a % b);
}

long long lcm(long long a, long long b) {
  long long r;
  assert(!__builtin_mul_overflow(a / gcd(a, b), b, &r));
  return r;
}

int cyclelen(int initp[moons]) {
  int v[moons] = {};
  int p[moons];
  memcpy(p, initp, sizeof(p));
  int s = 0;
  while (true) {
    s++;
    // simulate gravity.
    for (int i = 0; i < moons; i++) {
      for (int j = 0; j < moons; j++) {
        v[i] -= cmp(p[i], p[j]);
      }
    }
    // simulate movement.
    for (int i = 0; i < moons; i++) {
      p[i] += v[i];
      assert(abs(p[i]) < 10000);
    }
    if (memcmp(p, initp, sizeof(p)) == 0) {
      bool ok = true;
      for (int i = 0; ok && i < moons; i++) ok = v[i] == 0;
      if (ok) return s;
    }
  }
}

int main(void) {
  assert(freopen("12.in", "r", stdin) != NULL);
  struct vec3 initpos[moons], pos[moons], vel[moons] = {};
  for (int i = 0; i < moons; i++) {
    int x, y, z;
    assert(scanf("<x=%d, y=%d, z=%d> ", &x, &y, &z) == 3);
    initpos[i].x = x;
    initpos[i].y = y;
    initpos[i].z = z;
  }
  assert(feof(stdin));
  memcpy(pos, initpos, sizeof(pos));
  for (int s = 0; s < 1000; s++) {
    // simulate gravity.
    for (int i = 0; i < moons; i++) {
      for (int j = 0; j < moons; j++) {
        vel[i].x -= cmp(pos[i].x, pos[j].x);
        vel[i].y -= cmp(pos[i].y, pos[j].y);
        vel[i].z -= cmp(pos[i].z, pos[j].z);
      }
    }
    // simulate movement.
    for (int i = 0; i < moons; i++) {
      pos[i].x += vel[i].x;
      pos[i].y += vel[i].y;
      pos[i].z += vel[i].z;
      assert(abs(pos[i].x) < 10000);
      assert(abs(pos[i].y) < 10000);
      assert(abs(pos[i].z) < 10000);
    }
  }
  // calculate energy.
  int total = 0;
  for (int i = 0; i < moons; i++) {
    int pot = abs(pos[i].x) + abs(pos[i].y) + abs(pos[i].z);
    int kin = abs(vel[i].x) + abs(vel[i].y) + abs(vel[i].z);
    assert(pot < 10000 && kin < 10000);
    total += pot * kin;
  }
  printf("part1: %d\n", total);

  int p[moons];
  int a, b, c;
  for (int i = 0; i < moons; i++) p[i] = initpos[i].x;
  a = cyclelen(p);
  for (int i = 0; i < moons; i++) p[i] = initpos[i].y;
  b = cyclelen(p);
  for (int i = 0; i < moons; i++) p[i] = initpos[i].z;
  c = cyclelen(p);
  long long cyc = lcm(a, lcm(b, c));
  printf("part2: %lld\n", cyc);
  return 0;
}
