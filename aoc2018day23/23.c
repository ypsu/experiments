#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define check assert
enum { limit = 1020 };

struct nanobot {
  int x, y, z;
  int r;
};

struct searchbox {
  int coveredbots;
  int origindist;
  int sidelength;
  // the smallest coordinates corner
  int x, y, z;
};

struct nanobot nanobots[limit];
int nanobotscnt;
// heap[1] is the first element, heap[0] is unused. it's a min-heap. heap[i]
// indexes into the searchboxes array.
int heap[limit * limit];
int heapcnt;
struct searchbox searchboxes[limit * limit];
int searchboxescnt;

static inline void swap(int *a, int *b) {
  int t = *a;
  *a = *b;
  *b = t;
}

static inline bool cmpheapentry(int pa, int pb) {
  const struct searchbox *a = &searchboxes[heap[pa]];
  const struct searchbox *b = &searchboxes[heap[pb]];
  if (a->coveredbots != b->coveredbots) return a->coveredbots > b->coveredbots;
  if (a->origindist != b->origindist) return a->origindist < b->origindist;
  return a->sidelength < b->sidelength;
}

void bubbleup(void) {
  int pos = heapcnt;
  while (pos > 1 && cmpheapentry(pos, pos / 2)) {
    swap(&heap[pos / 2], &heap[pos]);
    pos /= 2;
  }
}
void bubbledown(void) {
  int pos = 1;
  while (pos * 2 <= heapcnt) {
    int swappos = pos * 2;
    if (swappos + 1 <= heapcnt && cmpheapentry(swappos + 1, swappos)) {
      swappos++;
    }
    if (cmpheapentry(swappos, pos)) {
      swap(&heap[swappos], &heap[pos]);
      pos = swappos;
    } else {
      break;
    }
  }
}

int rangedist(int x, int lo, int hi) {
  if (x < lo) return lo - x;
  if (x > hi) return x - hi;
  return 0;
}

void countboxes(struct searchbox *box) {
  int x1 = box->x, x2 = box->x + box->sidelength - 1;
  int y1 = box->y, y2 = box->y + box->sidelength - 1;
  int z1 = box->z, z2 = box->z + box->sidelength - 1;
  int cnt = 0;
  for (int i = 0; i < nanobotscnt; i++) {
    struct nanobot *bot = &nanobots[i];
    int d = 0;
    d += rangedist(bot->x, x1, x2);
    d += rangedist(bot->y, y1, y2);
    d += rangedist(bot->z, z1, z2);
    if (d <= bot->r) cnt++;
  }
  box->coveredbots = cnt;
}

int main(void) {
  //freopen("23.in1", "r", stdin);
  int x, y, z, r;
  int maxradid = 0;
  while (scanf(" pos=<%d,%d,%d>, r=%d", &x, &y, &z, &r) == 4) {
    check(nanobotscnt < limit);
    if (r > nanobots[maxradid].r) maxradid = nanobotscnt;
    nanobots[nanobotscnt++] = (struct nanobot){ x, y, z, r };
  }
  int inrange = 0;
  for (int i = 0; i < nanobotscnt; i++) {
    int dx = abs(nanobots[i].x - nanobots[maxradid].x);
    int dy = abs(nanobots[i].y - nanobots[maxradid].y);
    int dz = abs(nanobots[i].z - nanobots[maxradid].z);
    if (dx + dy + dz <= nanobots[maxradid].r) inrange++;
  }
  printf("A: %d\n", inrange);

  // solve b via a*. we need find a "searchbox" that has the sidelength of 1 and
  // has the most bots covered. start with large searchboxes. the "cost" of a
  // searchbox is the number of bots covered. front of the queue is always the
  // searchbox with the highest number of bots covered. break times by giving
  // more priority to searchboxes closer to origin. to process a searchbox,
  // subdivide it into 8 smaller boxes, calculate the number of bots each covers
  // and push them into the queue (observe that any smaller searchbox cannot
  // cover more bots than the parent searchbox). due to the properties of a* by
  // the time this algorithm pops a searchbox of sidelength of 1, it will be the
  // optimal point.
  int inf = 1234567890;
  int minx = inf, maxx = -inf, miny = inf, maxy = -inf, minz = inf, maxz = -inf;
  for (int i = 0; i < nanobotscnt; i++) {
    struct nanobot *bot = &nanobots[i];
    if (bot->x - bot->r < minx) minx = bot->x - bot->r;
    if (bot->x + bot->r > maxx) maxx = bot->x + bot->r;
    if (bot->y - bot->r < miny) miny = bot->y - bot->r;
    if (bot->y + bot->r > maxy) maxy = bot->y + bot->r;
    if (bot->z - bot->r < minz) minz = bot->z - bot->r;
    if (bot->z + bot->r > maxz) maxz = bot->z + bot->r;
  }
  struct searchbox *box = &searchboxes[searchboxescnt++];
  box->x = minx;
  box->y = miny;
  box->z = minz;
  box->origindist = abs(box->x) + abs(box->y) + abs(box->z);
  int len = 1;
  while (box->x + len < maxx || box->y + len < maxy || box->z + len < maxz) {
    len *= 2;
  }
  box->sidelength = len;
  countboxes(box);
  heap[1] = 0;
  heapcnt = 1;
  int processed = 0;
  while (heapcnt > 0) {
    processed++;
    struct searchbox *qbox = &searchboxes[heap[1]];
    heap[1] = heap[heapcnt--];
    bubbledown();
    if (qbox->sidelength == 1) {
      const char fmt[] = "B: %d (cover=%d, proc=%d)\n";
      printf(fmt, qbox->origindist, qbox->coveredbots, processed);
      break;
    }
    int newside = qbox->sidelength / 2;
    check(heapcnt + 8 < (int)(sizeof(heap) / sizeof(heap[0])));
    const int searchboxescap = sizeof(searchboxes) / sizeof(searchboxes[0]);
    check(searchboxescnt + 8 < searchboxescap);
    int x = qbox->x;
    for (int a = 0; a <= 1; a++, x += newside) {
      int y = qbox->y;
      for (int b = 0; b <= 1; b++, y += newside) {
        int z = qbox->z;
        for (int c = 0; c <= 1; c++, z += newside) {
          heap[heapcnt + 1] = searchboxescnt;
          box = &searchboxes[searchboxescnt];
          box->x = x;
          box->y = y;
          box->z = z;
          box->sidelength = newside;
          box->origindist = abs(box->x) + abs(box->y) + abs(box->z);
          countboxes(box);
          if (box->coveredbots) {
            heapcnt++;
            searchboxescnt++;
            bubbleup();
          }
        }
      }
    }
  }
  return 0;
}
