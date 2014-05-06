#include "minir.h"
#include <stdlib.h>
#include <string.h>

struct video_thread_frame {
	char* pixels;
	size_t bufsize;
	unsigned int width;
	unsigned int height;
	bool null;
};

struct video_thread {
	struct video i;
	
	//Mutexes and events. All use of these is thread safe.
	struct mutex * lock;
	struct event * wake_child;
	struct event * wake_parent;
	
	struct video * child;//This one is safe to check for NULL on the parent thread; otherwise, only the child may use it. May only be
	                     // accessed while NOT holding the mutex, as it's expensive; with the exception that deletion should be synchronous.
	
	UNION_BEGIN
		STRUCT_BEGIN
			//Used only during initialization.
			const char * backend;
			uintptr_t windowhandle;
		STRUCT_END
		STRUCT_BEGIN
			//Used only after initialization.
			bool reinit;
			bool exit;
			bool has_sync;//May be read without holding the lock.
			bool sync;//The parent can read this without locking. The child can't.
			bool change_sync;
			
			bool idle;//Read by the parent. If clear, vsyncing is done; go on.
			bool buf_next_full;//Whether buf_next contains valid data. Read by the child.
			
			char bytes_per_pixel;//Set by the child during initialization. Safe to read.
		STRUCT_END
	UNION_END
	unsigned int screen_width;
	unsigned int screen_height;
	unsigned int depth;
	double fps;
	
	//When the child thread is done drawing:
	//- Wait for signal
	//- Lock
	//- Swap buf_next and buf_this
	//- Set buf_next_full false
	//- Unlock
	//- Signal parent
	//- Draw buf_this
	
	//When the parent wants to draw something:
	//- Copy to buf_temp, ensure it's packed (reallocate if needed)
	//- Clone buf_temp to buf_last
	//- If sync&1:
	// - Lock
	// - Read buf_next_full
	// - Unlock
	// - If buf_next_full:
	//  - Wait for signal
	//  - Repeat check
	//
	//- Lock
	//- Swap buf_next and buf_this, whether buf_next_full is set or not
	//- Set buf_next_full
	//- Unlock
	//- Signal child
	
	struct video_thread_frame buf_this;//currently being drawn to the screen; owned by child, does not need lock (despite the buffer being allocated by the parent)
	struct video_thread_frame buf_next;//next to be drawn to the screen; needs lock
	struct video_thread_frame buf_temp;//frame being created; owned by parent, does not need lock
	
	struct video_thread_frame buf_last;//last frame drawn - does not have unique pointer (owned by parent)
};

static void threadproc(void* userdata)
{
	struct video_thread * this=userdata;
	this->child=video_create(this->backend, this->windowhandle, this->screen_width, this->screen_height, this->depth, this->fps);
	
	if (!this->child)
	{
		this->wake_parent->signal(this->wake_parent);
		return;
	}
	
	this->reinit=false;
	this->exit=false;
	this->has_sync=this->child->has_sync(this->child);
	
	this->bytes_per_pixel=((this->depth==32)?4:2);
	
	this->buf_next_full=false;
	
	this->wake_parent->signal(this->wake_parent);
	
	while (true)
	{
		this->lock->lock(this->lock);
		while (!this->buf_next_full && !this->exit)
		{
			this->lock->unlock(this->lock);
			this->wake_child->wait(this->wake_child);
			this->lock->lock(this->lock);
		}
		
		if (this->exit)
		{
			this->exit=false;
			this->child->free(this->child);
			this->lock->unlock(this->lock);
			this->wake_parent->signal(this->wake_parent);
			return;
		}
		
		this->idle=false;
		
		bool reinit=this->reinit;
		unsigned int screen_width=this->screen_width;
		unsigned int screen_height=this->screen_height;
		unsigned int depth=this->depth;
		double fps=this->fps;
		
		bool sync=this->sync;
		bool change_sync=this->change_sync;
		this->change_sync=false;
		
		struct video_thread_frame thisframe=this->buf_next;
		this->buf_next=this->buf_this;
		this->buf_this=thisframe;
		this->buf_next_full=false;
		
		this->lock->unlock(this->lock);
		
		if (reinit) this->child->reinit(this->child, screen_width, screen_height, depth, fps);
		if (change_sync) this->child->set_sync(this->child, sync);
		
		this->child->draw(this->child, thisframe.width, thisframe.height,
		                  thisframe.null ? NULL : thisframe.pixels, thisframe.width*this->bytes_per_pixel);
		
		this->lock->lock(this->lock);
		this->idle=(!this->buf_next_full);
		this->lock->unlock(this->lock);
		
		this->wake_parent->signal(this->wake_parent);
	}
}

