#include "minir.h"
#ifdef AUDIO_DIRECTSOUND
#define CINTERFACE
#include <dsound.h>

//this file is heavily based on ruby by byuu

const GUID GUID_NULL={0};

struct audio_directsound {
	struct audio i;
	
	LPDIRECTSOUND ds;
	LPDIRECTSOUNDBUFFER dsb_p;
	LPDIRECTSOUNDBUFFER dsb_b;
	
	DWORD lastwrite;
	
	DWORD bufsize;
	
	bool sync;
	double samplerate;//only needed for changing latency, but they can be changed separately and in any order
};

static void createbuf(struct audio_directsound * this, double samplerate, double latency);

static void render(struct audio * this_, unsigned int numframes, const int16_t * samples)
{
	struct audio_directsound * this=(struct audio_directsound*)this_;
	
	DWORD freeend;
	DWORD freestart;
	DWORD thispos=this->lastwrite;
	
wait:
	this->dsb_b->lpVtbl->GetCurrentPosition(this->dsb_b, &freeend, &freestart);
	freestart/=4; freeend/=4;
	if (thispos<freestart) thispos+=this->bufsize;
	if (freeend<thispos) freeend+=this->bufsize;
	
	if (thispos+numframes>freeend)
	{
		if (this->sync) { Sleep(1); goto wait; }
		else numframes=freeend-thispos;
	}
	
	void* data1; DWORD data1bytes;
	void* data2; DWORD data2bytes;
	if (this->dsb_b->lpVtbl->Lock(this->dsb_b, thispos%this->bufsize*4, numframes*4, &data1, &data1bytes, &data2, &data2bytes, 0) == DS_OK)
	{
		memcpy(data1, samples, data1bytes);
		memcpy(data2, samples+data1bytes/4*2, data2bytes);
		this->dsb_b->lpVtbl->Unlock(this->dsb_b, data1, data1bytes, data2, data2bytes);
		this->lastwrite=(thispos+(data1bytes+data2bytes)/4) % this->bufsize;
	}
}

static void clear(struct audio * this_)
{
	struct audio_directsound * this=(struct audio_directsound*)this_;
	
	if (!this->dsb_b) return;
	this->dsb_b->lpVtbl->Stop(this->dsb_b);
	this->dsb_b->lpVtbl->SetCurrentPosition(this->dsb_b, 0);
	
	DWORD size;
	void* buffer;
	this->dsb_b->lpVtbl->Lock(this->dsb_b, 0, -1, &buffer, &size, 0, 0, DSBLOCK_ENTIREBUFFER);
	memset(buffer, 0, size);
	this->dsb_b->lpVtbl->Unlock(this->dsb_b, buffer, size, 0, 0);
	
	this->dsb_b->lpVtbl->Play(this->dsb_b, 0, 0, DSBPLAY_LOOPING);
	
	this->lastwrite=0;
}

static void set_samplerate(struct audio * this_, double samplerate)
{
	struct audio_directsound * this=(struct audio_directsound*)this_;
	this->samplerate=samplerate;
	this->dsb_b->lpVtbl->SetFrequency(this->dsb_b, samplerate);
}

static void set_latency(struct audio * this_, double latency)
{
	struct audio_directsound * this=(struct audio_directsound*)this_;
	
	if (this->dsb_b) this->dsb_b->lpVtbl->Release(this->dsb_b);
	
	createbuf(this, this->samplerate, latency);
}

static void set_sync(struct audio * this_, bool sync)
{
	struct audio_directsound * this=(struct audio_directsound*)this_;
	this->sync=sync;
}

static bool has_sync(struct audio * this_)
{
	return true;
}

static void free_(struct audio * this_)
{
	struct audio_directsound * this=(struct audio_directsound*)this_;
	
	if (this->dsb_b) { this->dsb_b->lpVtbl->Stop(this->dsb_b); this->dsb_b->lpVtbl->Release(this->dsb_b); }
	if (this->dsb_p) { this->dsb_p->lpVtbl->Stop(this->dsb_p); this->dsb_p->lpVtbl->Release(this->dsb_p); }
	if (this->ds) { this->ds->lpVtbl->Release(this->ds); }
	
	free(this);
}

struct audio * audio_create_directsound(uintptr_t windowhandle, double samplerate, double latency)
{
	struct audio_directsound * this=malloc(sizeof(struct audio_directsound));
	this->i.render=render;
	this->i.clear=clear;
	this->i.set_samplerate=set_samplerate;
	this->i.set_latency=set_latency;
	this->i.set_sync=set_sync;
	this->i.has_sync=has_sync;
	this->i.free=free_;
	this->sync=true;
	
	this->samplerate=samplerate;
	
	DirectSoundCreate(0, &this->ds, 0);
	if (!this->ds)
	{
		free(this);
		return NULL;
	}
	this->ds->lpVtbl->SetCooperativeLevel(this->ds, (HWND)windowhandle, DSSCL_PRIORITY);
	
	this->bufsize=samplerate*latency/1000.0+0.5;
	
	DSBUFFERDESC dsbd;
	memset(&dsbd, 0, sizeof(dsbd));
	dsbd.dwSize        = sizeof(dsbd);
	dsbd.dwFlags       = DSBCAPS_PRIMARYBUFFER;
	dsbd.dwBufferBytes = 0;
	dsbd.lpwfxFormat   = 0;
	this->ds->lpVtbl->CreateSoundBuffer(this->ds, &dsbd, &this->dsb_p, 0);
	
	createbuf(this, samplerate, latency);
	
	return (struct audio*)this;
}

static void createbuf(struct audio_directsound * this, double samplerate, double latency)
{
	this->bufsize=this->samplerate*latency/1000.0+0.5;
	
	WAVEFORMATEX wfx;
	memset(&wfx, 0, sizeof(wfx));
	wfx.wFormatTag      = WAVE_FORMAT_PCM;
	wfx.nChannels       = 2;
	wfx.nSamplesPerSec  = this->samplerate;
	wfx.wBitsPerSample  = 16;
	wfx.nBlockAlign     = wfx.wBitsPerSample / 8 * wfx.nChannels;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	
	DSBUFFERDESC dsbd;
	memset(&dsbd, 0, sizeof(dsbd));
	dsbd.dwSize  = sizeof(dsbd);
	dsbd.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLFREQUENCY | DSBCAPS_GLOBALFOCUS | DSBCAPS_LOCSOFTWARE;
	dsbd.dwBufferBytes   = this->bufsize * sizeof(uint32_t);
	dsbd.guid3DAlgorithm = GUID_NULL;
	dsbd.lpwfxFormat     = &wfx;
	this->ds->lpVtbl->CreateSoundBuffer(this->ds, &dsbd, &this->dsb_b, 0);
	this->dsb_b->lpVtbl->SetFrequency(this->dsb_b, this->samplerate);
	this->dsb_b->lpVtbl->SetCurrentPosition(this->dsb_b, 0);
	
	clear((struct audio*)this);
}
#endif
