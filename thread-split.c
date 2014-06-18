#include "minir.h"

//TODO: there is no procedure for destroying threads
struct threadpool {
	struct mutex * lock;
	
	struct event * wake;
	struct event * started;
	unsigned int numthreads;
	unsigned int numidle;
	
	//these vary between each piece of work
	void(*work)(unsigned int id, void* userdata);
	unsigned int id;
	void* userdata;
	struct event * done;
};

static struct threadpool * pool;

static void threadproc(void* userdata)
{
	struct threadpool * this=(struct threadpool*)userdata;
	
	while (true)
	{
		this->wake->wait(this->wake);
		lock_decr(&this->numidle);
		
		void(*work)(unsigned int id, void* userdata) = this->work;
		unsigned int id = lock_incr(&this->id);
		void* userdata = this->userdata;
		struct event * done = this->done;
		
		this->started->signal(this->started);
		work(id, userdata);
		done->signal(done);
		lock_incr(&this->numidle);
	}
}

void thread_split(unsigned int count, void(*work)(unsigned int id, void* userdata), void* userdata)
{
	if (!count) return;
	if (count==1)
	{
		work(0, userdata);
		return;
	}
	struct threadpool * this=pool;
	if (!this)
	{
		this=malloc(sizeof(struct threadpool));
		pool=this;
		this->lock=mutex_create();
		this->wake=event_create();
		this->started=event_create();
		this->numthreads=0;
		this->numidle=0;
		
		//thread_create(threadproc, this);
	}
	this->lock->lock(this->lock);
	struct event * done=event_create();
	
	this->work=work;
	this->id=1;
	this->userdata=userdata;
	this->done=done;
	
	while (lock_read(&this->numidle) < count-1)
	{
		this->numthreads++;
		lock_incr(&this->numidle);
		thread_create(threadproc, this);
	}
	
	this->wake->multisignal(this->wake, count-1);
	this->started->multiwait(this->started, count-1);
	this->lock->unlock(this->lock);
	
	work(0, userdata);
	
	done->multiwait(done, count-1);
	done->free(done);
}






