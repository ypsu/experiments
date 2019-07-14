#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

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

#define MAX_ENTRIES 10000
#define MAX_DEPTH 16
#define STRSIZE 128

struct entry {
	char dirname[STRSIZE];
	char title[STRSIZE];
	wchar_t title_w[STRSIZE];
};

int current_depth;
char recursion[MAX_DEPTH][STRSIZE];
int recursion_lastpos[MAX_DEPTH];

int entry_count;
struct entry entries[MAX_ENTRIES];
int entry_order[MAX_ENTRIES];

int current_entry;

struct termios orig_termios;
int list_lines = 11;
int terminal_width, terminal_height;

FILE *fifo_poster;
FILE *logger;

int entry_cmp(const void *a, const void *b)
{
	int x = *(const int *)a;
	int y = *(const int *)b;

	return wcscoll(entries[x].title_w, entries[y].title_w);
}

void tty_reset(void)
{
	HANDLE_CASE(tcsetattr(2, TCSAFLUSH, &orig_termios) == -1);
	printf("\e[?25h");
	fflush(stdout);
}

void sigint_handler(int signal)
{
	(void) signal;
	exit(0);
}

void setup_terminal(void)
{
	struct winsize winsize;
	ioctl(2, TIOCGWINSZ, &winsize);
	terminal_width = winsize.ws_col;
	terminal_height = winsize.ws_row;

	HANDLE_CASE(tcgetattr(2, &orig_termios) == -1);
	HANDLE_CASE(atexit(tty_reset) != 0);
	struct termios raw = orig_termios;
	raw.c_lflag &= ~(ECHO | ICANON);

	HANDLE_CASE(tcsetattr(2, TCSAFLUSH, &raw) == -1);
	printf("\e[?25l");
	fflush(stdout);

	signal(SIGINT, sigint_handler);
}

void clear_screen(void)
{
	printf("\e[H\e[2J");
}

const char *static_load_file(const char *path)
{
	static char contents[65536];
	FILE *f = fopen(path, "r");
	HANDLE_CASE(f == NULL);

	HANDLE_CASE(fseek(f, 0, SEEK_END) == -1);
	long len = ftell(f);
	HANDLE_CASE(len >= 65536);
	rewind(f);

	size_t cnt = fread(contents, len, 1, f);
	HANDLE_CASE(cnt != 1);
	contents[len] = 0;

	HANDLE_CASE(fclose(f) == EOF);

	return contents;
}

const char *static_path_concat3(const char *part1, const char *part2, const char *part3)
{
	static char name[4*STRSIZE];
	char *p = name;
	p = stpcpy(p, part1);
	p = stpcpy(p, "/");
	p = stpcpy(p, part2);
	p = stpcpy(p, "/");
	p = stpcpy(p, part3);

	return name;
}

bool is_directory(const char *path)
{
	struct stat stat_buf;
	if (stat(path, &stat_buf) == -1)
		return false;
	return S_ISDIR(stat_buf.st_mode);
}

bool file_exists(const char *path)
{
	struct stat stat_buf;
	return stat(path, &stat_buf) == 0;
}

void populate_entries(void)
{
	const char *curdir = recursion[current_depth-1];
	DIR *dir = opendir(curdir);
	HANDLE_CASE(dir == NULL);

	entry_count = 0;
	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL) {
		if (!is_directory(static_path_concat3(curdir, ent->d_name, "")))
			continue;
		if (ent->d_name[0] == '.')
			continue;
		int name_length = (int) strlen(ent->d_name);
		if (name_length >= STRSIZE)
			continue;
		if (!file_exists(static_path_concat3(curdir, ent->d_name, "title.txt")))
			continue;
		if (!file_exists(static_path_concat3(curdir, ent->d_name, "desc.txt")))
			continue;
		if (!file_exists(static_path_concat3(curdir, ent->d_name, "poster.bmp")))
			continue;

		struct entry *e = &entries[entry_count];
		strcpy(e->dirname, ent->d_name);

		const char *path = static_path_concat3(curdir, ent->d_name, "title.txt");
		const char *title = static_load_file(path);
		strcpy(e->title, title);
		int title_len = strlen(e->title);
		while (title_len > 0 && e->title[title_len-1] == '\n') {
			e->title[--title_len] = 0;
		}
		HANDLE_CASE(title_len <= 0);
		size_t converted = mbstowcs(e->title_w, e->title, STRSIZE);
		HANDLE_CASE(converted == (size_t) -1 || converted == STRSIZE);

		entry_order[entry_count] = entry_count;

		entry_count += 1;
	}

	HANDLE_CASE(closedir(dir) == -1);

	qsort(entry_order, entry_count, sizeof entry_order[0], entry_cmp);
	current_entry = 0;
}

