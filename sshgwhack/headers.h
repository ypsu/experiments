#define _GNU_SOURCE
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

// When the condition is true, output the source location and abort
#define HANDLE_CASE(cond) do{ if (cond) handle_case(#cond, __FILE__, __func__, __LINE__); } while(0)
void handle_case(const char *expr, const char *file, const char *func, int line);

int open_fd_count(void);
void send_msg(int target_fd, int fd, int length, const void *data);
