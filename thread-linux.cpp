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


static int futex_wait(int * uaddr, int val, const struct timespec * timeout = NULL)
{
	return syscall(__NR_futex, uaddr, FUTEX_WAIT_PRIVATE, val, timeout);
}
static int futex_wake(int * uaddr, int val)
{
	return syscall(__NR_futex, uaddr, FUTEX_WAKE_PRIVATE, val);
}


//futexes. complex threading code. fun
#define MUT_UNLOCKED 0
#define MUT_LOCKED 1
#define MUT_CONTENDED 2

void mutex2::lock()
{
	int result = lock_cmpxchg_acq(&fut, MUT_UNLOCKED, MUT_LOCKED);
	if (result == MUT_UNLOCKED)
	{
		return; // unlocked, fast path
	}
	
	//If it was locked, mark it contended and force whoever to wake us.
	//In the common contended case, it was previously MUT_LOCKED, so the futex would instantly return.
	//Therefore, the xchg should be run first.
	
	while (true)
	{
		result = lock_xchg_loose(&fut, MUT_CONTENDED);
		//results:
		//MUT_UNLOCKED - we got it, continue
		//MUT_CONTENDED - didn't get it, sleep for a while
		//MUT_LOCKED - someone else got it and locked it, thinking it's empty, while we're here. force it to wake us.
		if (result == MUT_UNLOCKED) break;
		
		futex_wait(&fut, MUT_CONTENDED);
	}
}

bool mutex2::try_lock()
{
	return (lock_cmpxchg_acq(&fut, MUT_UNLOCKED, MUT_LOCKED) == MUT_UNLOCKED);
}

void mutex2::unlock()
{
	int result = lock_xchg_rel(&fut, MUT_UNLOCKED);
	if (UNLIKELY(result == MUT_CONTENDED))
	{
		futex_wake(&fut, 1);
	}
}






//stuff I should rewrite follows
#include <semaphore.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>


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
