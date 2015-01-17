#include "os.h"
#ifdef THREAD_PTHREAD
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//list of synchronization points: http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap04.html#tag_04_10

struct threaddata_pthread {
	function<void()> func;
};
static void * threadproc(void * userdata)
{
	struct threaddata_pthread * thdat=(struct threaddata_pthread*)userdata;
	thdat->func();
	free(thdat);
	return NULL;
}

void thread_create(function<void()> func)
{
	struct threaddata_pthread * thdat=malloc(sizeof(struct threaddata_pthread));
	thdat->func=func;
	pthread_t thread;
	if (pthread_create(&thread, NULL, threadproc, thdat)) abort();
	pthread_detach(thread);
}

unsigned int thread_ideal_count()
{
	//for more OSes: https://qt.gitorious.org/qt/qt/source/HEAD:src/corelib/thread/qthread_unix.cpp#L411, idealThreadCount()
	//or http://stackoverflow.com/questions/150355/programmatically-find-the-number-of-cores-on-a-machine
	return sysconf(_SC_NPROCESSORS_ONLN);
}


mutex::mutex()
{
	this->data=malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init((pthread_mutex_t*)this->data, NULL);
}

mutex::~mutex()
{
	pthread_mutex_destroy((pthread_mutex_t*)this->data);
	free(this->data);
}

void mutex::lock()
{
	pthread_mutex_lock((pthread_mutex_t*)this->data);
}

bool mutex::try_lock()
{
	return (pthread_mutex_trylock((pthread_mutex_t*)this->data)==0);
}

void mutex::unlock()
{
	pthread_mutex_unlock((pthread_mutex_t*)this->data); 
}


event::event()
{
	this->data=malloc(sizeof(sem_t));
	sem_init((sem_t*)this->data, 0, 0);
}

event::~event()
{
	sem_destroy((sem_t*)this->data);
	free(this->data);
}

void event::signal()
{
	if (!this->signalled()) sem_post((sem_t*)this->data);
}

void event::wait()
{
	sem_wait((sem_t*)this->data);
}

bool event::signalled()
{
	int active;
	sem_getvalue((sem_t*)this->data, &active);
	return (active>0);
}


multievent::multievent()
{
	this->data=malloc(sizeof(sem_t));
	sem_init((sem_t*)this->data, 0, 0);
}

multievent::~multievent()
{
	sem_destroy((sem_t*)this->data);
	free(this->data);
}

void multievent::signal(unsigned int count)
{
	while (count--) sem_post((sem_t*)this->data);
}

void multievent::wait(unsigned int count)
{
	while (count--) sem_wait((sem_t*)this->data);
}

signed int multievent::count()
{
	int active;
	sem_getvalue((sem_t*)this->data, &active);
	return active;
}


//pthread doesn't seem to contain anything like this, so I'll ask the one who does implement this.
#if __GNUC__*10000 + __GNUC_MINOR__*100 + __GNUC_PATCHLEVEL__*1 >= 40700
//https://gcc.gnu.org/onlinedocs/gcc-4.7.0/gcc/_005f_005fatomic-Builtins.html
uint32_t lock_incr(uint32_t * val) { return __atomic_fetch_add(val, 1, __ATOMIC_ACQ_REL); }
uint32_t lock_decr(uint32_t * val) { return __atomic_fetch_sub(val, 1, __ATOMIC_ACQ_REL); }
uint32_t lock_read(uint32_t * val) { return __atomic_load_n(val, __ATOMIC_ACQUIRE); }

void* lock_read(void* * val) { return __atomic_load_n(val, __ATOMIC_ACQUIRE); }
void lock_write(void** val, void* newval) { return __atomic_store_n(val, newval, __ATOMIC_RELEASE); }
//there is a modern version of this, but it adds another move instruction for whatever reason and otherwise gives the same binary.
void* lock_write_eq(void** val, void* old, void* newval) { return __sync_val_compare_and_swap(val, old, newval); }
//void* lock_write_eq(void** val, void* old, void* newval)
//{
//	__atomic_compare_exchange_n(val, &old, newval, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
//	return old;
//}
#else
//https://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Atomic-Builtins.html
uint32_t lock_incr(uint32_t * val) { return __sync_fetch_and_add(val, 1); }
uint32_t lock_decr(uint32_t * val) { return __sync_fetch_and_sub(val, 1); }
uint32_t lock_read(uint32_t * val) { return __sync_val_compare_and_swap(val, 0, 0); }

inline void* lock_read(void* * val) { return __sync_val_compare_and_swap(val, 0, 0); }
void lock_write(void** val, void* newval) { *val=newval; __sync_synchronize(); }
void* lock_write_eq(void** val, void* old, void* newval) { return __sync_val_compare_and_swap(val, old, newval); }
#endif
#endif
