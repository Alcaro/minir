#include "os.h"

#ifdef THREAD_NO_RWLOCK
//screw this, I'm lazy. I'll do it properly later.
struct rwlock_impl {
	mutex* lock;
};

rwlock* rwlock::create()
{
	struct rwlock_impl* impl=malloc(sizeof(struct rwlock_impl));
	impl->lock=mutex::create();
	return (rwlock*)impl;
}

void rwlock::wlock()
{
	struct rwlock_impl* impl=(struct rwlock_impl*)this;
	impl->lock->lock();
}

bool rwlock::wtry_lock()
{
	struct rwlock_impl* impl=(struct rwlock_impl*)this;
	return impl->lock->try_lock();
}

void rwlock::wunlock()
{
	struct rwlock_impl* impl=(struct rwlock_impl*)this;
	impl->lock->unlock();
}

void rwlock::rlock()
{
	struct rwlock_impl* impl=(struct rwlock_impl*)this;
	impl->lock->lock();
}

bool rwlock::rtry_lock()
{
	struct rwlock_impl* impl=(struct rwlock_impl*)this;
	return impl->lock->try_lock();
}

void rwlock::runlock()
{
	struct rwlock_impl* impl=(struct rwlock_impl*)this;
	impl->lock->unlock();
}

void rwlock::release()
{
	struct rwlock_impl* impl=(struct rwlock_impl*)this;
	delete impl->lock;
	free(impl);
}
#endif
