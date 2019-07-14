#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(cond) _util_check((cond), #cond, __FILE__, __LINE__);
static void
_util_check(bool ok, const char *cond, const char *file, int line)
{
	if (ok)
		return;
	const char *msg = "CHECK(%s) failed at %s:%d.\n";
	printf(msg, cond, file, line);
	abort();
}

char line[128];
void next_line(FILE *f)
{
	CHECK(fgets(line, 110, f) != NULL);
	CHECK(strlen(line) < 100);
}

const char usage[] =
	"Usage: stripe_hitmap [head_cutoff] [body_cutoff] [input.pgm]";

int main(int argc, char **argv)
{
	if (argc != 4) {
		puts(usage);
		exit(1);
	}
	int head_cutoff = atoi(argv[1]);
	int body_cutoff = atoi(argv[2]);
	FILE *in = fopen(argv[3], "r");
	CHECK(in != NULL);
	CHECK(unlink(argv[3]) == 0);
	FILE *out = fopen(argv[3], "w");
	CHECK(out != NULL);
	int R, C;
	next_line(in);
	CHECK(strcmp(line, "P5\n") == 0);
	fputs(line, out);
	next_line(in);
	fputs(line, out);
	sscanf(line, "%d %d", &C, &R);
	next_line(in);
	CHECK(strcmp(line, "255\n") == 0);
	fputs(line, out);
	for (int r = 0; r < R; r++) {
		for (int c = 0; c < C; c++) {
			int ich = fgetc(in);
			int och = 0;
			if (ich != 0) {
				if (r < head_cutoff) {
					och = 192;
				} else if (r < body_cutoff) {
					och = 128;
				} else {
					och = 64;
				}
			}
			fputc(och, out);
		}
	}
	CHECK(fclose(in) == 0);
	CHECK(fclose(out) == 0);
	return 0;
}
