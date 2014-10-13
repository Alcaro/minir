#include "minir.h"
#ifdef THREAD_PTHREAD
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define this This

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

void thread_create(void(*startpos)(void* userdata), void* userdata)
{
	struct threaddata_pthread * thdat=malloc(sizeof(struct threaddata_pthread));
	thdat->startpos=startpos;
	thdat->userdata=userdata;
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


struct mutex_pthread {
	struct mutex i;
	
	pthread_mutex_t lock;
};

static void mutex_lock(struct mutex * this_)
{
	struct mutex_pthread * this=(struct mutex_pthread*)this_;
	pthread_mutex_lock(&this->lock);
}

static bool mutex_try_lock(struct mutex * this_)
{
	struct mutex_pthread * this=(struct mutex_pthread*)this_;
	return (pthread_mutex_trylock(&this->lock)==0);
}

static void mutex_unlock(struct mutex * this_)
{
	struct mutex_pthread * this=(struct mutex_pthread*)this_;
	pthread_mutex_unlock(&this->lock); 
}

static void mutex_free_(struct mutex * this_)
{
	struct mutex_pthread * this=(struct mutex_pthread*)this_;
	pthread_mutex_destroy(&this->lock);
	free(this);
}

const struct mutex mutex_pthread_base = {
	mutex_lock, mutex_try_lock, mutex_unlock, mutex_free_
};
struct mutex * mutex_create()
{
	struct mutex_pthread * this=malloc(sizeof(struct mutex_pthread));
	memcpy(&this->i, &mutex_pthread_base, sizeof(struct mutex));
	
	pthread_mutex_init(&this->lock, NULL);
	
	return (struct mutex*)this;
}


struct event_pthread {
	struct event i;
	
	sem_t ev;
};

static void event_signal(struct event * this_)
{
	struct event_pthread * this=(struct event_pthread*)this_;
	sem_post(&this->ev);
}

static void event_wait(struct event * this_)
{
	struct event_pthread * this=(struct event_pthread*)this_;
	while (sem_wait(&this->ev)==EINTR) {} //the more plentiful one of user and implementation shall be
	                                      // simpler (minimizes the bug risk), so why does EINTR exist
}

static void event_multisignal(struct event * this_, unsigned int count)
{
	struct event_pthread * this=(struct event_pthread*)this_;
	while (count--) sem_post(&this->ev);
}

static void event_multiwait(struct event * this_, unsigned int count)
{
	struct event_pthread * this=(struct event_pthread*)this_;
	while (count--)
	{
		while (sem_wait(&this->ev)==EINTR) {} //the more plentiful one of user and implementation shall be
		                                      // simpler (minimizes the bug risk), so why does EINTR exist
	}
}

static int event_count(struct event * this_)
{
	struct event_pthread * this=(struct event_pthread*)this_;
	int active;
	sem_getvalue(&this->ev, &active);
	return active;
}

static void event_free_(struct event * this_)
{
	struct event_pthread * this=(struct event_pthread*)this_;
	sem_destroy(&this->ev);
	free(this);
}

struct event * event_create()
{
	struct event_pthread * this=malloc(sizeof(struct event_pthread));
	this->i.signal=event_signal;
	this->i.wait=event_wait;
	this->i.multisignal=event_multisignal;
	this->i.multiwait=event_multiwait;
	this->i.count=event_count;
	this->i.free=event_free_;
	
	sem_init(&this->ev, 0, 0);
	
	return (struct event*)this;
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
