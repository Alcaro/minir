#include "io.h"
#ifdef VIDEO_XSHM
#define video cvideo
#include <stdlib.h>
//#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

//force some year-old C code to compile properly as C++ - I decided to switch long ago but still haven't finished.
#define this This

//this file is slightly based on ruby by byuu

struct video_xshm {
	struct video i;
	
	Display* display;
	int screen;
	
	Window parentwindow;
	Window wndw;
	
	unsigned int width;
	unsigned int height;
	unsigned int pitch;
	videoformat bpp;
	
	XShmSegmentInfo shmInfo;
	XImage* image;
	GC gc;
};

static void reset(struct video_xshm * this)
{
	XFreeGC(this->display, this->gc);
	XShmDetach(this->display, &this->shmInfo);
	XDestroyImage(this->image);
	shmdt(this->shmInfo.shmaddr);
	shmctl(this->shmInfo.shmid, IPC_RMID, 0);
	XDestroyWindow(this->display, this->wndw);
}

static void reinit(struct video * this_, unsigned int screen_width, unsigned int screen_height, videoformat depth, double fps)
{
	struct video_xshm * this=(struct video_xshm*)this_;
	
	if (this->wndw) reset(this);
	
	this->width=screen_width;
	this->height=screen_height;
	this->pitch=sizeof(uint32_t)*screen_width;
	this->bpp=depth;
	
	XSetWindowAttributes attributes;
	attributes.border_pixel=0;
	this->wndw=XCreateWindow(this->display, this->parentwindow, 0, 0, screen_width, screen_height,
	                           0, 24, CopyFromParent, NULL, CWBorderPixel, &attributes);
	XSetWindowBackground(this->display, this->wndw, 0);
	XMapWindow(this->display, this->wndw);
	
	this->shmInfo.shmid=shmget(IPC_PRIVATE, this->pitch*screen_height,
	                           IPC_CREAT|0777);
	if (this->shmInfo.shmid<0) abort();//seems like an out of memory situation... let's just blow up
	
	this->shmInfo.shmaddr=(char*)shmat(this->shmInfo.shmid, 0, 0);
	this->shmInfo.readOnly=False;
	XShmAttach(this->display, &this->shmInfo);
	XSync(this->display,False);//no idea why this is required, but I get weird errors without it
	this->image=XShmCreateImage(this->display, NULL, 24, ZPixmap,
	                            this->shmInfo.shmaddr, &this->shmInfo, screen_width, screen_height);
	
	this->gc=XCreateGC(this->display, this->wndw, 0, NULL);
}

static void draw(struct video * this_, unsigned int width, unsigned int height, const void * data, unsigned int pitch)
{
	struct video_xshm * this=(struct video_xshm*)this_;
	if (!data) return;
	
	struct image src;
	src.width=width;
	src.height=height;
	src.pixels=(void*)data;
	src.pitch=pitch;
	src.format=this->bpp;
	
	struct image dst;
	dst.width=this->width;
	dst.height=this->height;
	dst.pixels=this->shmInfo.shmaddr;
	dst.pitch=this->pitch;
	dst.format=fmt_xrgb8888;
	image_convert_resize(&src, &dst);
	
	XShmPutImage(this->display, this->wndw, this->gc, this->image,
	             0, 0, 0, 0, this->width, this->height, False);
	XFlush(this->display);
}

static bool set_sync(struct video * this_, bool sync)
{
	return false;
}

static bool has_sync(struct video * this_)
{
	return false;
}

static void free_(struct video * this_)
{
	struct video_xshm * this=(struct video_xshm*)this_;
	reset(this);
	free(this);
}

static struct video * cvideo_create_xshm(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                 videoformat depth, double fps)
{
	struct video_xshm * this=malloc(sizeof(struct video_xshm));
	this->i.reinit=reinit;
	this->i.draw=draw;
	this->i.set_sync=set_sync;
	this->i.has_sync=has_sync;
	this->i.free=free_;
	
	this->display=window_x11.display;
	this->screen=window_x11.screen;
	
	if (!XShmQueryExtension(this->display)) goto cancel;
	
	this->parentwindow=windowhandle;
	this->wndw=0;
	
	reinit((struct video*)this, screen_width, screen_height, depth, fps);
	
	return (struct video*)this;
	
cancel:
	free_((struct video*)this);
	return NULL;
}

#undef video
static video* video_create_xshm(uintptr_t windowhandle)
{
	return video_create_compat(cvideo_create_xshm(windowhandle, 256, 256, fmt_xrgb1555, 60));
}
const video::driver video::driver_xshm = {"XShm", video_create_xshm, NULL, 0};
#endif
