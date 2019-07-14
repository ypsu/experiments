#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Generates a texture atlas from the command line arguments to the stdout.

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

enum { TEX_SIZE = 1024 };
static bool
_used[TEX_SIZE][TEX_SIZE];

__attribute__((packed))
struct png_header {
	uint64_t magic;
	uint32_t ihdr_length;
	uint32_t ihdr_magic;
	uint32_t width, height;
	uint8_t bit_depth;
};

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		// Extract filename.
		const char *fname = argv[i];
		const char *basename = strrchr(fname, '/');
		CHECK(basename != NULL);
		basename += 1;
		fprintf(stderr, "\tProcessing %s.\n", basename);

		// Read the image size from the file.
		struct png_header header;
		FILE *f = fopen(fname, "rb");
		CHECK(f != NULL);
		CHECK(fread(&header, sizeof header, 1, f) == 1);
		CHECK(fclose(f) == 0);
		CHECK(header.magic == 0x0a1a0a0d474e5089u);
		CHECK(header.ihdr_magic == 0x52444849u);
		CHECK(header.ihdr_length == htonl(13));
		// +1 is added so the textures are not too tight to avoid
		// texture bleeding.
		int width = ntohl(header.width) + 1;
		int height = ntohl(header.height) + 1;

		// Find a place for the image.
		bool found = false;
		int px = -1, py = -1;
		for (int y = 0; !found && y <= TEX_SIZE-height; y++) {
		for (int x = 0; x <= TEX_SIZE-width; x++) {
			if (_used[y][x]) {
				continue;
			}
			bool ok = true;
			int dx;
			for (dx = 0; ok && dx < width; dx++) {
				ok = !_used[y][x+dx];
			}
			if (!ok) {
				x += dx - 1;
				continue;
			}
			for (int dy = 0; ok && dy < height; dy++) {
				ok = !_used[y+dy][x];
			}
			if (!ok) {
				continue;
			}
			found = true;
			px = x;
			py = y;
			break;
		}
		}
		CHECK(found);

		// Place the image to the found place.
		printf("%s %d %d %d %d\n", basename, px, py, width-1, height-1);
		for (int dy = 0; dy < height; dy++) {
		for (int dx = 0; dx < width; dx++) {
			CHECK(!_used[py+dy][px+dx]);
			_used[py+dy][px+dx] = true;
		}
		}
	}
	return 0;
}
