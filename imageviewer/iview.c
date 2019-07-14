#define _GNU_SOURCE
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <IL/il.h>
#include <IL/ilu.h>
#include <SDL/SDL.h>
#include <libexif/exif-loader.h>

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

static inline void swap(int *a, int *b)
{
	int t = *a;
	*a = *b;
	*b = t;
}

void check_errors(void)
{
	HANDLE_CASE(ilGetError() != IL_NO_ERROR);
}

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
const Uint32 rmask = 0xff000000;
const Uint32 gmask = 0x00ff0000;
const Uint32 bmask = 0x0000ff00;
const Uint32 amask = 0x000000ff;
#else
const Uint32 rmask = 0x000000ff;
const Uint32 gmask = 0x0000ff00;
const Uint32 bmask = 0x00ff0000;
const Uint32 amask = 0xff000000;
#endif

pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t global_condvar = PTHREAD_COND_INITIALIZER;

SDL_Surface *screen;
ILuint image_name;
int rotate_count;
char temp_buffer[2000][2000][3];

#define ACTIVE_BUFFERS 32
int curfile;

#define MAX_FILES 65536
#define MAX_FILENAME_LENGTH 127
int file_count;
char filenames[MAX_FILES][MAX_FILENAME_LENGTH+1];
SDL_Surface *surfaces[MAX_FILES];
int file_order[MAX_FILES];

bool ends_with(const char *str, const char *suffix)
{
	size_t len_a = strlen(str);
	size_t len_b = strlen(suffix);
	if (len_a < len_b)
		return false;
	return strcasecmp(str+len_a-len_b, suffix) == 0;
}

bool is_directory(const char *path)
{
	struct stat stat_buf;
	if (stat(path, &stat_buf) == -1)
		return false;
	return S_ISDIR(stat_buf.st_mode);
}

int cmp_order(const void *a, const void *b)
{
	int aa = *(const int*)a;
	int bb = *(const int*)b;
	return strcasecmp(filenames[aa], filenames[bb]);
}

void load_directory(void)
{
	DIR *dir = opendir(".");
	HANDLE_CASE(dir == NULL);
	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL) {
		if (is_directory(ent->d_name))
			continue;
		if (ent->d_name[0] == '.')
			continue;
		int name_length = (int) strlen(ent->d_name);
		HANDLE_CASE(name_length >= MAX_FILENAME_LENGTH);
		if (true
				&& !ends_with(ent->d_name, "jpg")
				&& !ends_with(ent->d_name, "jpeg")
			)
		{
			continue;
		}
		HANDLE_CASE(file_count >= MAX_FILES);
		strcpy(filenames[file_count], ent->d_name);
		file_order[file_count] = file_count;
		file_count += 1;
	}
	qsort(file_order, file_count, sizeof file_order[0], cmp_order);
}

int rotation(const char *filename)
{
	int rot = 0;
	ExifData *ed;
	ed = exif_data_new_from_file(filename);
	if (ed == NULL)
		goto rotation_exit;

	ExifEntry *ef = exif_content_get_entry(ed->ifd[EXIF_IFD_0], EXIF_TAG_ORIENTATION);
	if (ef == NULL)
		goto rotation_exit;

	char buf[1024];
	exif_entry_get_value(ef, buf, sizeof buf);

	if (buf[0] == 0)
		goto rotation_exit;

	if (strcasecmp(buf, "left-bottom") == 0)
		rot = 1;
	else if (strcasecmp(buf, "bottom-right") == 0)
		rot = 2;
	else if (strcasecmp(buf, "right-top") == 0)
		rot = 3;

rotation_exit:
	exif_data_unref(ed);
	return rot;
}

void resize(int *w, int *h)
{
	double sw = 1920.0 / *w;
	double sh = 1080.0 / *h;
	double s = sw<sh ? sw : sh;
	*w = lrint(*w * s);
	*h = lrint(*h * s);
}

