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
int terminal_width, terminal_height;

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

		struct entry *e = &entries[entry_count];
		strcpy(e->dirname, ent->d_name);

		strcpy(e->title, ent->d_name);
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
	int first_entry = current_entry - terminal_height/2 + 1;
	for (int i = 0; i < terminal_height; ++i) {
		int e = first_entry + i;
		if (e < 0 || e >= entry_count)
			puts("");
		else if (e != current_entry)
			printf("   %3d.  %s\n", e+1, entries[entry_order[e]].title);
		else
			printf("\e[33m-> %3d.  %s\e[0m\n", e+1, entries[entry_order[e]].title);
	}
	fflush(stdout);
}

void handle_enter_press(void)
{
	int e = entry_order[current_entry];

	clear_screen();
	fflush(stdout);

	char cmdbuf[2*STRSIZE+1];
	HANDLE_CASE(sprintf(cmdbuf, "cd '%s'; iview", entries[e].dirname) >= 2*STRSIZE);
	system(cmdbuf);
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
	void *rvp;

	rvp = setlocale(LC_COLLATE, "hu_HU.UTF-8");
	HANDLE_CASE(rvp == NULL);
	rvp = setlocale(LC_CTYPE, "hu_HU.UTF-8");
	HANDLE_CASE(rvp == NULL);

	rvp = getcwd(recursion[0], STRSIZE);
	HANDLE_CASE(rvp == NULL);
	current_depth = 1;

	populate_entries();

	HANDLE_CASE(setvbuf(stdout, NULL, _IOFBF, 65536) != 0);

	setup_terminal();
	terminal_height = 35; // ugly workaround for the height issue

	print_entries();

	while (true) {
		char ch[10];
		int sz;
		sz = read(1, ch, 9);
		system("xdotool key ctrl"); // reset the idle timer (irxevent doesn't reset it)
		ch[sz] = 0;
		if (strcmp(ch, "\e") == 0) {
			// escape
			handle_esc_press();
		} else if (strcmp(ch, "\e[D") == 0) {
			// left
			handle_esc_press();
		} else if (strcmp(ch, "\e[C") == 0 || strcmp(ch, " ") == 0) {
			// right
			handle_enter_press();
		} else if (strcmp(ch, "\e[A") == 0) {
			// up
			if (current_entry > 0)
				current_entry -= 1;
		} else if (strcmp(ch, "\e[B") == 0) {
			// down
			if (current_entry < entry_count-1)
				current_entry += 1;
		} else if (strcmp(ch, "\e[5~") == 0 || strcmp(ch, "#") == 0) {
			// page up
			current_entry -= terminal_height;
			if (current_entry < 0)
				current_entry = 0;
		} else if (strcmp(ch, "\e[6~") == 0 || strcmp(ch, "j") == 0) {
			// page down
			current_entry += terminal_height;
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
