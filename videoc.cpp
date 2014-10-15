#include "minir.h"
#define video cvideo
#include <string.h>

struct cvideo * cvideo_create_d3d9(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                   unsigned int depth, double fps);
struct cvideo * cvideo_create_ddraw(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                  unsigned int depth, double fps);
struct cvideo * cvideo_create_gdi(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                unsigned int depth, double fps);
struct cvideo * cvideo_create_xshm(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                 unsigned int depth, double fps);

struct video * cvideo_create(const char * backend, uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                            unsigned int depth, double fps)
{
#ifdef VIDEO_D3D9
	if (!strcasecmp(backend, "Direct3D")) return cvideo_create_d3d9(windowhandle, screen_width, screen_height, depth, fps);
#endif
#ifdef VIDEO_DDRAW
	if (!strcasecmp(backend, "DirectDraw")) return cvideo_create_ddraw(windowhandle, screen_width, screen_height, depth, fps);
#endif
#ifdef VIDEO_GDI
	if (!strcasecmp(backend, "GDI")) return cvideo_create_gdi(windowhandle, screen_width, screen_height, depth, fps);
#endif
#ifdef VIDEO_XSHM
	if (!strcasecmp(backend, "XShm")) return cvideo_create_xshm(windowhandle, screen_width, screen_height, depth, fps);
#endif
	return NULL;
}
