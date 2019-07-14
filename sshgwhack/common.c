#include "headers.h"

void handle_case(const char *expr, const char *file, const char *func, int line)
{
	fprintf(stderr, "unhandled case, errno = %d (%m)\n", errno);
	fprintf(stderr, "in expression '%s'\n", expr);
	fprintf(stderr, "in function %s\n", func);
	fprintf(stderr, "in file %s\n", file);
	fprintf(stderr, "at line %d\n", line);
	abort();
}

int open_fd_count(void)
{
	int fd = open("/dev/null", O_RDONLY);
	HANDLE_CASE(fd == -1);
	HANDLE_CASE(close(fd) == -1);
	return fd;
}

void send_msg(int target_fd, int fd, int length, const void *data)
{
	int16_t headers[2] = { htons(fd), htons(length) };
	HANDLE_CASE(write(target_fd, headers, 4) != 4);
	if (length > 0)
		HANDLE_CASE(write(target_fd, data, length) != length);
}
