#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <SDL/SDL.h>

#define HANDLE_CASE(cond) do{ if (cond) handle_case(#cond, __FILE__, __func__, __LINE__); } while(0)
void handle_case(const char *expr, const char *file, const char *func, int line)
{
	printf("unhandled case, errno = %d (%m)\n", errno);
	printf("in expression '%s'\n", expr);
	printf("in function %s\n", func);
	printf("in file %s\n", file);
	printf("at line %d\n", line);
	exit(1);
}

int fifo;
SDL_Surface *image;
SDL_Surface *screen;

void display_pic(const char *fname)
{
	SDL_Surface *image;
	SDL_Surface *temp;

	temp = SDL_LoadBMP(fname);
	HANDLE_CASE(temp == NULL);

	image = SDL_DisplayFormat(temp);
	SDL_FreeSurface(temp);

	SDL_Rect src, dest;

	dest.x = 0;
	dest.y = 0;
	dest.w = 720;
	dest.h = 1080;
	SDL_FillRect(screen, &dest, 0);

	src.x = 0;
	src.y = 0;
	src.w = image->w;
	src.h = image->h;

	int extra_x = 720 - image->w;
	int extra_y = 1080 - image->h;
	HANDLE_CASE(extra_x < 0 || extra_y < 0);

	dest.x = extra_x;
	dest.y = extra_y / 2;
	dest.w = image->w;
	dest.h = image->h;

	SDL_BlitSurface(image, &src, screen, &dest);
	SDL_FreeSurface(image);

	SDL_Flip(screen);
}

int main(void)
{
	int rvi;

	rvi = mkfifo("/tmp/movie-selector-poster-fifo", 0777);
	HANDLE_CASE(rvi != 0 && errno != EEXIST);

	SDL_putenv("SDL_VIDEO_WINDOW_POS=1198,0");
	HANDLE_CASE(SDL_Init(SDL_INIT_VIDEO) != 0);
	atexit(SDL_Quit);

	screen = SDL_SetVideoMode(720, 1078, 32, SDL_DOUBLEBUF);
	HANDLE_CASE(screen == NULL);

	fifo = open("/tmp/movie-selector-poster-fifo", O_RDONLY);
	HANDLE_CASE(fifo == -1);

	int sz;
	char buf[65536];
	while ((sz = read(fifo, buf, 65535)) > 0) {
		HANDLE_CASE(buf[sz-1] != '\n');
		buf[sz-1] = 0;
		char *p = strrchr(buf, '\n');
		if (p == NULL)
			p = buf;
		else
			p += 1;
		display_pic(p);
	}

	HANDLE_CASE(false);
	return 0;
}
