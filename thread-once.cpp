#include "os.h"

static event* thread_once_event(event* * location)
{
	//this is different from thread_once - the equivalent of calculate() can be called twice. If this happens, we undo one of them.
	if (*location) return *location;//nonatomic - if something weird happens, all that happens is that another item is created and deleted.
	event* ev=new event;
	if (lock_write_eq((void**)location, NULL, ev)!=NULL) delete ev;
	return *location;
}

mutex* thread_once_mutex(mutex* * location)
{
	//this is different from thread_once - the equivalent of calculate() can be called twice. If this happens, we undo one of them.
	if (*location) return *location;//nonatomic - if something weird happens, all that happens is that another item is created and deleted.
	mutex* mut=mutex::create();
	if (lock_write_eq((void**)location, NULL, mut)!=NULL) delete mut;
	return *location;
}

static event* contention_unlocker=NULL;
#if 1 //if NULL==0 and points to a permanently reserved area of at least 3 bytes (the limit is 65536 on all modern OSes)
#define tag_busy ((void*)1)
#define tag_contended ((void*)2)
#else //assume sizeof(obj*)>=2 - no other thread can return this, they don't know where it is
#define MAKE_TAG(n) (void*)(((char*)&contention_unlocker)+n)
#define tag_busy MAKE_TAG(0)
#define tag_contended MAKE_TAG(1)
#endif

//Bug: If two objects are simultaneously initialized by two threads each, then one of the objects may hold up the other.
//This is not fixable without borrowing at least one bit from the item, which we don't want to do.
void* thread_once_core(void* * item, function<void*()> calculate)
{
	void* check=*item;
	if (check!=NULL && check!=tag_busy && check!=tag_contended) return check; // common case - initialized already
	
	void* old=lock_write_eq(item, NULL, tag_busy);
	if (old==NULL)
	{
		void* result=calculate();
		//'written' is either tag_busy or tag_contended here.
		//It's not NULL because we wrote tag_busy, and it can't be anything else
		// because the other threads know that they're only allowed to replace it with tag_contended.
		if (lock_write_eq(item, tag_busy, result)==tag_contended)
		{
			thread_once_event(&contention_unlocker);
			lock_write(item, result);
			contention_unlocker->signal();
		}
	}
	else if (old==tag_busy || old==tag_contended)
	{
		//don't bother optimizing this, contention only happens a few times during program lifetime
		lock_write_eq(item, tag_busy, tag_contended);
		thread_once_event(&contention_unlocker);
		while (lock_read(item)==tag_busy) contention_unlocker->wait();
		contention_unlocker->signal();
	}
	//it's possible to hit neither of the above if the object was written between the initial read and the swap
	return *item;
}
