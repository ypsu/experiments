#include "headers.h"

bool fd_open[34567];

int epoll_fd;
int fdbase;
socklen_t addrlen;
struct sockaddr_storage addr;

void open_fd(int fd)
{
	int res = socket(AF_INET, SOCK_STREAM, 0);
	HANDLE_CASE(fd != res);
	res = connect(fd, (struct sockaddr*) &addr, addrlen);
	if (res == -1) {
		HANDLE_CASE(close(fd) == -1);
		send_msg(1, fd-fdbase, 0, NULL);
		return;
	}
	fd_open[fd] = true;

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = fd;
	HANDLE_CASE(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1);
}

void set_target(const char *hostname, const char *port)
{
	struct addrinfo *info;
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	int res = getaddrinfo(hostname, port, &hints, &info);
	if (res != 0) {
		fprintf(stderr, "DNS lookup error: %s\n", gai_strerror(res));
		exit(2);
	}
	addrlen = info->ai_addrlen;
	assert(addrlen <= sizeof addr);
	memcpy(&addr, info->ai_addr, addrlen);
	freeaddrinfo(info);
}

void handle_msg_from_remote(void)
{
	static char buf[50000];
	static int rby;
	int res = read(0, buf+rby, 50000-rby);
	HANDLE_CASE(res <= 0);
	rby += res;

	while (rby >= 4) {
		int16_t fd;
		int16_t len;
		memcpy(&fd, buf, 2);
		memcpy(&len, buf+2, 2);
		fd = ntohs(fd);
		len = ntohs(len);
		HANDLE_CASE(len < 0);
		if (fd < 0) {
			HANDLE_CASE(len != 0);
			memmove(buf, buf+4, rby-4);
			rby -= 4;
			continue;
		}
		if (rby-4 < len)
			return;

		bool new_conn = !fd_open[fd + fdbase];
		if (new_conn)
			open_fd(fd + fdbase);

		if (len > 0 && fd_open[fd + fdbase]) {
			res = write(fd + fdbase, buf+4, len);
			// Deliberately ignore error (it will be picked up by epoll and
			// the connection will be closed properly).
			HANDLE_CASE(res != -1 && res != len);
		}

		memmove(buf, buf+4+len, rby-4-len);
		rby -= 4 + len;

		if (!new_conn && len == 0) {
			HANDLE_CASE(close(fd + fdbase) == -1);
			fd_open[fd + fdbase] = false;
		}
	}
}

void handle_msg_to_remote(int fd)
{
	char buf[30000];
	int rby = read(fd, buf, 30000);

	if (rby <= 0) {
		fd_open[fd] = false;
		close(fd);
		rby = 0;
	}

	send_msg(1, fd-fdbase, rby, buf);
}

int main(void)
{
	char buffer[100];
	HANDLE_CASE(write(1, "HLDS", 4) != 4);
	HANDLE_CASE(read(0, buffer, 4) != 4);
	if (memcmp(buffer, "SDLH", 4) != 0) {
		const char errmsg[] = "Expected SDLH but got: ";
		write(2, errmsg, (sizeof errmsg)-1);
		write(2, buffer, 4);
		write(2, "\n", 1);
		exit(3);
	}

	char buf[64];
	HANDLE_CASE(read(0, buf, 64) != 64);
	char *hostname = buf;
	char *port = memchr(buf, ' ', 64);
	HANDLE_CASE(port == NULL);
	*port++ = 0;
	set_target(hostname, port);

	epoll_fd = epoll_create(10);
	HANDLE_CASE(epoll_fd == -1);

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = 0;
	HANDLE_CASE(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, 0, &ev) == -1);

	fdbase = open_fd_count();
	while (true) {
		struct epoll_event events[16];
		int rc = epoll_wait(epoll_fd, events, 16, -1);
		if (rc == -1 && errno == EINTR)
			continue;
		HANDLE_CASE(rc == -1);

		for (int i = 0; i < rc; ++i) {
			int fd = events[i].data.fd;

			if (fd == 0) {
				HANDLE_CASE(events[i].events != EPOLLIN);
				handle_msg_from_remote();
			} else {
				HANDLE_CASE(fd < fdbase);
				assert(fd_open[fd]);
				handle_msg_to_remote(fd);
			}
		}
	}

	return 0;
}
