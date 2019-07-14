#include "headers.h"

enum { MAX_AUDIO_FILES = 64 };
enum { SAMPLES_PER_SEC = 22050 };
enum { BYTES_PER_SAMPLE = 2 };
enum { BUFFER_SIZE = 60 * SAMPLES_PER_SEC * BYTES_PER_SAMPLE };
enum { AL_BUFFERS = 32 };
enum { AL_FORMAT = AL_FORMAT_MONO16 };

static struct {
	char data[BUFFER_SIZE];
	struct {
		int samples;
		int first_sample;
	} files[MAX_AUDIO_FILES];
	ALCdevice *device;
	ALCcontext *context;
	ALuint buffers[AL_BUFFERS], sources[AL_BUFFERS];
	bool used[AL_BUFFERS];
	int next_free;
}
_v;

static void
_auto_stop(void)
{
	alDeleteBuffers(AL_BUFFERS, _v.buffers);
	alDeleteSources(AL_BUFFERS, _v.sources);
	CHECK(alcMakeContextCurrent(NULL));
	alcDestroyContext(_v.context);
	alcCloseDevice(_v.device);
	_v.context = NULL;
	_v.device = NULL;
}

void
audio_init(void)
{
	puts("Initializing audio.");
	puts("\tLoading assets/sounds.raw");
	int buffer_length;
	const char *buf = util_load_file("assets/sounds.raw", &buffer_length);
	CHECK(buffer_length <= BUFFER_SIZE);
	memcpy(_v.data, buf, buffer_length);

	static const char snd2file[SND_LAST][MAXSTR] = {
		[SND_BREATHING] = "breathing.wav",
		[SND_GUN_BERETTA_EMPTY] = "gun_beretta_empty.wav",
		[SND_GUN_BERETTA_FIRE] = "gun_beretta_fire.wav",
		[SND_GUN_BERETTA_RELOAD] = "gun_beretta_reload.wav",
		[SND_GUN_BERETTA_RELOAD_EMPTY] = "gun_beretta_reload_empty.wav",
		[SND_GUN_CLOSE] = "gun_close.wav",
		[SND_GUN_LOAD] = "gun_load.wav",
		[SND_GUNSHOT_DRY] = "gunshot_dry.wav",
		[SND_GUNSHOT] = "gunshot.wav",
	};

	puts("\tLoading assets/sounds.desc.");
	FILE *f = fopen("assets/sounds.desc", "r");
	CHECK(f != NULL);
	int all_samples = 0;
	char name[60];
	int samples;
	for (int i = 0; fscanf(f, "%50s %d", name, &samples) == 2; i++) {
		CHECK(i < MAX_AUDIO_FILES);
		enum snd_id id;
		for (id = 0; id != SND_LAST; id++) {
			if (strcmp(name, snd2file[id]) == 0) {
				break;
			}
		}
		if (id == SND_LAST) {
			printf("Error: unknown sound %s.\n", name);
			exit(1);
		}
		_v.files[id].samples = samples;
		_v.files[id].first_sample = all_samples;
		all_samples += samples;
		CHECK(all_samples*BYTES_PER_SAMPLE <= buffer_length);
	}
	CHECK(fclose(f) == 0);

	puts("\tInitializing OpenAL.");
	_v.device = alcOpenDevice(NULL);
	CHECK(_v.device != NULL);
	_v.context = alcCreateContext(_v.device, NULL);
	CHECK(_v.context != NULL);
	CHECK(alcMakeContextCurrent(_v.context));
	alGenBuffers(AL_BUFFERS, _v.buffers);
	alGenSources(AL_BUFFERS, _v.sources);
	CHECK(alGetError() == ALC_NO_ERROR);
	onexit(_auto_stop);
}

void
audio_stop(void)
{
}

void
audio_tick(void)
{
	// Set up the listener coords.
	alListenerfv(AL_POSITION, g.camera.pos.v);
	float orientation[6] = {
		cos(g.camera.zrot), sin(g.camera.zrot), 0.0f,
		0.0f, 0.0f, 1.0f,
	};
	alListenerfv(AL_ORIENTATION, orientation);

	// Reclaim free buffers.
	int used_count = 0;
	for (int i = 0; i < AL_BUFFERS; i++) {
		if (!_v.used[i]) {
			continue;
		}
		ALuint buffer;
		ALint processed;
		alGetSourcei(_v.sources[i], AL_BUFFERS_PROCESSED, &processed);
		if (processed <= 0) {
			used_count += 1;
			continue;
		}
		alSourceUnqueueBuffers(_v.sources[i], 1, &buffer);
		CHECK(buffer == _v.buffers[i]);
		_v.used[i] = false;
	}
	_v.next_free = 0;
	const char *text = qprintf("sounds: %d", used_count);
	int xoff = window_width - strlen(text)*font_x;
	font_draw(xoff, 2*font_y, &color_black, text);
	CHECK(alGetError() == ALC_NO_ERROR);
}

void
audio_play(enum snd_id id)
{
	audio_play_at(id, NULL);
}

void audio_play_at(enum snd_id id, const vector_t *v)
{
	CHECK(0 <= id && id < SND_LAST);
	int bid;
	for (bid = _v.next_free; bid < AL_BUFFERS && _v.used[bid]; bid++)
		;
	_v.next_free = bid;
	CHECK(bid != AL_BUFFERS);

	_v.used[bid] = true;
	char *data = _v.data + _v.files[id].first_sample*BYTES_PER_SAMPLE;
	int len = _v.files[id].samples*BYTES_PER_SAMPLE;
	alBufferData(_v.buffers[bid], AL_FORMAT, data, len, SAMPLES_PER_SEC);
	alSourceQueueBuffers(_v.sources[bid], 1, &_v.buffers[bid]);
	if (v == NULL) {
		alSourcei(_v.sources[bid], AL_SOURCE_RELATIVE, AL_TRUE);
		alSourcefv(_v.sources[bid], AL_POSITION, v_zero.v);
	} else {
		alSourcei(_v.sources[bid], AL_SOURCE_RELATIVE, AL_FALSE);
		alSourcefv(_v.sources[bid], AL_POSITION, v->v);
	}
	alSourcePlay(_v.sources[bid]);
}

int audio_length(enum snd_id id)
{
	CHECK(0 <= id && id < SND_LAST);
	return _v.files[id].samples*g.fps / 22050;
}
