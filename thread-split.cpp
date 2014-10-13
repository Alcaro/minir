#include "minir.h"

namespace {

#define this This

//TODO: there is no procedure for destroying threads
struct threadpool {
	mutex* lock;
	
	multievent* wake;
	multievent* started;
	uint32_t numthreads;
	uint32_t numidle;
	
	//these vary between each piece of work
	void(*work)(unsigned int id, void* userdata);
	uint32_t id;
	void* userdata;
	multievent* done;
};

static struct threadpool * pool;

void threadproc(void* userdata)
{
	struct threadpool * this=(struct threadpool*)userdata;
	
	while (true)
	{
		this->wake->wait();
		lock_decr(&this->numidle);
		
		void(*work)(unsigned int id, void* userdata) = this->work;
		unsigned int id = lock_incr(&this->id);
		void* userdata = this->userdata;
		multievent* done = this->done;
		
		this->started->signal();
		work(id, userdata);
		done->signal();
		lock_incr(&this->numidle);
	}
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
		this->lock=new mutex();
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
	this->userdata=userdata;
	this->done=done;
	
	while (lock_read(&this->numidle) < count-1)
	{
		this->numthreads++;
		lock_incr(&this->numidle);
		thread_create(bind_ptr(threadproc, this));
	}
	
	this->wake->signal(count-1);
	this->started->wait(count-1);
	this->lock->unlock();
	
	work(0, userdata);
	
	done->wait(count-1);
	delete done;
}
