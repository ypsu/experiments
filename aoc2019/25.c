#include "intcode.c"

int main(void) {
  FILE *f = fopen("25.in", "r");
  struct vm vm;
  icinitfromfile(&vm, f);
  assert(fclose(f) == 0);
  icputstring(&vm, "west\n");
  icputstring(&vm, "south\n");
  icputstring(&vm, "south\n");
  icputstring(&vm, "south\n");
  icputstring(&vm, "take asterisk\n");
  icputstring(&vm, "north\n");
  icputstring(&vm, "north\n");
  icputstring(&vm, "north\n");
  icputstring(&vm, "west\n");
  icputstring(&vm, "west\n");
  icputstring(&vm, "west\n");
  icputstring(&vm, "take dark matter\n");
  icputstring(&vm, "east\n");
  icputstring(&vm, "south\n");
  icputstring(&vm, "take fixed point\n");
  icputstring(&vm, "west\n");
  icputstring(&vm, "take food ration\n");
  icputstring(&vm, "east\n");
  icputstring(&vm, "north\n");
  icputstring(&vm, "east\n");
  icputstring(&vm, "south\n");
  icputstring(&vm, "take astronaut ice cream\n");
  icputstring(&vm, "south\n");
  icputstring(&vm, "take polygon\n");
  icputstring(&vm, "east\n");
  icputstring(&vm, "take easter egg\n");
  icputstring(&vm, "east\n");
  icputstring(&vm, "take weather machine\n");
  icputstring(&vm, "north\n");
  char items[8][30] = {
    "polygon",
    "fixed point",
    "astronaut ice cream",
    "easter egg",
    "dark matter",
    "food ration",
    "asterisk",
    "weather machine",
  };
  puts(icgetstring(&vm));
  for (int i = 1; i < 256; i++) {
    for (int j = 0; j < 8; j++) {
      char buf[50];
      sprintf(buf, "drop %s\n", items[j]);
      icputstring(&vm, buf);
    }
    printf("checking:");
    for (int j = 0; j < 8; j++) {
      if ((i & (1 << j)) != 0) {
        char buf[50];
        sprintf(buf, "take %s\n", items[j]);
        icputstring(&vm, buf);
        printf(" %s", items[j]);
      }
    }
    puts("");
    icputstring(&vm, "north\n");
    char *s = icgetstring(&vm);
    if (strstr(s, "than the detected value") != NULL) continue;
    puts(s);
    break;
  }
  //char buf[99];
  //while (fgets(buf, 90, stdin) != NULL) {
  //  assert(strlen(buf) < 80);
  //  icputstring(&vm, buf);
  //  puts(icgetstring(&vm));
  //}
  return 0;
}
