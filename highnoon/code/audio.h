#ifndef audio_h
#define audio_h

void audio_init(void);
void audio_destroy(void);

// Get the id for a sound. Pass this to audio_play. This might be a slow
// operation, do it at initialization times.
int audio_getid(const char *name);

// Mix the sound id into the currently playing stream.
void audio_play(int id);

#endif
