#include "os.h"
#ifdef __linux__
//I could try to rewrite all of this without pthread, but I'd rather not set up TLS stuff myself, that'd require replacing half of libc.
//However, I can remove everything except pthread_create.
//Minimum kernel version: 2.6.22 (FUTEX_PRIVATE_FLAG), released in 8 July, 2007 (source: http://kernelnewbies.org/LinuxVersions)
//Dropping the private mutex flag would drop requirements to 2.5.40, October 1, 2002.
#include <pthread.h>
#include <unistd.h>

#include <linux/futex.h>
#include <sys/syscall.h>

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

void thread_sleep(unsigned int usec)
{
	usleep(usec);
}


static int futex(int * uaddr, int op, int val, const struct timespec * timeout = NULL,
                 int * uaddr2 = NULL, int val3 = 0)
{
	syscall(__NR_futex, uaddr, op, val, timeout, uaddr2, val3);
}

//futexes. complex threading code. fun
#define MUT_UNLOCKED 0
#define MUT_LOCKED 1
#define MUT_CONTENDED 2
void mutex2::lock()
{
	int result = lock_cmpxchg(&fut, MUT_UNLOCKED, MUT_LOCKED);
	if (result == MUT_UNLOCKED)
	{
		return; // nothing to do, this is the fast path
	}
	
	lock_cmpxchg(&fut, MUT_LOCKED, MUT_CONTENDED);
	//may have changed in the meanwhile
	//changed to CONTENDED - ignore failure, that's what we want anyways
	//changed to UNLOCKED - ignore failure again, futex() will instantly return and then the cmpxchg will lock it for us
	while (true)
	{
		futex(&fut, FUTEX_WAIT|FUTEX_PRIVATE_FLAG, MUT_CONTENDED);
		result = lock_xchg(&fut, MUT_CONTENDED);
		if (result == MUT_UNLOCKED) break;
	}
}

bool mutex2::try_lock()
{
	return (lock_cmpxchg(&fut, MUT_UNLOCKED, MUT_LOCKED) == MUT_UNLOCKED);
}

void mutex2::unlock()
{
	int result = lock_xchg(&fut, MUT_UNLOCKED);
	if (result == MUT_CONTENDED)
	{
		futex(&fut, FUTEX_WAKE|FUTEX_PRIVATE_FLAG, 1);
	}
}






//stuff I should rewrite follows
#include <semaphore.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

mutex* mutex::create()
{
	pthread_mutex_t* ret=malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(ret, NULL);
	return (mutex*)ret;
}

void mutex::lock()
{
	pthread_mutex_lock((pthread_mutex_t*)this);
}

bool mutex::try_lock()
{
	return (pthread_mutex_trylock((pthread_mutex_t*)this)==0);
}

void mutex::unlock()
{
	pthread_mutex_unlock((pthread_mutex_t*)this); 
}

void mutex::release()
{
	pthread_mutex_destroy((pthread_mutex_t*)this);
	free(this);
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


uintptr_t thread_get_id()
{
	//disassembly:
	//jmpq   0x400500 <pthread_self@plt>
	//jmpq   *0x200b22(%rip)        # 0x601028 <pthread_self@got.plt>
	//mov    %fs:0x10,%rax
	//retq
	//(it's some big mess the first time, apparently the dependency is dynamically loaded)
	return pthread_self();
}
#endif
