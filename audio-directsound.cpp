#define CINTERFACE
#include "io.h"
#ifdef AUDIO_DIRECTSOUND
#undef bind
#define interface struct
#include <mmreg.h> // dsound.h demands this, why is it not included
#include <mmsystem.h> // dsound.h demands this too
#include <dsound.h>
#undef interface
#define bind bind_func

//this file is heavily based on ruby by byuu

//force some year-old C code to compile properly as C++ - I decided to switch long ago but still haven't finished.
#define this This

static HMODULE hDSound=NULL;
static HRESULT (WINAPI * lpDirectSoundCreate)(LPCGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter);

static bool libLoad();
static void libRelease();

const GUID GUID_NULL={0};

struct audio_directsound {
	struct caudio i;
	
	LPDIRECTSOUND ds;
	LPDIRECTSOUNDBUFFER dsb_p;
	LPDIRECTSOUNDBUFFER dsb_b;
	
	DWORD lastwrite;
	
	DWORD bufsize;
	
	bool sync;
	double samplerate;//only needed for changing latency, but they can be changed separately and in any order
};

static void createbuf(struct audio_directsound * this, double samplerate, double latency);

static void render(struct caudio * this_, unsigned int numframes, const int16_t * samples)
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

static void clear(struct caudio * this_)
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

static void set_samplerate(struct caudio * this_, double samplerate)
{
	struct audio_directsound * this=(struct audio_directsound*)this_;
	this->samplerate=samplerate;
	this->dsb_b->lpVtbl->SetFrequency(this->dsb_b, samplerate);
}

static void set_latency(struct caudio * this_, double latency)
{
	struct audio_directsound * this=(struct audio_directsound*)this_;
	
	if (this->dsb_b) this->dsb_b->lpVtbl->Release(this->dsb_b);
	
	createbuf(this, this->samplerate, latency);
}

static void set_sync(struct caudio * this_, bool sync)
{
	struct audio_directsound * this=(struct audio_directsound*)this_;
	this->sync=sync;
}

static bool has_sync(struct caudio * this_)
{
	return true;
}

static void free_(struct caudio * this_)
{
	struct audio_directsound * this=(struct audio_directsound*)this_;
	
	if (this->dsb_b) { this->dsb_b->lpVtbl->Stop(this->dsb_b); this->dsb_b->lpVtbl->Release(this->dsb_b); }
	if (this->dsb_p) { this->dsb_p->lpVtbl->Stop(this->dsb_p); this->dsb_p->lpVtbl->Release(this->dsb_p); }
	if (this->ds) { this->ds->lpVtbl->Release(this->ds); }
	
	libRelease();
	
	free(this);
}

static bool libLoad()
{
	hDSound=LoadLibrary("dsound.dll");
	if (!hDSound) return false;
	//lpDirectSoundCreate=DirectSoundCreate;//this is for type checking; it's not needed anymore
	lpDirectSoundCreate=(HRESULT(WINAPI*)(LPCGUID,LPDIRECTSOUND*,LPUNKNOWN))GetProcAddress(hDSound, "DirectSoundCreate");
	if (!lpDirectSoundCreate) { FreeLibrary(hDSound); return false; }
	return true;
}

static void libRelease()
{
	FreeLibrary(hDSound);
}

struct caudio * audio_create_directsound(uintptr_t windowhandle, double samplerate, double latency)
{
	if (!libLoad()) return NULL;
	
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
	
	lpDirectSoundCreate(NULL, &this->ds, NULL);
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
	
	return (struct caudio*)this;
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
	
	clear((struct caudio*)this);
}
#endif