void print_entries(void)
{
	clear_screen();
	int first_entry = current_entry - list_lines/2 + 1;
	for (int i = 0; i < list_lines; ++i) {
		int e = first_entry + i;
		if (e < 0 || e >= entry_count)
			puts("");
		else if (e != current_entry)
			printf("   %3d.  %s\n", e+1, entries[entry_order[e]].title);
		else
			printf("\e[33m-> %3d.  %s\e[0m\n", e+1, entries[entry_order[e]].title);
	}
	for (int i = 0; i < terminal_width; ++i)
		fputs("─", stdout);
	puts("");

	if (entry_count > 0) {
		int e = entry_order[current_entry];
		const char *path = recursion[current_depth-1];
		path = static_path_concat3(path, entries[e].dirname, "desc.txt");
		const char *contents = static_load_file(path);
		printf("%s", contents);
		path = recursion[current_depth-1];
		path = static_path_concat3(path, entries[e].dirname, "poster.bmp");
		fprintf(fifo_poster, "%s\n", path);
		fflush(fifo_poster);
	}
	fflush(stdout);
}

void handle_enter_press(void)
{
	int e = entry_order[current_entry];
	const char *path = recursion[current_depth-1];
	bool is_mkvmovie = file_exists(static_path_concat3(path, entries[e].dirname, "movie.mkv"));
	bool is_avimovie = file_exists(static_path_concat3(path, entries[e].dirname, "movie.avi"));
	bool is_mp4movie = file_exists(static_path_concat3(path, entries[e].dirname, "movie.mp4"));
	bool is_flvmovie = file_exists(static_path_concat3(path, entries[e].dirname, "movie.flv"));

	if (is_mkvmovie || is_avimovie || is_mp4movie || is_flvmovie) {
		clear_screen();
		fflush(stdout);
		time_t cur_time = time(NULL);
		struct tm *tm = localtime(&cur_time);
		char time_buf[STRSIZE];
		char logbuf[2*STRSIZE+1];
		HANDLE_CASE(strftime(time_buf, STRSIZE-1, "%Y/%m/%d (%A) %H:%M", tm) == 0);
		const char *dirname = static_path_concat3(path, entries[e].dirname, "");
		HANDLE_CASE(sprintf(logbuf, "%s %s", time_buf, dirname) >= 2*STRSIZE);
		HANDLE_CASE(fprintf(logger, "-> %s\n", logbuf) <= 0);
		fflush(logger);
		char buf[STRSIZE*4+1];
		const char *opts_file = static_path_concat3(path, entries[e].dirname, "opts.txt");
		static const char copts[] = "-title 'urxvt-floating'";
		char opts[STRSIZE];
		opts[STRSIZE-1] = 0;
		if (file_exists(opts_file))
			snprintf(opts, STRSIZE-1, "%s %s", copts, static_load_file(opts_file));
		else
			sprintf(opts, "%s -alang hun -slang hun -forcedsubsonly", copts);

		int opts_len = strlen(opts);
		while (opts[opts_len-1] == '\n')
			opts[--opts_len] = 0;

		if (is_mkvmovie)
			path = static_path_concat3(path, entries[e].dirname, "movie.mkv");
		else if (is_avimovie)
			path = static_path_concat3(path, entries[e].dirname, "movie.avi");
		else if (is_mp4movie)
			path = static_path_concat3(path, entries[e].dirname, "movie.mp4");
		else
			path = static_path_concat3(path, entries[e].dirname, "movie.flv");
		HANDLE_CASE(sprintf(buf, "mplayer %s %s", opts, path) >= 4*STRSIZE);
		puts(buf);
		fflush(stdout);
		system(buf);

		cur_time = time(NULL);
		tm = localtime(&cur_time);
		HANDLE_CASE(strftime(time_buf, STRSIZE-1, "%Y/%m/%d (%A) %H:%M", tm) == 0);
		dirname = static_path_concat3(recursion[current_depth-1], entries[e].dirname, "");
		HANDLE_CASE(sprintf(logbuf, "%s %s", time_buf, dirname) >= 2*STRSIZE);
		HANDLE_CASE(fprintf(logger, "<- %s\n", logbuf) <= 0);
		fflush(logger);
	} else {
		HANDLE_CASE(current_depth >= MAX_DEPTH);
		HANDLE_CASE(strlen(path)+strlen(entries[e].dirname)+1 >= STRSIZE);
		recursion_lastpos[current_depth-1] = current_entry;
		char *p = recursion[current_depth];
		p = stpcpy(p, path);
		p = stpcpy(p, "/");
		p = stpcpy(p, entries[e].dirname);
		current_depth += 1;
		populate_entries();
	}
}

