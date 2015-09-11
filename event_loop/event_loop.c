// gcc -std=c99 event_loop.c -lanl
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// Utility functions for error handling.
#define CHECK(x) check((x), #x, __FILE__, __LINE__);
void check(bool success, const char *expr, const char *file, int line)
{
	if (!success) {
		const char fmt[] = "%s:%d: Expression \"%s\" failed.\n";
		printf(fmt, file, line, expr);
		printf("errno: %m\n");
		abort();
	}
}
void *xmalloc(int size)
{
	void *p = malloc(size);
	CHECK(p != NULL);
	return p;
}
void xread(int fd, void *buf, int count)
{
	CHECK(read(fd, buf, count) == count);
}
void xwrite(int fd, const void *buf, int count)
{
	CHECK(write(fd, buf, count) == count);
}

// Event handler.
enum { MAX_FD = 64 };
typedef void (*event_callback_t)(int fd, void *data);
static struct {
	bool finished;
	int epoll_fd;
	int command_fd;
	// The descriptor array is indexed by the FD itself.
	struct {
		bool enabled;
		event_callback_t callback;
		void *data;
	} desc[MAX_FD+1];
} events;
void events_ctl(int op, int fd, event_callback_t z_callback, void *z_data)
{
	CHECK(op == EPOLL_CTL_ADD || op == EPOLL_CTL_DEL);
	CHECK(z_callback != NULL || op == EPOLL_CTL_DEL);
	events.desc[fd].enabled = (op == EPOLL_CTL_ADD);
	events.desc[fd].callback = z_callback;
	events.desc[fd].data = z_data;
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = fd;
	CHECK(epoll_ctl(events.epoll_fd, op, fd, &ev) == 0);
}

// Wakeup manager. Use it to wake up from code rather a file descriptor.
typedef void (*wakeup_callback_t)(void *);
struct wakeup_command {
	wakeup_callback_t callback;
	void *data;
};
void wakeup_schedule(wakeup_callback_t callback, void *z_data)
{
	struct wakeup_command cmd;
	cmd.callback = callback;
	cmd.data = z_data;
	xwrite(events.command_fd, &cmd, sizeof cmd);
}
void wakeup_handler(int fd, void *z_data)
{
	(void)z_data;
	struct wakeup_command cmd;
	xread(fd, &cmd, sizeof cmd);
	cmd.callback(cmd.data);
}

// DNS lookup manager.
enum { MAX_DNS_ENTRIES = 16 };
enum { MAX_DNS_WAITING_HANDLERS = 16 };
struct {
	int num;
	struct dns_entry {
		bool finished;
		struct gaicb gaicb;
		// The callbacks to be called when the DNS request finishes.
		int callbacks_count;
		struct dns_callback {
			wakeup_callback_t fn;
			void *data;
		} callbacks[MAX_DNS_WAITING_HANDLERS];
	} entries[MAX_DNS_ENTRIES];
} dns_entries;
// cb will be called when the DNS lookup finishes. host_address will return true
// after this.
void host_lookup(const char *host, wakeup_callback_t z_cb, void *z_cb_data)
{
	for (int i = 0; i < dns_entries.num; ++i) {
		struct dns_entry *e = &dns_entries.entries[i];
		if (strcmp(host, e->gaicb.ar_name) != 0) {
			continue;
		}
		if (z_cb == NULL) {
			return;
		}
		if (e->finished) {
			return;
		}
		CHECK(e->callbacks_count < MAX_DNS_WAITING_HANDLERS);
		e->callbacks[e->callbacks_count].fn = z_cb;
		e->callbacks[e->callbacks_count].data = z_cb_data;
		e->callbacks_count += 1;
		return;
	}
	struct sigevent sev;
	sev.sigev_signo = SIGRTMIN;
	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_value.sival_int = dns_entries.num;
	CHECK(dns_entries.num < MAX_DNS_ENTRIES);
	struct dns_entry *e = &dns_entries.entries[dns_entries.num++];
	e->gaicb.ar_name = strdup(host);
	e->gaicb.ar_service = "80";
	if (z_cb != NULL) {
		e->callbacks_count = 1;
		e->callbacks[0].fn = z_cb;
		e->callbacks[0].data = z_cb_data;
	}
	struct gaicb *ptr = &e->gaicb;
	CHECK(getaddrinfo_a(GAI_NOWAIT, &ptr, 1, &sev) == 0);
}
// Returns true on success, false if the result is not available.
bool host_address(const char *host, struct addrinfo **ai)
{
	for (int i = 0; i < dns_entries.num; ++i) {
		struct dns_entry *e = &dns_entries.entries[i];
		if (strcmp(host, e->gaicb.ar_name) != 0) {
			continue;
		}
		if (!e->finished) {
			return false;
		}
		*ai = e->gaicb.ar_result;
		return true;
	}
	return false;
}
void host_lookup_done(int idx)
{
	struct dns_entry *e = &dns_entries.entries[idx];
	e->finished = true;
	CHECK(gai_error(&e->gaicb) == 0);
	for (int i = 0; i < e->callbacks_count; ++i) {
		struct dns_callback *cb = &e->callbacks[i];
		wakeup_schedule(cb->fn, cb->data);
	}
	e->callbacks_count = 0;
}

