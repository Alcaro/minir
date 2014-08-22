#include "minir.h"
#ifdef VIDEO_DDRAW
#include <windows.h>

//http://msdn.microsoft.com/en-us/library/windows/desktop/gg426116%28v=vs.85%29.aspx
//we can not use Ex, because it is not present on a clean installation of XP.

struct video_ddraw {
	struct video i;
	
	
};

static void reinit(struct video * this_, unsigned int screen_width, unsigned int screen_height, unsigned int depth, double fps)
{
	//struct video_ddraw * this=(struct video_ddraw*)this_;
	
	
}

static void draw(struct video * this_, unsigned int width, unsigned int height, const void * data, unsigned int pitch)
{
	//struct video_ddraw * this=(struct video_ddraw*)this_;
	
	
}

static bool set_sync(struct video * this_, bool sync)
{
	//struct video_ddraw * this=(struct video_ddraw*)this_;
	
	return false;
}

static bool has_sync(struct video * this_)
{
	return true;
}

static bool repeat_frame(struct video * this_, unsigned int * width, unsigned int * height,
                                               const void * * data, unsigned int * pitch, unsigned int * bpp)
{
	if (width) *width=0;
	if (height) *height=0;
	if (data) *data=NULL;
	if (pitch) *pitch=0;
	if (bpp) *bpp=16;
	return false;
}

static void free_(struct video * this_)
{
	struct video_ddraw * this=(struct video_ddraw*)this_;
	
	
	
	free(this);
}

struct video * video_create_ddraw(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                   unsigned int depth, double fps)
{
	struct video_ddraw * this=malloc(sizeof(struct video_ddraw));
	this->i.reinit=reinit;
	this->i.draw=draw;
	this->i.set_sync=set_sync;
	this->i.has_sync=has_sync;
	this->i.repeat_frame=repeat_frame;
	this->i.free=free_;
	
	if (true) goto cancel;
	
	return (struct video*)this;
	
cancel:
	free_((struct video*)this);
	return NULL;
}
#endif