void handle_esc_press(void)
{
	if (current_depth > 1) {
		current_depth -= 1;
		populate_entries();
		current_entry = recursion_lastpos[current_depth-1];
	} else {
		recursion_lastpos[current_depth-1] = current_entry;
		populate_entries();
		current_entry = recursion_lastpos[current_depth-1];
	}
	if (current_entry >= entry_count)
		current_entry = entry_count-1;
}

int main(void)
{
	int rvi;
	void *rvp;

	rvi = mkfifo("/tmp/movie-selector-poster-fifo", 0777);
	HANDLE_CASE(rvi != 0 && errno != EEXIST);

	fifo_poster = fopen("/tmp/movie-selector-poster-fifo", "w");
	HANDLE_CASE(fifo_poster == NULL);

	rvp = setlocale(LC_COLLATE, "hu_HU.UTF-8");
	HANDLE_CASE(rvp == NULL);
	rvp = setlocale(LC_CTYPE, "hu_HU.UTF-8");
	HANDLE_CASE(rvp == NULL);

	rvp = getcwd(recursion[0], STRSIZE);
	HANDLE_CASE(rvp == NULL);
	current_depth = 1;

	populate_entries();

	HANDLE_CASE(setvbuf(stdout, NULL, _IOFBF, 65536) != 0);
	HANDLE_CASE(setvbuf(fifo_poster, NULL, _IOFBF, 65536) != 0);

	setup_terminal();
	HANDLE_CASE(list_lines+22 > terminal_height);

	logger = fopen("log.txt", "a");
	HANDLE_CASE(logger == NULL);

	print_entries();

	// Signal that everything has started.
	if (file_exists("/tmp/everything-ready")) {
		fclose(fopen("/tmp/everything-ready", "w"));
	}

	while (true) {
		char ch[10];
		int sz;
		sz = read(0, ch, 9);
		system("xdotool key ctrl"); // reset the idle timer (irxevent doesn't reset it)
		ch[sz] = 0;
		if (strcmp(ch, "\e") == 0) {
			// escape
			handle_esc_press();
		} else if (strcmp(ch, "\e[D") == 0 || strcmp(ch, "\eOD") == 0) {
			// left
			handle_esc_press();
		} else if (strcmp(ch, "\e[C") == 0 || strcmp(ch, " ") == 0 || strcmp(ch, "\eOC") == 0) {
			// right
			handle_enter_press();
		} else if (strcmp(ch, "\e[A") == 0 || strcmp(ch, "\eOA") == 0) {
			// up
			if (current_entry > 0)
				current_entry -= 1;
		} else if (strcmp(ch, "\e[B") == 0 || strcmp(ch, "\eOB") == 0) {
			// down
			if (current_entry < entry_count-1)
				current_entry += 1;
		} else if (strcmp(ch, "\e[5~") == 0 || strcmp(ch, "#") == 0) {
			// page up
			current_entry -= list_lines;
			if (current_entry < 0)
				current_entry = 0;
		} else if (strcmp(ch, "\e[6~") == 0 || strcmp(ch, "j") == 0) {
			// page down
			current_entry += list_lines;
			if (current_entry >= entry_count)
				current_entry = entry_count-1;
		} else if (strcmp(ch, "\n") == 0) {
			// enter
			handle_enter_press();
		}
		//for (int i = 0; i < sz; ++i)
			//printf("%d-%c ", ch[i], ch[i]<32 ? ' ' : ch[i]);
		//puts("");
		print_entries();
		fflush(stdout);
	}

	return 0;
}
