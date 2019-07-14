#define _GNU_SOURCE
#include <execinfo.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

enum { _MAX_STACK_DEPTH = 16 };

struct _allocinfo {
	int64_t allocs_count, bytes_used;
	int addresses_count;
	void *addresses[_MAX_STACK_DEPTH];
};

struct _alloc_header {
	int size;
	int cap;
	int info_id;
};

// All _allocinfos we track are stored here. Use _allocinfo_get to get a pointer
// from this store.
static int _total_bytes;
static int _ais_count, _ais_capacity;
static struct _allocinfo *_ais;

static int
_allocinfo_cmp(const struct _allocinfo *a, const struct _allocinfo *b)
{
	if (a->addresses_count != b->addresses_count)
		return a->addresses_count - b->addresses_count;
	int cnt = a->addresses_count;
	for (int i = 0; i < cnt; ++i) {
		if (a->addresses[i] < b->addresses[i])
			return -1;
		if (a->addresses[i] > b->addresses[i])
			return 1;
	}
	return 0;
}

// Return the index of this allocinfo into the array of all allocinfos.
static int
_allocinfo_get(const struct _allocinfo *like_this)
{
	// Find the position where this ai should be.
	int pos = 0;
	while (pos < _ais_count && _allocinfo_cmp(&_ais[pos], like_this) != 0)
		pos += 1;
	if (pos < _ais_count)
		return pos;

	// We seen this the first time: add it into the array.
	if (_ais_count == _ais_capacity) {
		// Make room in the array.
		_ais_capacity = _ais_count * 2;
		if (_ais_capacity < 1024)
			_ais_capacity = 1024;
		struct _allocinfo *newais;
		size_t oldlength = _ais_count * sizeof _ais[0];
		size_t length = _ais_capacity * sizeof _ais[0];
		int prot = PROT_READ | PROT_WRITE;
		int flags = MAP_PRIVATE | MAP_ANONYMOUS;
		newais = mmap(NULL, length, prot, flags, -1, 0);
		if (newais == MAP_FAILED)
			abort();
		memcpy(newais, _ais, oldlength);
		if (_ais != NULL && munmap(_ais, oldlength) == -1)
			abort();
		_ais = newais;
	}
	_ais_count += 1;
	struct _allocinfo *ai = &_ais[pos];
	memcpy(ai, like_this, sizeof *like_this);
	ai->allocs_count = 0;
	ai->bytes_used = 0;
	return pos;
}

static void
_fatal(const char *errmsg)
{
	write(2, errmsg, strlen(errmsg));
	write(2, "\n", 1);
	abort();
}

enum { _BUFFER_SIZE = 20 * 1024*1024*1024LL };
static int64_t _buffer_used;
static char *_buffer;

static int
_jumpsize(int size)
{
	return (sizeof(struct _alloc_header) + size + 7) & ~7;
}

static bool _track_backtraces = true;

void *
malloc(size_t size)
{
	if (size > 1024*1024*1024) {
		_fatal("Too big allocation.");
	}
	if (_buffer == NULL) {
		size_t length = _BUFFER_SIZE;
		int prot = PROT_READ | PROT_WRITE;
		int flags = MAP_PRIVATE | MAP_ANONYMOUS;
		_buffer = mmap(NULL, length, prot, flags, -1, 0);
		if (_buffer == MAP_FAILED) {
			_fatal("Memory allocation failed.");
		}
	}
	if (_buffer_used + _jumpsize(size) > (int64_t) _BUFFER_SIZE) {
		_fatal("Buffer too little.");
	}

	struct _alloc_header *hdr;
	hdr = (struct _alloc_header *) &_buffer[_buffer_used];
	hdr->size = size;
	hdr->cap = size;
	_total_bytes += size;
	_buffer_used += _jumpsize(size);
	if (_track_backtraces) {
		_track_backtraces = false;
		struct _allocinfo stack;
		int depth = _MAX_STACK_DEPTH;
		stack.addresses_count = backtrace(stack.addresses, depth);
		hdr->info_id = _allocinfo_get(&stack);
		struct _allocinfo *ai = &_ais[hdr->info_id];
		ai->allocs_count += 1;
		ai->bytes_used += size;
		_track_backtraces = true;
	} else {
		hdr->info_id = -1;
	}
	return hdr + 1;
}

void *
calloc(size_t nmemb, size_t size)
{
	void *p = malloc(nmemb * size);
	memset(p, 0, nmemb * size);
	return p;
}

void *
realloc(void *ptr, size_t size_)
{
	if (ptr == NULL) {
		return malloc(size_);
	}
	if (size_ > 1024*1024*1024/128) {
		_fatal("Too big allocation.");
	}
	int size = size_;
	struct _alloc_header *hdr = ((struct _alloc_header *) ptr)-1;
	if (hdr->cap > size) {
		if (hdr->info_id != -1) {
			struct _allocinfo *ai = &_ais[hdr->info_id];
			ai->bytes_used -= hdr->size;
			ai->bytes_used += size;
		}
		_total_bytes -= hdr->size;
		_total_bytes += size;
		hdr->size = size;
		return ptr;
	}
	void *np = malloc(size * 128);
	struct _alloc_header *nhdr = ((struct _alloc_header *) np)-1;
	nhdr->size = size;
	if (nhdr->info_id != -1) {
		struct _allocinfo *nai = &_ais[nhdr->info_id];
		nai->bytes_used -= size * 127;
	}
	_total_bytes -= size * 127;
	memcpy(np, ptr, hdr->size);
	free(ptr);
	return np;
}

const char *mallinfo_usage(void);

void
free(void *ptr)
{
	if ((intptr_t) ptr == -1) {
		const char *info = mallinfo_usage();
		int flags = O_WRONLY | O_CREAT | O_TRUNC;
		int fd = open("/tmp/alloc.txt", flags, 0666);
		if (fd == -1)
			abort();
		write(fd, info, strlen(info));
		close(fd);
		return;
	}
	if (ptr == NULL)
		return;
	struct _alloc_header *hdr = ((struct _alloc_header *) ptr) - 1;
	_total_bytes -= hdr->size;
	if (hdr->info_id != -1) {
		struct _allocinfo *ai = &_ais[hdr->info_id];
		ai->allocs_count -= 1;
		ai->bytes_used -= hdr->size;
	}

	// Optimization: if free at the end, reclaim that memory.
	const char *bufpos = (const char *) hdr;
	if (bufpos + _jumpsize(hdr->size) == &_buffer[_buffer_used])
		_buffer_used -= _jumpsize(hdr->size);
}

const char *
mallinfo_usage(void)
{
	static char res[64*1024*1024];
	char *p = res, *end = res+sizeof(res);
	p += sprintf(p, "total: %lld bytes\n", (long long) _total_bytes);
	for (int i = 0; i < _ais_count && end-p > 256; ++i) {
		struct _allocinfo *ai = &_ais[i];
		if (ai->bytes_used == 0)
			continue;
		char hdr[200];
		sprintf(hdr, "usage: %lld bytes;", (long long) ai->bytes_used);
		char **trace;
		trace = backtrace_symbols(ai->addresses, ai->addresses_count);
		p = stpcpy(p, hdr);
		for (int i = 0; i < ai->addresses_count; ++i) {
			if ((uintptr_t)(end-p) < strlen(trace[i])+2)
				break;
			p = stpcpy(p, "  ");
			p = stpcpy(p, trace[i]);
		}
		p = stpcpy(p, "\n");
		free(trace);
	}
	*p = 0;
	return res;
}
