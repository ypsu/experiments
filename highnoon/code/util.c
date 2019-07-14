#include "headers.h"

enum { MAXSTR_PER_TICK = 128 };

static int _strings_allocated;
static char _strings[MAXSTR_PER_TICK][MAXSTR];

void util_tick(void)
{
	_strings_allocated = 0;
	state.debuglines_count = 0;
}

void *smalloc(int len)
{
	void *p = malloc(len);
	HANDLE_CASE(p == NULL);
	return p;
}

void handle_case(bool failed, const char *cond, const char *file, int line)
{
	if (failed) {
		const char *msg = "Unhandled condition \"%s\" at %s:%d!\n";
		printf(msg, cond, file, line);
		kill(getpid(), SIGABRT);
	}
}

const char *qprintf(const char *format, ...)
{
	HANDLE_CASE(_strings_allocated == MAXSTR_PER_TICK);
	char *s = _strings[_strings_allocated++];
	va_list ap;
	va_start(ap, format);
	int n = vsnprintf(s, MAXSTR, format, ap);
	va_end(ap);
	HANDLE_CASE(n >= MAXSTR);
	return s;
}

void set_status(int level, const char *msg)
{
	assert(0 <= level && level < STATUS_MSG_MAX);
	state.status_msg_last_frame[level] = state.frame_id + 5*60;
	strcpy(state.status_msg[level], msg);
}

void add_debugline(const rect_t *r)
{
	HANDLE_CASE(state.debuglines_count == DEBUGLINES_MAX);
	state.debuglines[state.debuglines_count++] = *r;
}