// Signal management.
void signalfd_handler(int fd, void *z_data)
{
	(void)z_data;
	struct signalfd_siginfo si;
	xread(fd, &si, sizeof si);
	if ((int)si.ssi_signo == SIGRTMIN && (int)si.ssi_code == SI_ASYNCNL) {
		host_lookup_done(si.ssi_int);
		return;
	}
	printf("Received unexpected signal %d, exiting.\n", (int) si.ssi_signo);
	exit(1);
}

enum { BUFFER_SIZE = 1 * 1024 * 1024 };
enum { MAX_RESULTS = 5 };
struct getpics_state {
	int state;
	char subreddit[64];
	wakeup_callback_t finish_callback;
	void *finish_callback_data;

	int buffer_count;
	char buffer[BUFFER_SIZE];

	int reddit_fd;
	int http_header_length;

	int wget_pipes_count;
	FILE *wget_pipes[MAX_RESULTS];
	char urls[MAX_RESULTS][128];
};
void getpics_wakeup(void *data);
void getpics_handler(int fd, void *data)
{
	struct getpics_state *s = data;
	int r; // Used for return values from syscalls.
	switch (s->state) {
	case 0: {

		// Look up reddit's address asynchronously.
		host_lookup("www.reddit.com", getpics_wakeup, data);

		s->state += 1;
	} case 1: {

		// If we an ip address then open a connection to reddit.
		struct addrinfo *ai;
		if (!host_address("www.reddit.com", &ai))
			return;
		s->reddit_fd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
		CHECK(s->reddit_fd > 0);
		r = connect(s->reddit_fd, ai->ai_addr, ai->ai_addrlen);
		CHECK(r == -1 && errno == EINPROGRESS);
		events_ctl(EPOLL_CTL_ADD, s->reddit_fd, getpics_handler, data);

		// We need to switch the fd to write polling to get notified.
		struct epoll_event e;
		e.events = EPOLLOUT;
		e.data.fd = s->reddit_fd;
		r = epoll_ctl(events.epoll_fd, EPOLL_CTL_MOD, s->reddit_fd, &e);
		CHECK(r == 0);

		s->state += 1;
	} case 2: {

		// Wait until connected.
		if (fd != s->reddit_fd) {
			return;
		}
		int err = -1;
		socklen_t err_sz = sizeof err_sz;
		r = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &err_sz);
		CHECK(err_sz == sizeof err && r == 0);
		CHECK(err == 0);

		// Connected, switch back to read polling.
		struct epoll_event e;
		e.events = EPOLLIN;
		e.data.fd = s->reddit_fd;
		r = epoll_ctl(events.epoll_fd, EPOLL_CTL_MOD, s->reddit_fd, &e);
		CHECK(r == 0);

		// Write the query.
		const char *sr = s->subreddit;
		char host[] = "Host: www.reddit.com";
		char user_agent[] = "User-Agent: test_bot/0.1";
		char fmt[] = "GET /r/%s/top.json HTTP/1.0\r\n%s\r\n%s\r\n\r\n";
		char q[1024];
		int len = snprintf(q, sizeof q, fmt, sr, host, user_agent);
		CHECK(len < (int)sizeof q);
		printf("Downloading /r/%s\n", sr);
		xwrite(s->reddit_fd, q, len);

		s->state += 1;
	} case 3: {

		// Read the response.
		int bufrem = BUFFER_SIZE - s->buffer_count;
		CHECK(bufrem > 0);
		int rby = read(s->reddit_fd, s->buffer+s->buffer_count, bufrem);
		if (rby == -1 && errno == EAGAIN) {
			return;
		}
		if (rby <= 0) {
			printf("Problem reading %s.\n", s->subreddit);
			goto error;
		}
		s->buffer_count += rby;

		// Check whether we have the HTTP header already.
		int header_length = s->buffer_count;
		if (header_length > 1024)
			header_length = 1024;
		char *hdr_end = memmem(s->buffer, header_length, "\r\n\r\n", 4);
		if (hdr_end == NULL) {
			if (header_length < 1024) {
				// We need more data, let's wait.
				return;
			} else {
				printf("HTTP headers too long.\n");
				goto error;
			}
		}
		hdr_end[3] = 0;
		s->http_header_length = hdr_end+4 - s->buffer;

		// Sanity check the headers.
		if (strstr(s->buffer, "HTTP/1.1 200 OK\r\n") != s->buffer) {
			printf("Non-OK response for %s.\n", s->subreddit);
			goto error;
		}
		if (strstr(s->buffer, "Connection: close\r\n") == NULL) {
			printf("Unexpected response for %s.\n", s->subreddit);
			goto error;
		}

		s->state += 1;
	} case 4: {

		// Fully read the body -- until reddit closes the connection.
		int bufrem = BUFFER_SIZE - s->buffer_count;
		CHECK(bufrem > 0);
		int rby = read(s->reddit_fd, s->buffer+s->buffer_count, bufrem);
		if (rby == -1 && errno == EAGAIN) {
			return;
		}
		if (rby == -1) {
			printf("Problem reading %s.\n", s->subreddit);
			goto error;
		}
		s->buffer_count += rby;
		if (rby > 0) {
			// We haven't seen EOF yet, let's wait a bit.
			return;
		}

		// We have everything for offline processing, close the socket.
		events_ctl(EPOLL_CTL_DEL, s->reddit_fd, NULL, NULL);
		CHECK(close(s->reddit_fd) == 0);
		s->reddit_fd = 0;

		// Extract the jpg links from the json and start the the wget
		// commands.
		bufrem = BUFFER_SIZE - s->buffer_count;
		CHECK(bufrem > 0);
		s->buffer[s->buffer_count] = 0;
		char *p = s->buffer + s->http_header_length;
		while ((p = strstr(p, "\"url\": ")) != NULL) {
			p += 8;
			char *e = strchr(p, '"');
			CHECK(e != NULL);
			*e = 0;
			if (e-p < 5 || strcmp(e-4, ".jpg") != 0) {
				p = e+1;
				continue;
			}
			// Disallow malicious input.
			CHECK(strchr(p, '\'') == NULL);
			int sz = sizeof s->urls[0];
			snprintf(s->urls[s->wget_pipes_count], sz, "%s", p);
			char cmd[128];
			r = snprintf(cmd, sizeof cmd, "wget -c -q '%s'", p);
			CHECK(r < (int) sizeof cmd);
			printf("executing: %s\n", cmd);
			FILE *f = popen(cmd, "w");
			CHECK(f != NULL);
			s->wget_pipes[s->wget_pipes_count++] = f;
			int pfd = fileno(f);
			CHECK(pfd > 0);
			int flags = fcntl(pfd, F_GETFL);
			CHECK(flags >= 0);
			CHECK(fcntl(pfd, F_SETFL, flags | O_ASYNC) == 0);
			events_ctl(EPOLL_CTL_ADD, pfd, getpics_handler, data);
			p = e+1;
			if (s->wget_pipes_count == MAX_RESULTS) {
				break;
			}
		}
		if (s->wget_pipes_count == 0) {
			puts("No images found.");
			s->state += 2;
			s->finish_callback(s->finish_callback_data);
			return;
		}

		s->state += 1;
		return;
	} case 5: {

		// Wait until the wget commands finish.
		if (fd == -1) {
			return;
		}
		int idx;
		for (idx = 0; idx < s->wget_pipes_count; ++idx) {
			if (s->wget_pipes[idx] == NULL)
				continue;
			if (fileno(s->wget_pipes[idx]) == fd)
				break;
		}
		CHECK(idx < s->wget_pipes_count);
		events_ctl(EPOLL_CTL_DEL, fd, NULL, NULL);
		r = pclose(s->wget_pipes[idx]);
		CHECK(r >= 0);
		s->wget_pipes[idx] = NULL;
		printf("%s: ", s->urls[idx]);
		if (r > 0) {
			puts("error.");
		} else {
			puts("download succeeded.");
		}
		int active = 0;
		for (int i = 0; i < s->wget_pipes_count; ++i) {
			active += (s->wget_pipes[i] != NULL);
		}
		if (active > 0) {
			return;
		}

		// We are finished, quit.
		s->state += 1;
		s->finish_callback(s->finish_callback_data);
		return;
	} default: {
		CHECK(0);
	}
	};