SDL_Surface *create_surface(const char *filename)
{
	rotate_count = rotation(filename);
	ilLoadImage(filename);
	int w = (int) ilGetInteger(IL_IMAGE_WIDTH);
	int h = (int) ilGetInteger(IL_IMAGE_HEIGHT);
	if (rotate_count % 2 == 1)
		swap(&w, &h);
	resize(&w, &h);
	if (rotate_count % 2 == 1)
		swap(&w, &h);
	iluScale(w, h, 1);
	assert(w == (int) ilGetInteger(IL_IMAGE_WIDTH));
	assert(h == (int) ilGetInteger(IL_IMAGE_HEIGHT));
	if (rotate_count % 2 == 1)
		swap(&w, &h);

	SDL_Surface *surface;
	surface = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 24, rmask, gmask, bmask, 0);
	HANDLE_CASE(surface == 0);

	HANDLE_CASE(SDL_LockSurface(surface) == -1);

	typedef char char3[3];

	if (rotate_count == 0) {
		char *dst = surface->pixels;
		for (int r = 0; r < h; ++r) {
			ilCopyPixels(0, r, 0, w, 1, 1, IL_RGB, IL_UNSIGNED_BYTE, dst);
			dst += surface->pitch;
		}
	} else if (rotate_count == 1) {
		for (int r = 0; r < w; ++r)
			ilCopyPixels(0, r, 0, h, 1, 1, IL_RGB, IL_UNSIGNED_BYTE, temp_buffer[r]);
		char3 *dst = surface->pixels;
		for (int r = 0; r < h; ++r) {
			for (int c = 0; c < w; ++c) {
				for (int k = 0; k < 3; ++k)
					dst[c][k] = temp_buffer[c][h-1-r][k];
			}
			dst = (char3*) (surface->pitch + (char*)dst);
		}
	} else if (rotate_count == 2) {
		for (int r = 0; r < h; ++r)
			ilCopyPixels(0, r, 0, w, 1, 1, IL_RGB, IL_UNSIGNED_BYTE, temp_buffer[r]);
		char3 *dst = surface->pixels;
		for (int r = 0; r < h; ++r) {
			for (int c = 0; c < w; ++c) {
				for (int k = 0; k < 3; ++k) {
					dst[c][k] = temp_buffer[h-1-r][w-1-c][k];
				}
			}
			dst = (char3*) (surface->pitch + (char*)dst);
		}
	} else if (rotate_count == 3) {
		for (int r = 0; r < w; ++r)
			ilCopyPixels(0, r, 0, h, 1, 1, IL_RGB, IL_UNSIGNED_BYTE, temp_buffer[r]);
		char3 *dst = surface->pixels;
		for (int r = 0; r < h; ++r) {
			for (int c = 0; c < w; ++c) {
				for (int k = 0; k < 3; ++k)
					dst[c][k] = temp_buffer[w-1-c][r][k];
			}
			dst = (char3*) (surface->pitch + (char*)dst);
		}
	}

	SDL_UnlockSurface(surface);

	return surface;
}

void show_surface(SDL_Surface *s)
{
	SDL_Rect src, dest;

	dest.x = 0;
	dest.y = 0;
	dest.w = screen->w;
	dest.h = screen->h;
	SDL_FillRect(screen, &dest, 0);

	int x = screen->w - s->w;
	if (x < 0)
		x = 0;
	x /= 2;

	int y = screen->h - s->h;
	if (y < 0)
		y = 0;
	y /= 2;

	src.x = 0;
	src.y = 0;
	src.w = s->w;
	src.h = s->h;
	dest.x = x;
	dest.y = y;

	HANDLE_CASE(SDL_BlitSurface(s, &src, screen, &dest) == -1);
	SDL_Flip(screen);
}

// Assumption: we are under the lock
void _check_and_load(int file)
{
	if (surfaces[file] != NULL)
		return;

	pthread_mutex_unlock(&global_mutex);
	SDL_Surface *s = create_surface(filenames[file_order[file]]);
	pthread_mutex_lock(&global_mutex);

	surfaces[file] = s;
	pthread_cond_signal(&global_condvar);
}

bool inside(int x, int lo, int hi)
{
	return lo <= x && x <= hi;
}

