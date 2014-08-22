#include "minir.h"
#include <string.h>

const char * const * audio_supported_backends()
{
	static const char * backends[]={
#ifdef AUDIO_PULSEAUDIO
		"PulseAudio",
#endif
#ifdef AUDIO_DIRECTSOUND
		"DirectSound",
#endif
		"None",
		NULL
	};
	return backends;
}

static void render(struct audio * this, unsigned int numframes, const int16_t * samples) {}
static void clear(struct audio * this) {}
static void set_samplerate(struct audio * this, double samplerate) {}
static void set_latency(struct audio * this, double latency) {}
static void set_sync(struct audio * this, bool sync) {}
static bool has_sync(struct audio * this) { return false; }
static void free_(struct audio * this) {}

struct audio * audio_create_none(uintptr_t windowhandle, double samplerate, double latency)
{
	static struct audio this={render, clear, set_samplerate, set_latency, set_sync, has_sync, free_};
	return &this;
}

struct audio * audio_create(const char * backend, uintptr_t windowhandle, double samplerate, double latency)
{
#ifdef AUDIO_PULSEAUDIO
	if (!strcmp(backend, "PulseAudio")) return audio_create_pulseaudio(windowhandle, samplerate, latency);
#endif
#ifdef AUDIO_DIRECTSOUND
	if (!strcmp(backend, "DirectSound")) return audio_create_directsound(windowhandle, samplerate, latency);
#endif
	if (!strcmp(backend, "None")) return audio_create_none(windowhandle, samplerate, latency);
	return NULL;
}