error:
	if (s->reddit_fd != 0) {
		events_ctl(EPOLL_CTL_DEL, s->reddit_fd, NULL, NULL);
		CHECK(close(s->reddit_fd) == 0);
		s->reddit_fd = 0;
	}
	s->state = -1;
	s->finish_callback(s->finish_callback_data);
}
void getpics_wakeup(void *data)
{
	getpics_handler(-1, data);
}

struct exit_when_zero_state {
	int counter;
};
void exit_when_zero_callback(void *data)
{
	struct exit_when_zero_state *s = data;
	s->counter -= 1;
	if (s->counter <= 0) {
		events.finished = true;
	}
}

int main(int argc, char **argv)
{
	// Initialization.
	CHECK((events.epoll_fd = epoll_create1(0)) != -1);
	enum { MAX_EVENTS = 64 };
	struct epoll_event epoll_events[MAX_EVENTS];
	int command_fd[2];
	CHECK(pipe2(command_fd, O_NONBLOCK) == 0);
	events.command_fd = command_fd[1];
	events_ctl(EPOLL_CTL_ADD, command_fd[0], wakeup_handler, NULL);
	// Signalfd setup.
	int sfd;
	sigset_t sigmask;
	CHECK(sigemptyset(&sigmask) == 0);
	CHECK(sigaddset(&sigmask, SIGRTMIN) == 0);
	CHECK(sigprocmask(SIG_BLOCK, &sigmask, NULL) == 0);
	CHECK((sfd = signalfd(-1, &sigmask, SFD_NONBLOCK)) > 0);
	events_ctl(EPOLL_CTL_ADD, sfd, signalfd_handler, NULL);
	// Start downloading pics from all subreddits simultaneously.
	struct exit_when_zero_state exit_when_zero_data = { .counter = 1 };
	wakeup_schedule(exit_when_zero_callback, &exit_when_zero_data);
	struct getpics_state **states = xmalloc(argc * sizeof states[0]);
	memset(states, 0, argc * sizeof states[0]);
	for (int i = 1; i < argc; ++i) {
		if (strlen(argv[i]) >= sizeof(states[0]->subreddit)) {
			printf("Bad subreddit: %s\n", argv[i]);
			continue;
		}
		exit_when_zero_data.counter += 1;
		struct getpics_state *s = xmalloc(sizeof(*states[0]));
		strcpy(s->subreddit, argv[i]);
		s->finish_callback = exit_when_zero_callback;
		s->finish_callback_data = &exit_when_zero_data;
		states[i] = s;
		getpics_handler(-1, s);
	}

	// Main loop.
	while (!events.finished) {
		int cnt;
		cnt = epoll_wait(events.epoll_fd, epoll_events, MAX_EVENTS, -1);
		if (cnt == -1 && errno == EINTR) {
			continue;
		}
		CHECK(cnt > 0);
		for (int i = 0; i < cnt; ++i) {
			int fd = epoll_events[i].data.fd;
			if (!events.desc[fd].enabled)
				continue;
			events.desc[fd].callback(fd, events.desc[fd].data);
		}
	}

	// Cleanup.
	for (int i = 1; i < argc; ++i) {
		free(states[i]);
	}
	free(states);
	CHECK(close(command_fd[0]) == 0);
	CHECK(close(command_fd[1]) == 0);
	CHECK(close(events.epoll_fd) == 0);
	return 0;
}
