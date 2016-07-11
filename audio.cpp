#include "io.h"
#include <string.h>

//force some year-old C code to compile properly as C++ - I decided to switch long ago but still haven't finished.
#define this This

const char * const * audio_supported_backends()
{
	static const char * backends[]={
#ifdef AUDIO_ALSA
		"ALSA",
#endif
#ifdef AUDIO_PULSEAUDIO
		"PulseAudio",
#endif
#ifdef AUDIO_WASAPI
		"WASAPI",
#endif
#ifdef AUDIO_DIRECTSOUND
		"DirectSound",
#endif
		"None",
		NULL
	};
	return backends;
}

static void render(struct caudio * this, unsigned int numframes, const int16_t * samples) {}
static void clear(struct caudio * this) {}
static void set_samplerate(struct caudio * this, double samplerate) {}
static void set_latency(struct caudio * this, double latency) {}
static void set_sync(struct caudio * this, bool sync) {}
static bool has_sync(struct caudio * this) { return false; }
static void free_(struct caudio * this) {}

struct caudio * audio_create_none(uintptr_t windowhandle, double samplerate, double latency)
{
	static struct caudio this={render, clear, set_samplerate, set_latency, set_sync, has_sync, free_};
	return &this;
}

struct caudio * audio_create(const char * backend, uintptr_t windowhandle, double samplerate, double latency)
{
#ifdef AUDIO_ALSA
	if (!strcmp(backend, "ALSA")) return audio_create_alsa(windowhandle, samplerate, latency);
#endif
#ifdef AUDIO_PULSEAUDIO
	if (!strcmp(backend, "PulseAudio")) return audio_create_pulseaudio(windowhandle, samplerate, latency);
#endif
#ifdef AUDIO_DIRECTSOUND
	if (!strcmp(backend, "DirectSound")) return audio_create_directsound(windowhandle, samplerate, latency);
#endif
#ifdef AUDIO_WASAPI
	if (!strcmp(backend, "WASAPI")) return audio_create_wasapi(windowhandle, samplerate, latency);
#endif
	if (!strcmp(backend, "None")) return audio_create_none(windowhandle, samplerate, latency);
	return NULL;
}
