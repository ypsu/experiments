#include "headers.h"

int keep_alive = -1;
int listen_port = 2222;
int child_pid;
int child_fd;
int fdbase;
const char optstring[] = "+a:c:hkp:";
char address[128];

int epoll_fd;
int listener_fd;

void handle_exit(int sig)
{
	fprintf(stderr, "Received signal %d, quitting.\n", sig);
	kill(child_pid, SIGKILL);
	exit(0);
}

void print_usage(void)
{
	puts("Usage: loc [OPTION]... [-p PORT] [-a ADDRESS] [-c COMMAND]... ssh [SSH_ARGS]...");
	puts("");
	puts("-a ADDRESS     the address to which the remote side should connect in");
	puts("               'machine port' format without the quotes");
	puts("-c COMMAND     command to execute on the remote side before starting multiplexing;");
	puts("               the last command must be the rem command");
	puts("-h             print this usage and exit");
	puts("-k             keep the connection alive by sending heartbeat every 2 minutes of");
	puts("               inactivity");
	puts("-p PORT        local port on which to listen for connections");
}

void parse_args(int argc, char **argv)
{
	optind = 0;
	int opt;
	while ((opt = getopt(argc, argv, optstring)) != -1) {
		switch (opt) {
		case 'a':
			{
				char *space;
				bool ok = true;;
				ok = true;
				ok = ok && strlen(optarg) < 60;
				ok = ok && (space = strchr(optarg, ' ')) != NULL;
				ok = ok && strchr(space+1, ' ') == NULL;
				if (!ok) {
					puts("Address must be less than 60 characters long and");
					puts("must contain single space!");
					exit(4);
				}
				strcpy(address, optarg);
				break;
			}
		case 'h':
			print_usage();
			exit(0);
			break;
		case 'k':
			keep_alive = 2*60*1000;
			break;
		case 'p':
			listen_port = atoi(optarg);
			break;
		case '?':
			exit(1);
			break;
		default:
			break;
		}
	}
	if (address[0] == 0) {
		puts("Missing remote address!");
		exit(5);
	}
	if (argv[optind] == NULL) {
		puts("Missing command to execute!");
		exit(1);
	}
}

void run_command(int argc, char **argv)
{
	int fd[2];
	HANDLE_CASE(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == -1);

	child_pid = fork();
	HANDLE_CASE(child_pid == -1);
	if (child_pid == 0) {
		child_fd = fd[1];
		HANDLE_CASE(close(0) == -1);
		HANDLE_CASE(close(1) == -1);
		HANDLE_CASE(dup(child_fd) != 0);
		HANDLE_CASE(dup(child_fd) != 1);
		HANDLE_CASE(close(child_fd) == -1);
		execvp(argv[optind], argv+optind);
		perror("execve()");
		kill(getppid(), SIGINT);
		exit(2);
	} else {
		child_fd = fd[0];
		optind = 0;
		int opt;
		while ((opt = getopt(argc, argv, optstring)) != -1) {
			if (opt == 'c') {
				int len = strlen(optarg);
				write(child_fd, optarg, len);
				write(child_fd, "\n", 1);
			}
		}

		struct epoll_event ev;
		ev.events = EPOLLIN;
		ev.data.fd = child_fd;
		HANDLE_CASE(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, child_fd, &ev) == -1);
	}
}

void setup_listening(void)
{
	epoll_fd = epoll_create(10);
	HANDLE_CASE(epoll_fd == -1);

	listener_fd = socket(AF_INET, SOCK_STREAM, 0);
	HANDLE_CASE(listener_fd == -1);

	int val = 1;
	HANDLE_CASE(setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof val) == -1);

	struct sockaddr_in server_sa;
	server_sa.sin_family = AF_INET;
	server_sa.sin_port = htons(listen_port);
	server_sa.sin_addr.s_addr = INADDR_ANY;
	HANDLE_CASE(bind(listener_fd, &server_sa, sizeof server_sa) == -1);
	HANDLE_CASE(listen(listener_fd, 128) == -1);

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = listener_fd;
	HANDLE_CASE(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listener_fd, &ev) == -1);
}

void handle_new_connection(void)
{
	int fd = accept(listener_fd, NULL, NULL);
	HANDLE_CASE(fd == -1);

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = fd;
	HANDLE_CASE(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1);

	send_msg(child_fd, fd-fdbase, 0, NULL);
}

void handle_msg_from_remote(void)
{
	static char buf[50000];
	static int rby;
	int res = read(child_fd, buf+rby, 50000-rby);
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

		if (len > 0) {
			res = write(fd + fdbase, buf+4, len);
			// Deliberately ignore error (it will be picked up by epoll and
			// the connection will be closed properly).
			HANDLE_CASE(res != -1 && res != len);
		}

		memmove(buf, buf+4+len, rby-4-len);
		rby -= 4 + len;

		if (len == 0)
			HANDLE_CASE(close(fd + fdbase) == -1);
	}
}

void handle_msg_to_remote(int fd)
{
	char buf[30000];
	int rby = read(fd, buf, 30000);

	if (rby <= 0) {
		close(fd);
		rby = 0;
	}

	send_msg(child_fd, fd-fdbase, rby, buf);
}

int main(int argc, char **argv)
{
	HANDLE_CASE(signal(SIGTERM, handle_exit) == SIG_ERR);
	HANDLE_CASE(signal(SIGINT, handle_exit) == SIG_ERR);
	HANDLE_CASE(signal(SIGABRT, handle_exit) == SIG_ERR);
	HANDLE_CASE(signal(SIGCHLD, handle_exit) == SIG_ERR);

	parse_args(argc, argv);
	setup_listening();
	run_command(argc, argv);

	// Sanity handshake
	char buffer[16];
	HANDLE_CASE(read(child_fd, buffer, 4) != 4);
	HANDLE_CASE(memcmp(buffer, "HLDS", 4) != 0);
	HANDLE_CASE(write(child_fd, "SDLH", 4) != 4);

	HANDLE_CASE(write(child_fd, address, 64) != 64);

	fdbase = open_fd_count();

	while (true) {
		struct epoll_event events[16];
		int rc = epoll_wait(epoll_fd, events, 16, keep_alive);
		if (rc == -1 && errno == EINTR)
			continue;
		HANDLE_CASE(rc == -1);
		if (rc == 0 && keep_alive != -1) {
			send_msg(child_fd, -1, 0, NULL);
		}
		for (int i = 0; i < rc; ++i) {
			int fd = events[i].data.fd;
			if (fd == listener_fd) {
				HANDLE_CASE(events[i].events != EPOLLIN);
				handle_new_connection();
			} else if (fd == child_fd) {
				HANDLE_CASE(events[i].events != EPOLLIN);
				handle_msg_from_remote();
			} else {
				HANDLE_CASE(fd < fdbase);
				handle_msg_to_remote(fd);
			}
		}
	}

	return 0;
}
