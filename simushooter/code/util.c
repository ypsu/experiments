#include "headers.h"

enum { _SCRATCHPAD_SIZE = 1024 * 1024 * 20 };
enum { _MAX_EXIT_FUNCTIONS = 4 };
static struct {
	// Memory for the per tick scratch allocation.
	char scratchpad[_SCRATCHPAD_SIZE];
	int scratchpad_allocated;

	int exit_functions_count;
	void (*exit_functions[_MAX_EXIT_FUNCTIONS])(void);
}
_v;

static void
_run_exit_functions(void)
{
	for (int i = _v.exit_functions_count-1; i >= 0; --i) {
		_v.exit_functions[i]();
	}
}

void
util_check(bool ok, const char *cond, const char *file, int line)
{
	if (ok)
		return;
	const char *msg = "CHECK(%s) failed at %s:%d.\n";
	printf(msg, cond, file, line);
	_run_exit_functions();
	abort();
}

void *
qalloc(int len)
{
	len = (len+15) & ~15;
	CHECK(_v.scratchpad_allocated + len <= _SCRATCHPAD_SIZE);
	_v.scratchpad_allocated += len;
	return &_v.scratchpad[_v.scratchpad_allocated - len];
}

const char *
qprintf(const char *format, ...)
{
	char *s = qalloc(MAXSTR);
	va_list ap;
	va_start(ap, format);
	int n = vsnprintf(s, MAXSTR, format, ap);
	va_end(ap);
	CHECK(n < MAXSTR);
	return s;
}

void *
util_load_file(const char *fname, int *length)
{
	FILE *f = fopen(fname, "rb");
	CHECK(f != NULL);
	CHECK(fseek(f, 0, SEEK_END) == 0);
	int len = ftell(f);
	CHECK(len >= 0);
	CHECK(fseek(f, 0, SEEK_SET) == 0);

	void *buf = qalloc(len);
	CHECK(fread(buf, len, 1, f) == 1);
	CHECK(fclose(f) == 0);

	*length = len;
	return buf;
}

void
onexit(void (*f)())
{
	static bool initialized;
	if (!initialized) {
		atexit(_run_exit_functions);
	}
	CHECK(_v.exit_functions_count < _MAX_EXIT_FUNCTIONS);
	_v.exit_functions[_v.exit_functions_count++] = f;
}


void
util_tick(void)
{
	_v.scratchpad_allocated = 0;
}
