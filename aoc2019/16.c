#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char rdig[1000 * 10000];

// a little bit optimized version of the standard brute force algorithm.
void fft1(char *digits, int len) {
  for (int i = 0; i < 100; i++) {
    for (int j = 1; j <= len; j++) {
      int sum = 0;
      int x = 0;
      bool add = true;
      for (int k = j; k <= len; k++, x++) {
        if (x >= j) {
          x = -1;
          add ^= 1;
          k += j - 1;
          continue;
        }
        if (add) {
          sum += digits[k - 1];
        } else {
          sum -= digits[k - 1];
        }
      }
      digits[j - 1] = abs(sum) % 10;
    }
  }
}

// only calculate the values in the second half.
void fft2(char *digits, int len) {
  for (int i = 0; i < 100; i++) {
    int sum = 0;
    for (int j = len; j >= len / 2; j--) {
      sum += digits[j - 1];
      digits[j - 1] = sum % 10;
    }
  }
}

int main(void) {
  assert(freopen("16.in", "r", stdin) != NULL);
  char digits[1000];
  char input[1000];
  assert(scanf("%900s", input) == 1);
  int len = strlen(input);
  assert(len < 800);
  for (int i = 0; i < len; i++) digits[i] = input[i] - '0';
  fft1(digits, len);
  for (int i = 0; i < len; i++) digits[i] += '0';
  printf("part1: %.8s\n", digits);

  int offset = -1;
  assert(sscanf(input, "%7d", &offset) == 1);
  printf("offset %d\n", offset);
  int mul = 10000;
  for (int i = 0; i < mul; i++) {
    for (int j = 0; j < len; j++) rdig[i * len + j] = input[j] - '0';
  }
  fft2(rdig, mul * len);
  for (int i = 0; i < mul * len; i++) rdig[i] += '0';
  printf("part2: %.8s\n", rdig + offset);
  return 0;
}
