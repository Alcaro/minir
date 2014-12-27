#include "os.h"

static event* contention_unlocker=NULL;
#if 1
#define tag_busy ((void*)1)
#define tag_contended ((void*)2)
#else
#define MAKE_TAG(n) (void*)(((char*)&contention_unlocker)+n)
#define tag_busy MAKE_TAG(0)
#define tag_contended MAKE_TAG(1)
#endif

static void make_event()
{
	if (contention_unlocker) return;//nonatomic - if something weird happens, all that happens is that another event is created and deleted.
	event* ev=new event;
	if (lock_write_eq((void**)&contention_unlocker, NULL, ev)!=NULL) delete ev;
}

//Limitation: If two objects are simultaneously initialized by two threads each, then one of the objects may hold up the other.
void* thread_once_core(void* * item, function<void*()> calculate)
{
	void* check=*item;
	if (check!=NULL && check!=tag_busy && check!=tag_contended) return check; // common case - initialized already
	
	void* written=lock_write_eq(item, NULL, tag_busy);
	if (written==NULL)
	{
		void* result=calculate();
		//'written' is either tag_busy or tag_contended here.
		//It's not NULL because we wrote tag_busy. The other threads know that they're only allowed to replace it with tag_contended.
		if (lock_write_eq(item, tag_busy, result)==tag_contended)
		{
			make_event();
			lock_write(item, result);
			contention_unlocker->signal();
		}
	}
	if (written==tag_busy || written==tag_contended)
	{
		void* written=lock_write_eq(item, tag_busy, tag_contended);
		if (written==tag_busy || written==tag_contended)
		{
			make_event();
			while (lock_read(item)==tag_busy) contention_unlocker->wait();
			contention_unlocker->signal();
		}
	}
	//it's possible to hit neither of the above if the object was written between the nonatomic read and the swap
	return *item;
}
