#pragma once
#include "global.h"

//this is more thread.h than anything else - dylib is the only non-thread-related part. But there's
// no other place where dylib would fit, so os.h it is.

#ifdef DYLIB_POSIX
#define DYLIB_EXT ".so"
#define DYLIB_MAKE_NAME(name) "lib" name DYLIB_EXT
#endif
#ifdef DYLIB_WIN32
#define DYLIB_EXT ".dll"
#define DYLIB_MAKE_NAME(name) name DYLIB_EXT
#endif

//Nasty stuff going on here... it's impossible to construct this object.
//The size varies per platform, so I have to allocate the object. This could be done by putting in a void* member,
// but that's a pointless level of indirection - instead, I cast the allocated value and return that!
//It's probably undefined, but the compiler won't be able to prove that, so it has to do what I want.
//Perhaps it would be better to let the configure script declare what the size is so they can have a
// member of type uint32_t data[12] and be constructed normally, but this is good enough for now.
class dylib : private nocopy {
	dylib(){}
public:
	static dylib* create(const char * filename, bool * owned=NULL);
	static const char * ext() { return DYLIB_EXT; }
	void* sym_ptr(const char * name);
	funcptr sym_func(const char * name);
	
	//per http://chadaustin.me/cppinterface.html - redirect operator delete to a function, this doesn't come from the normal allocator.
	static void operator delete(void* p) { if (p) ((dylib*)p)->release(); }
private:
	void release();//this is the real destructor
};



//If the program is run under a debugger, this triggers a breakpoint. If not, ignored.
void debug_break();
//If the program is run under a debugger, this triggers a breakpoint. The program is then terminated.
void debug_abort();



//Any data associated with this thread is freed once the thread procedure returns.
//It is safe to malloc() something in one thread and free() it in another.
//It is not safe to call window_run_*() from within another thread than the one entering main().
//A thread is rather heavy; for short-running jobs, use thread_create_short or thread_split.
void thread_create(function<void()> startpos);

//Returns the number of threads to create to utilize the system resources optimally.
unsigned int thread_ideal_count();

//This is a simple tool that ensures only one thread is doing a certain action at a given moment.
//Memory barriers are inserted as appropriate. Any memory access done while holding a lock is finished while holding this lock.
//This means that if all access to an object is done exclusively while holding the lock, no further synchronization is needed.
//It is not allowed for a thread to call lock() or try_lock() while holding the lock already. It is not allowed
// for a thread to release the lock unless it holds it. It is not allowed to delete the lock while it's held.
//However, it it allowed to hold multiple locks simultaneously.
//lock() is not guaranteed to yield the CPU if it can't grab the lock. It may be implemented as a busy loop.
//Remember to create all relevant mutexes before creating a thread.
class mutex : private nocopy {
	mutex(){}
public:
	static mutex* create();
	void lock();
	bool try_lock();
	void unlock();
	
	static void operator delete(void* p) { if (p) ((mutex*)p)->release(); }
private:
	void release();
};


//A reader-writer lock is similar to a mutex, but can be locked in two ways - either for reading or for writing.
//Multiple threads may lock it for reading simultaneously; however, only one writer may enter.
class rwlock : private nocopy {
	rwlock(){}
public:
	static rwlock* create();
	
	void wlock();
	bool wtry_lock();
	void wunlock();
	
	void rlock();
	bool rtry_lock();
	void runlock();
	
	static void operator delete(void* p) { if (p) ((rwlock*)p)->release(); }
private:
	void release();
};



//Executes 'calculate' exactly once. The return value is stored in 'item'. If multiple threads call
// this simultaneously, none returns until calculate() is done.
//'item' must be initialized to NULL. calculate() must return a valid pointer to an object.
// 'return new mutex;' is valid, as is returning the address of something static.
//Returns *item.
void* thread_once_core(void* * item, function<void*()> calculate);
template<typename T> T* thread_once(T* * item, function<T*()> calculate)
{
	return (T*)thread_once_core((void**)item, *(function<void*()>*)&calculate);
}


//This is like thread_once, but calculate() can be called multiple times. If this happens, undo()
//will be called for all except one; the last one will be returned.
void* thread_once_undo_core(void* * item, function<void*()> calculate, function<void(void*)> undo);
template<typename T> T* thread_once_undo(T* * item, function<T*()> calculate, function<void(T*)> undo)
{
	return (T*)thread_once_undo_core((void**)item, *(function<void*()>*)&calculate, *(function<void(void*)>*)&undo);
}


//This function is a workaround for a GCC bug. Don't call it yourself.
template<void*(*create)(), void(*undo)(void*)> void* thread_once_create_gccbug(void* * item)
{
	return thread_once_undo(item, bind(create), bind(undo));
}
//Simple convenience function, just calls the above.
template<typename T> T* thread_once_create(T* * item)
{
	return (T*)thread_once_create_gccbug<generic_new_void<T>, generic_delete_void<T> >((void**)item);
}

#define CREATE_FUNCTION(T)                                                  \
	static inline T* thread_once_create(T* * item)                            \
	{                                                                         \
		class I {                                                               \
		public:                                                                 \
			static void* create() { return (void*)T::create(); }                  \
			static void undo(void* obj) { delete (T*)obj; }                       \
		};                                                                      \
		                                                                        \
		return (T*)thread_once_create_gccbug<generic_create_void<T>, generic_delete_void<T> >((void**)item); \
	}

