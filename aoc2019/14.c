#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum { chemlimit = 100 };
enum { sourcelimit = 10 };

// the ids start from 1. 0 is a sentinel value.
int oreid;
int fuelid;
char code[chemlimit][8];
int getid(const char *c) {
  int id = 1;
  while (code[id][0] != 0) {
    if (strcmp(c, code[id]) == 0) return id;
    id++;
  }
  assert(id < chemlimit - 1);
  assert(strlen(c) < 8);
  strcpy(code[id], c);
  return id;
}

struct ingredients {
  int outputcnt;
  int chemid[sourcelimit];
  int cnt[sourcelimit];
};
struct ingredients source[chemlimit];

// order contains the topological order of the materials.
int ordercnt;
int order[chemlimit];

bool visited[chemlimit];
void visit(int chemid) {
  if (visited[chemid]) return;
  visited[chemid] = true;
  for (int i = 0; source[chemid].chemid[i] != 0; i++) {
    visit(source[chemid].chemid[i]);
  }
  order[ordercnt++] = chemid;
}

long long oreneeded(long long fuel) {
  long long need[chemlimit] = {};
  need[fuelid] = fuel;
  for (int i = ordercnt - 1; i >= 1; i--) {
    int id = order[i];
    long long c = (need[id] + source[id].outputcnt - 1) / source[id].outputcnt;
    for (int j = 0; source[id].chemid[j] != 0; j++) {
      int sid = source[id].chemid[j];
      need[sid] += c * (long long)source[id].cnt[j];
      assert(need[sid] < 100000000000000);
    }
  }
  return need[oreid];
}

int main(void) {
  assert(freopen("14.in", "r", stdin) != NULL);
  oreid = getid("ORE");
  fuelid = getid("FUEL");
  while (true) {
    int sources = 0;
    struct ingredients sourcebuf = {};
    int cnt;
    char chemical[16];
    while (true) {
      assert(sources < sourcelimit - 1);
      assert(scanf("%d %15[A-Z]", &cnt, chemical) == 2);
      sourcebuf.chemid[sources] = getid(chemical);
      sourcebuf.cnt[sources] = cnt;
      sources++;
      assert(1 <= cnt && cnt < 1000);
      if (getchar() != ',') break;
    }
    assert(getchar() == '=');
    assert(getchar() == '>');
    assert(scanf("%d %15[A-Z] ", &cnt, chemical) == 2);
    assert(1 <= cnt && cnt < 1000);
    sourcebuf.outputcnt = cnt;
    int id = getid(chemical);
    assert(source[id].outputcnt == 0);
    source[id] = sourcebuf;
    if (feof(stdin)) break;
  }
  visit(fuelid);
  assert(order[0] = oreid);
  printf("part1: %lld\n", oreneeded(1));

  long long ownedore = 1000000000000;
  long long lo = 1, hi = 1;
  while (oreneeded(hi) < ownedore) hi *= 2;
  while (lo <= hi) {
    long long mid = (lo + hi) / 2;
    long long ore = oreneeded(mid);
    if (ore < ownedore) {
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  printf("part2: %lld\n", hi);
  return 0;
}
