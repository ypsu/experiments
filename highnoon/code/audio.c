#include "headers.h"

// _mixaudio() is running on a different thread so we need to be careful in the
// inter-thread communication
#define _VOLATILE(x) (*(volatile int*)&(x))

static int _all_sounds_length;
static short *_all_sounds;

enum { _MAX_SOUNDS = 64 };
static int _sounds_count;
static struct {
	// These are indexes into the _all_sounds array.
	int begin, end;
	char name[MAXSTR];
} _sounds_desc[_MAX_SOUNDS];

enum { _STREAM_COUNT = 64 };
static struct {
	// These are indexes into the _all_sounds array. If begin==-1 then this
	// stream is unused and thus it is available for playing sounds.
	int begin, end;
} _streams[_STREAM_COUNT];

static void _mixaudio(void *unused, unsigned char *data, int len)
{
	(void) unused;
	assert(len%2 == 0);

	// Mix each stream into SDL's buffer
	for (int s = 0; s < _STREAM_COUNT; ++s) {
		if (_streams[s].begin == -1)
			continue;
		if (_streams[s].begin == _streams[s].end) {
			// Stream fully played, mark it as available
			_VOLATILE(_streams[s].begin) = -1;
			continue;
		}
		unsigned char *buf = (void*) &_all_sounds[_streams[s].begin];
		int amount = _streams[s].end - _streams[s].begin;
		amount *= sizeof _all_sounds[0];
		if (amount > len)
			amount = len;
		_streams[s].begin += amount/2;
		SDL_MixAudio(data, buf, amount, SDL_MIX_MAXVOLUME);
	}
}

void audio_init(void)
{
	FILE *f;

	puts("Initializing the audio subsystem");
	memset(_streams, -1, sizeof _streams);

	puts("\tInitializing SDL audio");
	HANDLE_CASE(SDL_Init(SDL_INIT_AUDIO) == -1);
	SDL_AudioSpec fmt;
	fmt.freq = 22050;
	fmt.format = AUDIO_S16;
	fmt.channels = 1;
	fmt.samples = 512;
	fmt.callback = _mixaudio;
	fmt.userdata = NULL;
	HANDLE_CASE(SDL_OpenAudio(&fmt, NULL) == -1);
	SDL_PauseAudio(0);

	puts("\tLoading sounds.raw");
	f = fopen("sounds.raw", "r");
	HANDLE_CASE(f == NULL);
	HANDLE_CASE(fseek(f, 0, SEEK_END) != 0);
	int filesize = ftell(f);
	HANDLE_CASE(filesize <= 0);
	HANDLE_CASE(fseek(f, 0, SEEK_SET) != 0);
	_all_sounds = malloc(filesize);
	HANDLE_CASE(fread(_all_sounds, filesize, 1, f) != 1);
	HANDLE_CASE(fclose(f) != 0);
	_all_sounds_length = filesize / 2;

	puts("\tLoading sounds.desc");
	// sounds.desc consists of two entries per line:
	// <name of the sound> <the number of samples it has>
	f = fopen("sounds.desc", "r");
	HANDLE_CASE(f == NULL);
	char name[MAXSTR];
	int begin = 0, length;
	while (fscanf(f, "%60s %d", name, &length) == 2) {
		HANDLE_CASE(_sounds_count >= _MAX_SOUNDS);
		int idx = _sounds_count++;
		_sounds_desc[idx].begin = begin;
		begin += length;
		_sounds_desc[idx].end = begin;
		strcpy(_sounds_desc[idx].name, name);
		HANDLE_CASE(begin > _all_sounds_length);
	}
	HANDLE_CASE(fclose(f) != 0);
}

void audio_destroy(void)
{
	SDL_CloseAudio();
}

int audio_getid(const char *name)
{
	for (int i = 0; i < _sounds_count; ++i) {
		if (strcmp(name, _sounds_desc[i].name) == 0)
			return i;
	}
	HANDLE_CASE(false);
	return 0;
}

void audio_play(int id)
{
	assert(id < _sounds_count);

	// Find a free stream
	int free_stream;
	for (free_stream = 0; free_stream < _STREAM_COUNT; ++free_stream) {
		if (_VOLATILE(_streams[free_stream].begin) == -1)
			break;
	}
	HANDLE_CASE(free_stream == _STREAM_COUNT);

	// Play on the free stream
	_VOLATILE(_streams[free_stream].end) = _sounds_desc[id].end;
	_VOLATILE(_streams[free_stream].begin) = _sounds_desc[id].begin;
}
