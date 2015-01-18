#include "os.h"
#ifdef THREAD_WIN32
#undef bind
#include <windows.h>
#define bind bind_func
#include <stdlib.h>
#include <string.h>

//list of synchronization points: http://msdn.microsoft.com/en-us/library/windows/desktop/ms686355%28v=vs.85%29.aspx

struct threaddata_win32 {
	function<void()> func;
};
static DWORD WINAPI ThreadProc(LPVOID lpParameter)
{
	struct threaddata_win32 * thdat=(struct threaddata_win32*)lpParameter;
	thdat->func();
	free(thdat);
	return 0;
}
void thread_create(function<void()> func)
{
	struct threaddata_win32 * thdat=malloc(sizeof(struct threaddata_win32));
	thdat->func=func;
	
	//CreateThread is not listed as a synchronization point; it probably is, but I'd rather use a pointless
	// operation than risk a really annoying bug. It's lightweight compared to creating a thread, anyways.
	
	//MemoryBarrier();//gcc lacks this, and msvc lacks the gcc builtin I could use instead.
	//And of course my gcc supports only ten out of the 137 InterlockedXxx functions. Let's pick the simplest one...
	LONG ignored=0;
	InterlockedIncrement(&ignored);
	
	HANDLE h=CreateThread(NULL, 0, ThreadProc, thdat, 0, NULL);
	if (!h) abort();
	CloseHandle(h);
}

unsigned int thread_ideal_count()
{
	SYSTEM_INFO sysinf;
	GetSystemInfo(&sysinf);
	return sysinf.dwNumberOfProcessors;
}



mutex* mutex::create()
{
	CRITICAL_SECTION* cs=malloc(sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(cs);
	return (mutex*)cs;
}

void mutex::lock() { EnterCriticalSection((CRITICAL_SECTION*)this); }
bool mutex::try_lock() { return TryEnterCriticalSection((CRITICAL_SECTION*)this); }
void mutex::unlock() { LeaveCriticalSection((CRITICAL_SECTION*)this); }

void mutex::release()
{
	DeleteCriticalSection((CRITICAL_SECTION*)this);
	free(this);
}


event::event() { data=(void*)CreateEvent(NULL, false, false, NULL); }
void event::signal() { SetEvent((HANDLE)this->data); }
void event::wait() { WaitForSingleObject((HANDLE)this->data, INFINITE); }
bool event::signalled() { if (WaitForSingleObject((HANDLE)this->data, 0)==WAIT_OBJECT_0) { SetEvent((HANDLE)this->data); return true; } else return false; }
event::~event() { CloseHandle((HANDLE)this->data); }


multievent::multievent()
{
	this->data=(void*)CreateSemaphore(NULL, 0, 127, NULL);
	this->n_count=0;
}

void multievent::signal(unsigned int count)
{
	InterlockedExchangeAdd((volatile LONG*)&this->n_count, count);
	ReleaseSemaphore((HANDLE)this->data, count, NULL);
}

void multievent::wait(unsigned int count)
{
	InterlockedExchangeAdd((volatile LONG*)&this->n_count, -(LONG)count);
	while (count)
	{
		WaitForSingleObject((HANDLE)this->data, INFINITE);
		count--;
	}
}

signed int multievent::count()
{
	return InterlockedCompareExchange((volatile LONG*)&this->n_count, 0, 0);
}

multievent::~multievent() { CloseHandle((HANDLE)this->data); }


uint32_t lock_incr(uint32_t * val) { return InterlockedIncrement((LONG*)val)-1; }
uint32_t lock_decr(uint32_t * val) { return InterlockedDecrement((LONG*)val)+1; }
uint32_t lock_read(uint32_t * val) { return InterlockedCompareExchange((LONG*)val, 0, 0); }

void* lock_read(void* * val) { return InterlockedCompareExchangePointer(val, 0, 0); }
void lock_write(void** val, void* value) { (void)InterlockedExchangePointer(val, value); }
void* lock_write_eq(void** val, void* old, void* newval) { return InterlockedCompareExchangePointer(val, old, newval); }
#endif
