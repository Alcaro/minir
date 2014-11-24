#include "minir.h"
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

//this is gcc, not pthread, but it works.
unsigned int lock_incr(unsigned int * val)
{
	return __sync_fetch_and_add(val, 1);
}

unsigned int lock_decr(unsigned int * val)
{
	return __sync_fetch_and_sub(val, 1);
}

unsigned int lock_read(unsigned int * val)
{
	return __sync_val_compare_and_swap(val, 0, 0);
}
#endif
