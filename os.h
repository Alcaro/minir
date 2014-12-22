#pragma once
#include "global.h"

//C-based API that uses native dylib handles directly.
typedef struct ndylib_* ndylib;
ndylib* dylib_create(const char * filename, bool * owned=NULL);//Handles may be non-unique. First to load is owner.
void* dylib_sym_ptr(ndylib* lib, const char * name);
funcptr dylib_sym_func(ndylib* lib, const char * name);
void dylib_free(ndylib* lib);
#ifdef DYLIB_POSIX
#define DYLIB_EXT ".so"
#define DYLIB_MAKE_NAME(name) "lib" name DYLIB_EXT
#endif
#ifdef DYLIB_WIN32
#define DYLIB_EXT ".dll"
#define DYLIB_MAKE_NAME(name) name DYLIB_EXT
#endif
static const char * dylib_ext() { return DYLIB_EXT; }



class dylib : private nocopy {
public:
	static dylib* create(const char * filename)
	{
		dylib* ret=new dylib;
		ret->lib = dylib_create(filename, &ret->owned_);
		if (!ret->lib)
		{
			delete ret;
			ret=NULL;
		}
		return ret;
	}
	
	static const char * ext() { return dylib_ext(); }
	
	bool owned() { return owned_; }
	
	void* sym_ptr(const char * name) { return dylib_sym_ptr(this->lib, name); }
	funcptr sym_func(const char * name) { return dylib_sym_func(this->lib, name); }
	
	~dylib() { if (this->lib) dylib_free(this->lib); }
	
private:
	ndylib* lib;
	bool owned_;
};



//Any data associated with this thread is freed once the thread procedure returns.
//It is safe to malloc() something in one thread and free() it in another.
//It is not safe to call window_run_*() from within another thread than the one entering main().
//A thread is rather heavy; for short-running jobs, use thread_create_short or thread_split.
void thread_create(function<void()> startpos);

//Returns the number of threads to create to utilize the system resources optimally.
unsigned int thread_ideal_count();

//This is a simple tool that ensures only one thread is doing a certain action at a given moment.
//It may be used from multiple threads simultaneously.
//Memory barrier are inserted as appropriate. Any memory access done while holding a lock is finished while holding this lock.
//This means that if all access to an object is done exclusively while holding the lock, no further synchronization is needed.
//It is not allowed for a thread to call lock() or try_lock() while holding the lock already. It is not allowed
// for a thread to release the lock unless it holds it. It is not allowed to free() the lock while it's held.
//However, it it allowed to hold multiple locks simultaneously.
//lock() is not guaranteed to yield the CPU if it can't grab the lock. It may be implemented as a busy loop.
//Remember to create all relevant mutexes before creating a thread.
class mutex {
public:
	mutex();
	~mutex();
	
	void lock();
	bool try_lock();
	void unlock();
private:
	void* data;
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
	
	//count is how many times to signal or wait. It is equivalent to calling it multiple times with count=1.
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
//Returns the value before changing it.
uint32_t lock_incr(uint32_t* val);
uint32_t lock_decr(uint32_t* val);
uint32_t lock_read(uint32_t* val);
void lock_write(uint32_t* val, uint32_t value);
//Writes 'newval' to *val only if it currently equals 'old'. Returns the old value of *val, which can be compared with 'old'.
uint32_t lock_write_eq(uint32_t* val, uint32_t old, uint32_t newval);

//For various data sizes.
uint8_t lock_incr(uint8_t* val);
uint8_t lock_decr(uint8_t* val);
uint8_t lock_read(uint8_t* val);
void lock_write(uint8_t* val, uint8_t value);
uint8_t lock_write_eq(uint8_t* val, uint8_t old, uint8_t newval);

uint16_t lock_incr(uint16_t* val);
uint16_t lock_decr(uint16_t* val);
uint16_t lock_read(uint16_t* val);
void lock_write(uint16_t* val, uint16_t value);
uint16_t lock_write_eq(uint16_t* val, uint16_t old, uint16_t newval);

uint64_t lock_incr(uint64_t* val);
uint64_t lock_decr(uint64_t* val);
uint64_t lock_read(uint64_t* val);
void lock_write(uint64_t* val, uint64_t value);
uint64_t lock_write_eq(uint64_t* val, uint64_t old, uint64_t newval);

void* lock_read(void** val);
void lock_write(void** val, void* value);
void* lock_write_eq(void** val, void* old, void* newval);

//This one creates 'count' threads, calls startpos() in each of them with 'id' from 0 to 'count'-1, and
// returns once each thread has returned.
//Unlike thread_create, thread_split is expected to be called often, for short-running tasks. The threads may be reused.
//It is safe to use the values 0 and 1. However, you should avoid going above thread_ideal_count().
void thread_split(unsigned int count, void(*work)(unsigned int id, void* userdata), void* userdata);

//These are provided for subsystems which can not dynamically initialize a mutex, due to not having an initialization function.
enum _int_mutex {
	_imutex_cwd,
	_imutex_dylib,
	_imutex_count
};
void _int_mutex_lock(enum _int_mutex id);
bool _int_mutex_try_lock(enum _int_mutex id);
void _int_mutex_unlock(enum _int_mutex id);
