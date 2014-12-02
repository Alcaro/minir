#include "minir.h"
#ifdef VIDEO_DDRAW
#define video cvideo
#undef bind
#include <windows.h>
#define bind BIND_CB

#define this This

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

struct video * cvideo_create_ddraw(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
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

#undef video
static video* video_create_ddraw(uintptr_t windowhandle)
{
	return video_create_compat(cvideo_create_ddraw(windowhandle, 256, 256, 16, 60));
}
extern const driver_video video_ddraw_desc = {"DirectDraw (unimplemented)", video_create_ddraw, NULL, video::f_vsync};
#endif