static void reinit(struct video * this_, unsigned int screen_width, unsigned int screen_height, unsigned int depth, double fps)
{
	struct video_thread * this=(struct video_thread*)this_;
	this->lock->lock(this->lock);
	this->reinit=true;
	this->screen_width=screen_width;
	this->screen_height=screen_height;
	this->depth=depth;
	this->fps=fps;
	this->lock->unlock(this->lock);
}

static void draw(struct video * this_, unsigned int width, unsigned int height, const void * data, unsigned int pitch)
{
	struct video_thread * this=(struct video_thread*)this_;
	
	this->buf_temp.null=(!data);
	
	if (data)
	{
		unsigned int true_pitch=this->bytes_per_pixel*width;
		if (this->buf_temp.bufsize!=true_pitch*height)
		{
			free(this->buf_temp.pixels);
			this->buf_temp.bufsize=true_pitch*height;
			this->buf_temp.pixels=malloc(this->buf_temp.bufsize);
		}
		
		const char * srcdat=data;
		char * dstdat=this->buf_temp.pixels;
		for (unsigned int i=0;i<height;i++)
		{
			memcpy(dstdat, srcdat, true_pitch);
			dstdat+=true_pitch;
			srcdat+=pitch;
		}
	}
	
	this->buf_temp.width=width;
	this->buf_temp.height=height;
	
	this->lock->lock(this->lock);
	//use buf_last as temp space - we want to update it anyways
	this->buf_last=this->buf_temp;
	this->buf_temp=this->buf_next;
	this->buf_next=this->buf_last;
	this->buf_next_full=true;
	this->idle=false;
	this->lock->unlock(this->lock);
	this->wake_child->signal(this->wake_child);
	
	//wait for vsync
	if (this->sync)
	{
		while (true)
		{
			this->lock->lock(this->lock);
			bool idle=this->idle;
			this->lock->unlock(this->lock);
			if (idle) break;
			else this->wake_parent->wait(this->wake_parent);
		}
	}
}

static void set_sync(struct video * this_, bool sync)
{
	struct video_thread * this=(struct video_thread*)this_;
	this->lock->lock(this->lock);
	this->sync=sync;
	this->change_sync=true;
	this->lock->unlock(this->lock);
}

static bool has_sync(struct video * this_)
{
	struct video_thread * this=(struct video_thread*)this_;
	return this->has_sync;
}

static bool repeat_frame(struct video * this_, unsigned int * width, unsigned int * height,
                                               const void * * data, unsigned int * pitch, unsigned int * bpp)
{
	struct video_thread * this=(struct video_thread*)this_;
	if (width) *width=this->buf_last.width;
	if (height) *height=this->buf_last.height;
	if (data) *data=this->buf_last.pixels;
	if (pitch) *pitch=this->bytes_per_pixel*this->buf_last.width;
	if (bpp) *bpp=(this->buf_last.pixels ? this->depth : 16);
	return (this->buf_last.pixels);
}

static void free_(struct video * this_)
{
	struct video_thread * this=(struct video_thread*)this_;
	
	this->lock->lock(this->lock);
	this->exit=true;
	this->lock->unlock(this->lock);
	this->wake_child->signal(this->wake_child);
	
	while (true)
	{
		this->lock->lock(this->lock);
		bool waitmore=this->exit;
		this->lock->unlock(this->lock);
		if (!waitmore) break;
		else this->wake_parent->wait(this->wake_parent);
	}
	
	this->wake_parent->free(this->wake_parent);
	this->wake_child->free(this->wake_child);
	this->lock->free(this->lock);
	
	free(this->buf_this.pixels);
	free(this->buf_next.pixels);
	free(this->buf_temp.pixels);
	
	free(this);
}

struct video * video_create_thread(const char * backend, uintptr_t windowhandle,
                                   unsigned int screen_width, unsigned int screen_height,
                                   unsigned int depth, double fps)
{
	struct video_thread * this=malloc(sizeof(struct video_thread));
	memset(this, 0, sizeof(struct video_thread));//mainly for the three video buffers
	this->i.reinit=reinit;
	this->i.draw=draw;
	this->i.set_sync=set_sync;
	this->i.has_sync=has_sync;
	this->i.repeat_frame=repeat_frame;
	this->i.free=free_;
	
	this->backend=backend;
	this->windowhandle=windowhandle;
	this->screen_width=screen_width;
	this->screen_height=screen_height;
	this->depth=depth;
	this->fps=fps;
	
	this->wake_child=event_create();
	this->wake_parent=event_create();
	this->lock=mutex_create();
	thread_create(threadproc, this);
	this->wake_parent->wait(this->wake_parent);
	
	if (!this->child) goto cancel;
	
	return (struct video*) this;
	
cancel:
	this->wake_parent->free(this->wake_parent);
	this->wake_child->free(this->wake_child);
	this->lock->free(this->lock);
	free(this);
	return NULL;
}
