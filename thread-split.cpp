#include "os.h"

namespace {

//force some year-old C code to compile properly as C++ - I decided to switch long ago but still haven't finished.
#define this This

//TODO: there is no procedure for destroying threads
struct threadpool {
	mutex* lock;
	
	multievent* wake;
	multievent* started;
	uint32_t numthreads;
	uint32_t numidle;
	
	//these vary between each piece of work
	function<void(unsigned int id)> work;
	uint32_t id;
	multievent* done;
};

static struct threadpool * pool;

void threadproc(struct threadpool * this)
{
	while (true)
	{
		this->wake->wait();
		lock_decr(&this->numidle);
		
		function<void(unsigned int id)> work = this->work;
		unsigned int id = lock_incr(&this->id);
		multievent* done = this->done;
		
		this->started->signal();
		work(id);
		done->signal();
		lock_incr(&this->numidle);
	}
}

}

void thread_split(unsigned int count, function<void(unsigned int id)> work)
{
	if (!count) return;
	if (count==1)
	{
		work(0);
		return;
	}
	struct threadpool * this=pool;
	if (!this)
	{
		this=malloc(sizeof(struct threadpool));
		pool=this;
		this->lock=mutex::create();
		this->wake=new multievent();
		this->started=new multievent();
		this->numthreads=0;
		this->numidle=0;
		
		//thread_create(threadproc, this);
	}
	this->lock->lock();
	multievent* done=new multievent();
	
	this->work=work;
	this->id=1;
	this->done=done;
	
	while (lock_read(&this->numidle) < count-1)
	{
		this->numthreads++;
		lock_incr(&this->numidle);
		thread_create(bind_this(threadproc));
	}
	
	this->wake->signal(count-1);
	this->started->wait(count-1);
	this->lock->unlock();
	
	work(0);
	
	done->wait(count-1);
	delete done;
}