CREATE_FUNCTION(mutex);
#undef CREATE_FUNCTION


class mutexlocker {
	mutexlocker();
	mutex* m;
public:
	mutexlocker(mutex* m) { this->m=m; this->m->lock(); }
	~mutexlocker() { this->m->unlock(); }
};
#define CRITICAL_FUNCTION() static smutex CF_holder; mutexlocker CF_lock(&CF_holder)

//Static mutex. Initialized on first use.
class smutex {
public:
	smutex() : mut(NULL) {}
	~smutex() { delete mut; }
	
	void lock() { thread_once_create(&this->mut)->lock(); }
	bool try_lock() { return thread_once_create(&this->mut)->try_lock(); }
	void unlock() { this->mut->unlock(); }
	
	mutex* operator&() { return thread_once_create(&this->mut); }
private:
	mutex* mut;
};

//This one lets one thread wake another.
//The conceptual difference between this and a mutex is that while a mutex is intended to protect a
// shared resource from being accessed simultaneously, an event is intended to wait until another
// thread is done with something. A mutex is unlocked on the same thread as it's locked; an event is
// unlocked on a different thread.
//An example would be a producer-consumer scenario; if one thread is producing 200 items per second,
// and another thread processes them at 100 items per second, then there will soon be a lot of
// waiting items. An event allows the consumer to ask the producer to get to work, so it'll spend
// half of its time sleeping, instead of filling the system memory.
//An event is boolean; calling signal() twice will drop the extra signal. It is created in the unsignalled state.
//Can be used by multiple threads, but each of signal(), wait() and signalled() should only be used by one thread.
class event {
public:
	event();
	~event();
	
	void signal();
	void wait();
	bool signalled();
	
private:
	void* data;
};

//This is like event, but it allows setting the event multiple times.
class multievent {
public:
	multievent();
	~multievent();
	
	//count is how many times to signal or wait. Calling it multiple times is equivalent to calling it with the sum of the arguments.
	void signal(unsigned int count=1);
	void wait(unsigned int count=1);
	//This is how many signals are waiting to be wait()ed for. Can be below zero if something is currently waiting for this event.
	//Alternate explaination: Increased for each entry to signal() and decreased for each entry to wait().
	signed int count();
private:
	void* data;
	signed int n_count;//Not used by all implementations.
};

//Increments or decrements a variable, while guaranteeing atomicity relative to other threads. lock_read() just reads the value.
//Returns the value after changing it.
uint32_t lock_incr(uint32_t* val);
uint32_t lock_decr(uint32_t* val);

uint32_t lock_read(uint32_t* val);
void lock_write(uint32_t* val, uint32_t newval);
//Writes 'newval' to *val only if it currently equals 'old'. Returns the old value of *val, which can be compared with 'old'.
uint32_t lock_write_eq(uint32_t* val, uint32_t old, uint32_t newval);

//Alternate overloads for pointers.
void* lock_read_i(void** val);
void lock_write_i(void** val, void* newval);
void* lock_write_eq_i(void** val, void* old, void* newval);
void* lock_xchg_i(void** val, void* newval);

template<typename T> T* lock_read(T** val) { return (T*)lock_read_i((void**)val); }
template<typename T> void lock_write(T** val, T* newval) { lock_write_i((void**)val, (void*)newval); }
template<typename T> T* lock_write_eq(T** val, T* old, T* newval) { return (T*)lock_write_eq_i((void**)val, (void*)old, (void*)newval); }
template<typename T> T* lock_xchg(T** val, T* newval) { return (T*)lock_xchg_i((void**)val, (void*)newval); }

#if NULL==0
//the NULL/0 duality is one of the dumbest things I have ever seen. at least C++11 fixes that garbage
class null_only;
template<typename T> void lock_write(T** val, null_only* newval) { lock_write_i((void**)val, NULL); }
template<typename T> T* lock_write_eq(T** val, null_only* old, T* newval) { return (T*)lock_write_eq_i((void**)val, NULL, (void*)newval); }
template<typename T> T* lock_write_eq(T** val, T* old, null_only* newval) { return (T*)lock_write_eq_i((void**)val, (void*)old, NULL); }
template<typename T> T* lock_write_eq(T** val, null_only* old, null_only* newval) { return (T*)lock_write_eq_i((void**)val, NULL, NULL); }
template<typename T> T* lock_xchg(T** val, null_only* newval) { return (T*)lock_xchg_i((void**)val, NULL); }
#endif

void thread_sleep(unsigned int usec);

//Returns a value that's unique to the current thread for as long as the process lives. Does not
// necessarily have any relationship to OS-level thread IDs.
size_t thread_get_id();

//This one creates 'count' threads, calls startpos() in each of them with 'id' from 0 to 'count'-1, and
// returns once each thread has returned.
//Unlike thread_create, thread_split is expected to be called often, for short-running tasks. The threads may be reused.
//It is safe to use the values 0 and 1. However, you should avoid going above thread_ideal_count().
void thread_split(unsigned int count, function<void(unsigned int id)> work);
