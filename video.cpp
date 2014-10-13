#include "minir.h"
#include <string.h>

namespace {
class video_none : public video {
	uint32_t features() { return 0; }
	void set_input_2d(unsigned int depth, double fps) {}
	void draw_2d(unsigned int width, unsigned int height, const void * data, unsigned int pitch) {}
	void draw_repeat() {}
	~video_none() {}
};
video* video_create_none(uintptr_t windowhandle) { return new video_none(); }
};

const driver_video video_none_desc={ "None", video_create_none, 0 };


namespace {
class video_compat : public video {
	function<cvideo*(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                   unsigned int depth, double fps)> create;
	cvideo* child;
	
	unsigned int screen_width;
	unsigned int screen_height;
	uintptr_t windowhandle;
	
	unsigned int last_width;
	unsigned int last_height;
	
public:
	
	uint32_t features()
	{
		uint32_t ret=0;
		if (child->has_sync(child)) ret|=f_vsync;
		return ret;
	}
	
	void set_input_2d(unsigned int depth, double fps)
	{
		this->child=this->create(this->windowhandle, this->screen_width, this->screen_height, depth, fps);
	}
	
	void draw_2d(unsigned int width, unsigned int height, const void * data, unsigned int pitch)
	{
		this->last_width=width;
		this->last_height=height;
		child->draw(child, width, height, data, pitch);
	}
	
	void draw_repeat()
	{
		child->draw(child, this->last_width, this->last_height, NULL, 0);
	}
	
	void set_output(unsigned int screen_width, unsigned int screen_height)
	{
		this->screen_width=screen_width;
		this->screen_height=screen_height;
	}
	
	void set_vsync(bool sync)
	{
		this->child->set_sync(this->child, sync);
	}
	
	~video_compat() { child->free(child); }
	
	video_compat(function<cvideo*(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                   unsigned int depth, double fps)> create, uintptr_t windowhandle)
	{
		this->create=create;
		this->windowhandle=windowhandle;
	}
};

}

video* video_create_compat(function<cvideo*(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                   unsigned int depth, double fps)> create, uintptr_t windowhandle)
{
	return new video_compat(create, windowhandle);
}

extern const driver_video video_d3d9_desc;
extern const driver_video video_ddraw_desc;
extern const driver_video video_opengl_desc;
extern const driver_video video_gdi_desc;
extern const driver_video video_xshm_desc;
extern const driver_video video_none_desc;

const driver_video list_video[]={
#ifdef VIDEO_D3D9
	//video_d3d9_desc,
#endif
#ifdef VIDEO_DDRAW
	//video_ddraw_desc,
#endif
#ifdef VIDEO_OPENGL
	video_opengl_desc,
#endif
#ifdef VIDEO_GDI
	video_gdi_desc,
#endif
#ifdef VIDEO_XSHM
	video_xshm_desc,
#endif
	video_none_desc,
	{}
};