void *loader_threadfunc(void *arg)
{
	(void) arg;
	pthread_mutex_lock(&global_mutex);

	while (true) {
		int start_curfile = curfile;
		int lo = (curfile + 2*file_count - 2) % file_count;
		int hi = (curfile + 2*file_count + ACTIVE_BUFFERS-3) % file_count;
		if (file_count <= ACTIVE_BUFFERS) {
			lo = 0;
			hi = file_count - 1;
		}
		if (hi < lo)
			hi += file_count;
		for (int file = 0; file < file_count; ++file) {
			if (inside(file, lo, hi) || inside(file+file_count, lo, hi)) {
				_check_and_load(file);
			} else {
				if (surfaces[file] != NULL) {
					SDL_FreeSurface(surfaces[file]);
					surfaces[file] = NULL;
				}
			}
		}
		if (start_curfile != curfile)
			continue;
		pthread_cond_wait(&global_condvar, &global_mutex);
	}

	return NULL;
}

// Assumption: we are under the lock
SDL_Surface *_get_surface(int file)
{
	if (surfaces[file] == NULL) {
		SDL_Rect dest;
		dest.x = 0;
		dest.y = 0;
		dest.w = 32;
		dest.h = 32;
		SDL_FillRect(screen, &dest, (255<<8) + (255<<16));
		SDL_Flip(screen);
	}

	while (surfaces[file] == NULL) {
		pthread_cond_wait(&global_condvar, &global_mutex);
	}
	return surfaces[file];
}

int last_rendered = -1;
void render_curfile()
{
	pthread_mutex_lock(&global_mutex);
	if (curfile != last_rendered) {
		last_rendered = curfile;
		show_surface(_get_surface(curfile));
	}
	pthread_mutex_unlock(&global_mutex);
}

int main(void)
{
	HANDLE_CASE(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) != 0);
	signal(SIGINT, SIG_DFL);
	atexit(SDL_Quit);
	screen = SDL_SetVideoMode(1920, 1080, 24, SDL_DOUBLEBUF);
	HANDLE_CASE(screen == NULL);
	SDL_ShowCursor(SDL_DISABLE);

	HANDLE_CASE(ilGetInteger(IL_VERSION_NUM) < IL_VERSION);
	HANDLE_CASE(iluGetInteger(ILU_VERSION_NUM) < ILU_VERSION);
	ilInit();
	ilGenImages(1, &image_name);
	ilBindImage(image_name);
	iluImageParameter(ILU_FILTER, ILU_BILINEAR);
	check_errors();

	load_directory();

	if (file_count == 0)
		return 0;

	pthread_mutex_lock(&global_mutex);
	_check_and_load(curfile);
	pthread_mutex_unlock(&global_mutex);
	render_curfile();
	pthread_mutex_lock(&global_mutex);
	_check_and_load((curfile+1) % file_count);
	pthread_mutex_unlock(&global_mutex);

	pthread_t loader_thread;
	HANDLE_CASE(pthread_create(&loader_thread, NULL, loader_threadfunc, NULL));

	SDL_Event event;
	SDL_WaitEvent(&event);

	while (true) {
		if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.sym == SDLK_ESCAPE) {
				break;
			} else if (event.key.keysym.sym == SDLK_LEFT) {
				pthread_mutex_lock(&global_mutex);
				curfile = (curfile + file_count - 1) % file_count;
				pthread_cond_signal(&global_condvar);
				pthread_mutex_unlock(&global_mutex);
				render_curfile();
			} else if (event.key.keysym.sym == SDLK_RIGHT) {
				pthread_mutex_lock(&global_mutex);
				curfile = (curfile + 1) % file_count;
				pthread_cond_signal(&global_condvar);
				pthread_mutex_unlock(&global_mutex);
				render_curfile();
			}
		} else if (event.type == SDL_QUIT) {
			break;
		}

		if (SDL_PollEvent(&event) == 0) {
			SDL_WaitEvent(&event);
		}
	}

	ilDeleteImage(image_name);
	_exit(0);
	return 0;
}
