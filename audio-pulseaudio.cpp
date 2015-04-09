#include "io.h"
#ifdef AUDIO_PULSEAUDIO
#include <pulse/pulseaudio.h>
#include <stdlib.h>
#include <string.h>

//this file is heavily based on the implementation in byuu's ruby, written by RedDwarf

//force some year-old C code to compile properly as C++ - I decided to switch long ago but still haven't finished.
#define this This

struct audio_pulse {
	struct caudio i;
	
	pa_mainloop* mainloop;
	pa_context* context;
	pa_stream* stream;
	
	double samplerate;
	double latency;
	bool sync;
	
	bool can_hang;
};

static void render(struct caudio * this_, unsigned int numframes, const int16_t * samples);
static void render_reset(struct caudio * this_, unsigned int numframes, const int16_t * samples);

static void create(struct audio_pulse * this, uintptr_t windowhandle, double samplerate, double latency)
{
	this->mainloop = pa_mainloop_new();
	
	this->context = pa_context_new(pa_mainloop_get_api(this->mainloop), "minir");
	pa_context_connect(this->context, NULL, PA_CONTEXT_NOFLAGS, NULL);
	
	pa_context_state_t cstate;
	do {
		pa_mainloop_iterate(this->mainloop, 1, NULL);
		cstate = pa_context_get_state(this->context);
		if(!PA_CONTEXT_IS_GOOD(cstate)) abort();
	} while(cstate != PA_CONTEXT_READY);
	
	this->stream=NULL;
	
	this->samplerate=samplerate;
	this->latency=latency;
	this->i.render=render_reset;
}

static void reset(struct audio_pulse * this)
{
	if(this->stream) {
		pa_stream_disconnect(this->stream);
		pa_stream_unref(this->stream);
		this->stream = NULL;
	}
	
	pa_sample_spec spec;
	pa_buffer_attr buffer_attr;
	
	spec.format = PA_SAMPLE_S16NE;
	spec.channels = 2;
	spec.rate = this->samplerate;
	this->stream = pa_stream_new(this->context, "audio", &spec, NULL);
	if (!this->stream) return;
	
	buffer_attr.maxlength = -1;
	buffer_attr.tlength = pa_usec_to_bytes(this->latency * PA_USEC_PER_MSEC, &spec);
	buffer_attr.prebuf = -1;
	buffer_attr.minreq = -1;
	buffer_attr.fragsize = -1;
	
	pa_stream_flags_t flags =(pa_stream_flags_t)(PA_STREAM_ADJUST_LATENCY | PA_STREAM_VARIABLE_RATE);
	pa_stream_connect_playback(this->stream, NULL, &buffer_attr, flags, NULL, NULL);
	
	pa_stream_state_t sstate;
	do {
		pa_mainloop_iterate(this->mainloop, 1, NULL);
		sstate = pa_stream_get_state(this->stream);
		if(!PA_STREAM_IS_GOOD(sstate)) abort();
	} while(sstate != PA_STREAM_READY);
	
	this->can_hang=false;
}

static void render(struct caudio * this_, unsigned int numframes, const int16_t * samples)
{
	struct audio_pulse * this=(struct audio_pulse*)this_;
	if (!this->stream) return;
	
	while (pa_mainloop_iterate(this->mainloop, 0, NULL)) {}
	
	unsigned int length;
	while(true)
	{
		length = pa_stream_writable_size(this->stream);
		if (length >= numframes*4) break;
		
		if (!this->sync) break;
		
		pa_mainloop_prepare(this->mainloop, this->can_hang?20:this->latency*1000);
		pa_mainloop_poll(this->mainloop);
		int nevent=pa_mainloop_dispatch(this->mainloop);
		if (!nevent)
		{
			this->can_hang=true;
			break;
		}
		//pa_mainloop_iterate(this->mainloop, 1, NULL);
		while (pa_mainloop_iterate(this->mainloop, 0, NULL)) {}
	}
	if (length>numframes*4) length=numframes*4;
	if (length) pa_stream_write(this->stream, samples, length, NULL, 0LL, PA_SEEK_RELATIVE);
}

static void render_reset(struct caudio * this_, unsigned int numframes, const int16_t * samples)
{
	struct audio_pulse * this=(struct audio_pulse*)this_;
	reset(this);
	this->i.render=render;
	render(this_, numframes, samples);
}

static void clear(struct caudio * this)
{
	//not needed; underruns are handled with whitespace
}

static void set_samplerate(struct caudio * this_, double samplerate)
{
	struct audio_pulse * this=(struct audio_pulse*)this_;
	
	this->samplerate=samplerate;
	this->i.render=render_reset;
}

static void set_latency(struct caudio * this_, double latency)
{
	struct audio_pulse * this=(struct audio_pulse*)this_;
	
	this->latency=latency;
	this->i.render=render_reset;
}

static void set_sync(struct caudio * this_, bool sync)
{
	struct audio_pulse * this=(struct audio_pulse*)this_;
	this->sync=sync;
}

static bool has_sync(struct caudio * this_)
{
	return true;
}

static void free_(struct caudio * this_)
{
	struct audio_pulse * this=(struct audio_pulse*)this_;
	
	if(this->stream) {
		pa_stream_disconnect(this->stream);
		pa_stream_unref(this->stream);
		this->stream = NULL;
	}
	
	if(this->context) {
		pa_context_disconnect(this->context);
		pa_context_unref(this->context);
		this->context = NULL;
	}
	
	if(this->mainloop) {
		pa_mainloop_free(this->mainloop);
		this->mainloop = NULL;
	}
	
	free(this);
}

struct caudio * audio_create_pulseaudio(uintptr_t windowhandle, double samplerate, double latency)
{
	struct audio_pulse * this=malloc(sizeof(struct audio_pulse));
	this->i.render=render;
	this->i.clear=clear;
	this->i.set_samplerate=set_samplerate;
	this->i.set_latency=set_latency;
	this->i.set_sync=set_sync;
	this->i.has_sync=has_sync;
	this->i.free=free_;
	this->sync=true;
	create(this, windowhandle, samplerate, latency);
	return (struct caudio*)this;
}
#endif
