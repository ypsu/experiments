#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <readline/history.h>
#include <readline/readline.h>

// When the condition is true, output the source location and abort
#define HANDLE_CASE(cond) do{ if (cond) handle_case(#cond, __FILE__, __func__, __LINE__); } while(0)
void handle_case(const char *expr, const char *file, const char *func, int line)
{
	printf("unhandled case, errno = %d (%m)\n", errno);
	printf("in expression '%s'\n", expr);
	printf("in function %s\n", func);
	printf("in file %s\n", file);
	printf("at line %d\n", line);
	abort();
}

void swap_stdout_stderr(void)
{
	HANDLE_CASE(dup(1) != 3);
	HANDLE_CASE(dup(2) != 4);
	HANDLE_CASE(close(1) != 0);
	HANDLE_CASE(close(2) != 0);
	HANDLE_CASE(dup(4) != 1);
	HANDLE_CASE(dup(3) != 2);
	HANDLE_CASE(close(3) != 0);
	HANDLE_CASE(close(4) != 0);
}

// 0 <- preprocessed input
// 1 <- stdout
// 2 <- result output
// 4 <- fd for writing to preprocessed input
// 5 <- stdin
// The preprocessed input is used to handle the Escape key for early exit.
void setup_fd(void)
{
	int p[2];
	HANDLE_CASE(pipe(p) != 0);
	HANDLE_CASE(dup(0) != 5);
	HANDLE_CASE(close(0) != 0);
	HANDLE_CASE(dup(3) != 0);
	HANDLE_CASE(close(3) != 0);
}

void reset_fd(void)
{
	HANDLE_CASE(close(0) != 0);
	HANDLE_CASE(close(4) != 0);
	HANDLE_CASE(dup(5) != 0);
}

int child_pid = -1;
int gp_fd = -1;
bool in_eval;
enum { MAX_BUFSIZE = 65536 };
int bufsize;
char old_input[MAX_BUFSIZE];
char gp_output[MAX_BUFSIZE];
bool should_empty;

void start_gp(void)
{
	if (child_pid != -1) {
		HANDLE_CASE(kill(child_pid, SIGKILL) != 0);
		HANDLE_CASE(wait(NULL) != child_pid);
		child_pid = -1;
	}

	if (gp_fd != -1)
		HANDLE_CASE(close(gp_fd) != 0);

	int fd_toexec[2];
	int fd_return[2];
	HANDLE_CASE(pipe(fd_toexec) != 0);
	HANDLE_CASE(pipe(fd_return) != 0);

	int res = fork();
	HANDLE_CASE(res == -1);
	if (res == 0) {
		HANDLE_CASE(close(0) != 0);
		HANDLE_CASE(close(1) != 0);
		HANDLE_CASE(close(2) != 0);
		HANDLE_CASE(close(4) != 0);
		HANDLE_CASE(close(5) != 0);
		HANDLE_CASE(dup(fd_toexec[0]) != 0);
		HANDLE_CASE(dup(fd_return[1]) != 1);
		HANDLE_CASE(dup(fd_return[1]) != 2);
		HANDLE_CASE(close(fd_toexec[0]) != 0);
		HANDLE_CASE(close(fd_toexec[1]) != 0);
		HANDLE_CASE(close(fd_return[0]) != 0);
		HANDLE_CASE(close(fd_return[1]) != 0);
		execl("/usr/bin/gp", "/usr/bin/gp", "-f", "-q", "/home/rlblaster/.gp_helpers", NULL);
		HANDLE_CASE(true);
	} else {
		const char *cmd = rl_line_buffer;
		if (cmd == NULL)
			cmd = "";
		int len = strlen(cmd);
		HANDLE_CASE(write(fd_toexec[1], cmd, len) != len);
		HANDLE_CASE(close(fd_toexec[0]) != 0);
		HANDLE_CASE(close(fd_toexec[1]) != 0);
		HANDLE_CASE(close(fd_return[1]) != 0);
		child_pid = res;
		gp_fd = fd_return[0];
		in_eval = true;
		should_empty = true;
	}
}

int wait_event(void)
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(5, &fds);
	if (gp_fd != -1)
		FD_SET(gp_fd, &fds);

	int evcnt = select(10, &fds, NULL, NULL, NULL);
	if (evcnt == -1 && errno == EINTR)
		return -1;
	HANDLE_CASE(evcnt == -1);
	if (FD_ISSET(5, &fds))
		return 0;
	if (FD_ISSET(gp_fd, &fds))
		return 1;

	abort();
}

void print_output(void)
{
	char buf[MAX_BUFSIZE + 256];
	char *p = buf;
	memcpy(p, "\e[s\e[J\n", 7);
	p += 7;
	memcpy(p, " Result", 7);
	p += 7;
	if (in_eval) {
		memcpy(p, " (in eval):\n", 12);
		p += 12;
	} else {
		memcpy(p, ":\n", 2);
		p += 2;
	}
	memcpy(p, gp_output, bufsize);
	p += bufsize;
	memcpy(p, "\e[u", 3);
	p += 3;
	HANDLE_CASE(write(1, buf, p-buf) != p-buf);

	rl_refresh_line(0, 0);
}

void handle_stdin(void)
{
	char ch[8];
	int rby = read(5, ch, 8);
	HANDLE_CASE(rby == -1);
	if (rby == 0 || (rby == 1 && ch[0] == 27)) {
		reset_fd();
		rl_deprep_terminal();
		add_history(rl_line_buffer);
		write_history("/home/rlblaster/.gpgui_history");
		write(1, "\e[H\e[2J", 7);
		exit(1);
	} else if (ch[0] == 13) {
		reset_fd();
		rl_deprep_terminal();
		add_history(rl_line_buffer);
		write_history("/home/rlblaster/.gpgui_history");
		write(1, "\e[H\e[2J", 7);
		write(2, gp_output, bufsize);
		exit(0);
	}
	HANDLE_CASE(write(4, ch, rby) != rby);
	for (int i = 0; i < rby; ++i)
		rl_callback_read_char();

	int len = strlen(rl_line_buffer);
	HANDLE_CASE(len >= MAX_BUFSIZE);
	if (strcmp(old_input, rl_line_buffer) != 0) {
		strcpy(old_input, rl_line_buffer);
		start_gp();
	}
}

void handle_gp(void)
{
	if (should_empty) {
		bufsize = 0;
		should_empty = false;
	}
	int rby = read(gp_fd, gp_output+bufsize, MAX_BUFSIZE - bufsize);
	HANDLE_CASE(rby == -1);
	bufsize += rby;
	if (rby == 0 || bufsize == MAX_BUFSIZE) {
		HANDLE_CASE(close(gp_fd) != 0);
		gp_fd = -1;
		in_eval = false;
	}
}

int main(void)
{
	swap_stdout_stderr();
	write(1, "\e]2;PARI/GP\007", 12);
	write(1, "\e[H\e[2J", 7);
	read_history("/home/rlblaster/.gpgui_history");
	rl_callback_handler_install("PARI/GP: ", (rl_vcpfunc_t*) 1); // passing NULL is an error
	setup_fd();
	start_gp();

	while (true) {
		print_output();
		int res = wait_event();
		if (res == 0)
			handle_stdin();
		else if (res == 1)
			handle_gp();
	}
	return 0;
}
