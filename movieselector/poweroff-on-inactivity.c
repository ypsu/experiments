#define _GNU_SOURCE
#include <X11/extensions/scrnsaver.h>
#include <errno.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

const char mplayer[] = "mplayer";

bool mplayer_running(void)
{
	DIR *dir = opendir("/proc");
	HANDLE_CASE(dir == NULL);

	bool found = false;
	struct dirent *ent;
	while (!found && (ent = readdir(dir)) != NULL) {
		char buf1[2048];
		char buf2[2048];
		sprintf(buf1, "/proc/%s/exe", ent->d_name);
		ssize_t len = readlink(buf1, buf2, 2047);
		if (len == -1)
			continue;
		buf2[len] = 0;
		if (len >= (int)sizeof(mplayer)-1 && strcmp(buf2+len-sizeof(mplayer)+1, mplayer) == 0)
			found = true;
	}

	closedir(dir);
	return found;
}

int main(void)
{
	Display *dpy = XOpenDisplay(NULL);

	if (!dpy) {
		return 1;
	}

	XScreenSaverInfo *info = XScreenSaverAllocInfo();

	while (true) {
		sleep(300);
		unsigned limit = 60*1000;
		limit *= mplayer_running() ? 3*60 : 45;
		XScreenSaverQueryInfo(dpy, DefaultRootWindow(dpy), info);
		if (info->idle > limit)
			system("poweroff");
	}

	return 0;
}
