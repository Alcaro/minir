#include "minir.h"
#ifdef THREAD_WIN32
#include <windows.h>
#include <stdlib.h>
#include <string.h>

//list of synchronization points: http://msdn.microsoft.com/en-us/library/windows/desktop/ms686355%28v=vs.85%29.aspx

struct threaddata_win32 {
	void(*startpos)(void* userdata);
	void* userdata;
};
static DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
	struct threaddata_win32 * thdat=lpParameter;
	thdat->startpos(thdat->userdata);
	free(thdat);
	return 0;
}

void thread_create(void(*startpos)(void* userdata), void* userdata)
{
	struct threaddata_win32 * thdat=malloc(sizeof(struct threaddata_win32));
	thdat->startpos=startpos;
	thdat->userdata=userdata;
	
	//CreateThread is not listed as a synchronization point; it probably is, but I'd rather use a pointless
	// operation than risk a really annoying bug. It's lightweight compared to creating a thread, anyways.
	
	//MemoryBarrier();//gcc lacks this, and msvc lacks the gcc builtin I could use instead.
	//And of course my gcc supports only ten out of the 137 InterlockedXxx functions. Let's pick the simplest one...
	LONG ignored=0;
	InterlockedIncrement(&ignored);
	
	CloseHandle(CreateThread(NULL, 0, ThreadProc, thdat, 0, NULL));
}


struct mutex_win32 {
	struct mutex i;
	
	CRITICAL_SECTION cs;
};

static void mutex_lock(struct mutex * this_)
{
	struct mutex_win32 * this=(struct mutex_win32*)this_;
	EnterCriticalSection(&this->cs);
}

static bool mutex_try_lock(struct mutex * this_)
{
	struct mutex_win32 * this=(struct mutex_win32*)this_;
	return TryEnterCriticalSection(&this->cs);
}

static void mutex_unlock(struct mutex * this_)
{
	struct mutex_win32 * this=(struct mutex_win32*)this_;
	LeaveCriticalSection(&this->cs); 
}

static void mutex_free_(struct mutex * this_)
{
	struct mutex_win32 * this=(struct mutex_win32*)this_;
	DeleteCriticalSection(&this->cs);
	free(this);
}

const struct mutex mutex_win32_base = {
	mutex_lock, mutex_try_lock, mutex_unlock, mutex_free_
};
struct mutex * mutex_create()
{
	struct mutex_win32 * this=malloc(sizeof(struct mutex_win32));
	memcpy(&this->i, &mutex_win32_base, sizeof(struct mutex));
	
	InitializeCriticalSection(&this->cs);
	
	return (struct mutex*)this;
}


struct event_win32 {
	struct event i;
	
	HANDLE h;
};

static void event_signal(struct event * this_)
{
	struct event_win32 * this=(struct event_win32*)this_;
	SetEvent(this->h);
}

static void event_wait(struct event * this_)
{
	struct event_win32 * this=(struct event_win32*)this_;
	WaitForSingleObject(this->h, INFINITE);
}

static void event_free_(struct event * this_)
{
	struct event_win32 * this=(struct event_win32*)this_;
	CloseHandle(this->h);
	free(this);
}

struct event * event_create()
{
	struct event_win32 * this=malloc(sizeof(struct event_win32));
	this->i.signal=event_signal;
	this->i.wait=event_wait;
	this->i.free=event_free_;
	
	this->h=CreateEvent(NULL, FALSE, FALSE, NULL);
	
	return (struct event*)this;
}
#endif
