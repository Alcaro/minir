#include "io.h"
#ifdef AUDIO_ALSA
#include <alsa/asoundlib.h>

//this file is heavily based on the implementation in byuu's ruby, written by BearOso, byuu, Nach, RedDwarf

//force some year-old C code to compile properly as C++ - I decided to switch long ago but still haven't finished.
#define this This

struct audio_alsa {
	struct caudio i;
	
	snd_pcm_t* handle;
	snd_pcm_uframes_t buffer;
	snd_pcm_uframes_t period;
	
	unsigned int samplerate;
	unsigned int latency;
	bool sync;
};

static void render(struct caudio * this_, unsigned int numframes, const int16_t * samples);
static void render_reset(struct caudio * this_, unsigned int numframes, const int16_t * samples);

static bool reset(struct audio_alsa * this);

static bool create(struct audio_alsa * this, uintptr_t windowhandle, double samplerate, double latency)
{
	this->handle = NULL;
	
	if (snd_pcm_open(&this->handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0) return false;
	
	this->samplerate=samplerate;
	this->latency=latency;
	this->i.render=render_reset;
	
	return reset(this);
}

static void cleanup(struct audio_alsa * this)
{
	if (this->handle)
	{
		//snd_pcm_drain(device.handle);  //prevents popping noise; but causes multi-second lag
		snd_pcm_close(this->handle);
	}
	this->handle = NULL;
}

static bool reset(struct audio_alsa * this)
{
	//below code will not work with 24khz frequency rate (ALSA library bug)
#if 0
	if(snd_pcm_set_params(this->handle, device.format, SND_PCM_ACCESS_RW_INTERLEAVED,
		device.channels, settings.frequency, 1, settings.latency * 1000) < 0) {
		//failed to set device parameters
		return false;
	}
	
	if(snd_pcm_get_params(this->handle, &device.buffer_size, &device.period_size) < 0) {
		device.period_size = settings.latency * 1000 * 1e-6 * settings.frequency / 4;
	}
#endif
	
	snd_pcm_hw_params_t* hwparams;
	snd_pcm_hw_params_alloca(&hwparams);
	if (snd_pcm_hw_params_any(this->handle, hwparams) < 0) return false;
	
	if (snd_pcm_hw_params_set_access(this->handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) return false;
	if (snd_pcm_hw_params_set_format(this->handle, hwparams, SND_PCM_FORMAT_S16) < 0) return false;
	if (snd_pcm_hw_params_set_channels(this->handle, hwparams, 2) < 0) return false;
	if (snd_pcm_hw_params_set_rate_near(this->handle, hwparams, &this->samplerate, 0) < 0) return false;
	unsigned int buffer = this->latency * 1000;
	if (snd_pcm_hw_params_set_buffer_time_near(this->handle, hwparams, &buffer, 0) < 0) return false;
	unsigned int period = this->latency * 1000 / 4;
	if (snd_pcm_hw_params_set_period_time_near(this->handle, hwparams, &period, 0) < 0) return false;
	
	if (snd_pcm_hw_params(this->handle, hwparams) < 0) return false;
	
	if (snd_pcm_get_params(this->handle, &this->buffer, &this->period) < 0) return false;
	
	snd_pcm_sw_params_t* swparams;
	snd_pcm_sw_params_alloca(&swparams);
	if (snd_pcm_sw_params_current(this->handle, swparams) < 0) return false;
	
	snd_pcm_uframes_t start = (this->buffer / this->period) * this->period;
	if (snd_pcm_sw_params_set_start_threshold(this->handle, swparams, start) < 0) return false;
	
	if (snd_pcm_sw_params(this->handle, swparams) < 0) return false;
	
	this->i.render=render;
	
	return true;
}

static void render(struct caudio * this_, unsigned int numframes, const int16_t * samples)
{
	struct audio_alsa * this=(struct audio_alsa*)this_;
	
	while (numframes)
	{
		snd_pcm_sframes_t avail = snd_pcm_avail_update(this->handle);
		if (avail < 0)
		{
			snd_pcm_recover(this->handle, avail, true);
			continue;
		}
		if (avail < numframes)
		{
			int error = snd_pcm_wait(this->handle, -1);
			if (error < 0)
			{
				snd_pcm_recover(this->handle, error, true);
			}
		}
		
		snd_pcm_uframes_t towrite = avail;
		if (numframes < avail) towrite = numframes;
		snd_pcm_sframes_t written = snd_pcm_writei(this->handle, samples, towrite);
		if (written < 0)
		{
			snd_pcm_recover(this->handle, written, true);
		}
		else
		{
			numframes -= written;
			samples += 2*written;
		}
		
		if (!this->sync) break;
	}
}

static void render_reset(struct caudio * this_, unsigned int numframes, const int16_t * samples)
{
	struct audio_alsa * this=(struct audio_alsa*)this_;
	cleanup(this);
	reset(this);
	render(this_, numframes, samples);
}

static void clear(struct caudio * this)
{
	//not needed; underruns are handled with whitespace
}

static void set_samplerate(struct caudio * this_, double samplerate)
{
	struct audio_alsa * this=(struct audio_alsa*)this_;
	
	this->samplerate=samplerate;
	this->i.render=render_reset;
}

static void set_latency(struct caudio * this_, double latency)
{
	struct audio_alsa * this=(struct audio_alsa*)this_;
	
	this->latency=latency;
	this->i.render=render_reset;
}

static void set_sync(struct caudio * this_, bool sync)
{
	struct audio_alsa * this=(struct audio_alsa*)this_;
	this->sync=sync;
}

static bool has_sync(struct caudio * this_)
{
	return true;
}

static void free_(struct caudio * this_)
{
	struct audio_alsa * this=(struct audio_alsa*)this_;
	cleanup(this);
	free(this);
}

struct caudio * audio_create_alsa(uintptr_t windowhandle, double samplerate, double latency)
{
	struct audio_alsa * this=malloc(sizeof(struct audio_alsa));
	this->i.render=render;
	this->i.clear=clear;
	this->i.set_samplerate=set_samplerate;
	this->i.set_latency=set_latency;
	this->i.set_sync=set_sync;
	this->i.has_sync=has_sync;
	this->i.free=free_;
	this->sync=true;
	if (!create(this, windowhandle, samplerate, latency))
	{
		free_((struct caudio*)this);
		return NULL;
	}
	return (struct caudio*)this;
}
#endif
