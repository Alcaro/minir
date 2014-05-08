#include "minir.h"
#include <string.h>



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
static bool set_sync(struct video * this, bool sync) { return false; }
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
