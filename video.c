#include "minir.h"
#include <string.h>

#if 0
#include <stdlib.h>
#include<stdio.h>

struct video_pass {
	struct video i;
	struct video * inner;
	uint64_t last;
};

static void reinitP(struct video * this_, unsigned int screen_width, unsigned int screen_height, unsigned int depth, double fps)
{
	struct video_pass * this=(struct video_pass*)this_;
	this->inner->reinit(this->inner, screen_width, screen_height, depth, fps);
}

static void drawP(struct video * this_, unsigned int width, unsigned int height, const void * data, unsigned int pitch)
{
	struct video_pass * this=(struct video_pass*)this_;
	
	uint64_t now=window_get_time();
printf("%i ",now-this->last);
	if (now-this->last < 10) return;
	this->last=now;
	
	this->inner->draw(this->inner, width, height, data, pitch);
}

static void set_syncP(struct video * this_, bool sync)
{
	struct video_pass * this=(struct video_pass*)this_;
	this->inner->set_sync(this->inner, sync);
}

static bool has_syncP(struct video * this_)
{
	struct video_pass * this=(struct video_pass*)this_;
	return this->inner->has_sync(this->inner);
}

static void free_P(struct video * this_)
{
	struct video_pass * this=(struct video_pass*)this_;
	this->inner->free(this->inner);
	free(this);
}

struct video * video_create_pass(struct video * inner)
{
	struct video_pass * this=malloc(sizeof(struct video_pass));
	this->i.reinit=reinitP;
	this->i.draw=drawP;
	this->i.set_sync=set_syncP;
	this->i.has_sync=has_syncP;
	this->i.free=free_P;
	
	this->inner=inner;
	this->last=window_get_time();
	return (struct video*)this;
}
#endif


const char * const * video_supported_backends()
{
	static const char * backends[]={
#ifdef VIDEO_D3D9
		"Direct3D",
#endif
#ifdef VIDEO_OPENGL
		"OpenGL",
#endif
#ifdef VIDEO_GDI
		"GDI",
#endif
#ifdef VIDEO_XSHM
		"XShm",
#endif
		"None",
		NULL
	};
	return backends;
}

static void reinit(struct video * this, unsigned int screen_width, unsigned int screen_height, unsigned int depth, double fps) {}
static void draw(struct video * this, unsigned int width, unsigned int height, const void * data, unsigned int pitch) {}
static void set_sync(struct video * this, bool sync) {}
static bool has_sync(struct video * this) { return false; }
static void free_(struct video * this) {}

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

struct video * video_create_none(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                 unsigned int depth, double fps)
{
	static struct video this={ reinit, draw, set_sync, has_sync, repeat_frame, free_ };
	return &this;
}

struct video * video_create(const char * backend, uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                            unsigned int depth, double fps)
{
#ifdef VIDEO_D3D9
	if (!strcasecmp(backend, "Direct3D")) return video_create_d3d9(windowhandle, screen_width, screen_height, depth, fps);
#endif
#ifdef VIDEO_OPENGL
	if (!strcasecmp(backend, "OpenGL")) return video_create_opengl(windowhandle, screen_width, screen_height, depth, fps);
#endif
#ifdef VIDEO_GDI
	if (!strcasecmp(backend, "GDI")) return video_create_gdi(windowhandle, screen_width, screen_height, depth, fps);
#endif
#ifdef VIDEO_XSHM
	if (!strcasecmp(backend, "XShm")) return video_create_xshm(windowhandle, screen_width, screen_height, depth, fps);
#endif
	if (!strcasecmp(backend, "None")) return video_create_none(windowhandle, screen_width, screen_height, depth, fps);
	return NULL;
}
