#include "minir.h"
#include <string.h>

namespace {
class video_none : public video {
	uint32_t features() { return 0; }
	
	void finalize_2d(unsigned int base_width, unsigned int base_height, unsigned int depth) {}
	void draw_2d(unsigned int width, unsigned int height, const void * data, unsigned int pitch) {}
	void draw_repeat() {}
	void get_base_size(unsigned int * width, unsigned int * height) { *width=1; *height=1; }
	void set_size(unsigned int width, unsigned int height) {}
	~video_none() {}
};
video* video_create_none(uintptr_t windowhandle) { return new video_none(); }
};

const driver_video video_none_desc={ "None", video_create_none, NULL, 0 };


namespace {
class video_compat : public video {
	function<cvideo*(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
	                 unsigned int depth, double fps)> create;
	cvideo* child;
	
	uintptr_t windowhandle;
	
	unsigned int depth;
	unsigned int base_width;
	unsigned int base_height;
	
	unsigned int last_width;
	unsigned int last_height;
	
public:
	
	uint32_t features()
	{
		uint32_t ret=0;
		if (child->has_sync(child)) ret|=f_vsync;
		return ret;
	}
	
	void finalize_2d(unsigned int base_width, unsigned int base_height, unsigned int depth)
	{
		this->base_width=base_width;
		this->base_height=base_height;
		this->child=this->create(this->windowhandle, base_width, base_height, depth, 60);
		this->depth=depth;
		//failure cannot be handled at this point
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
	
	void get_base_size(unsigned int * width, unsigned int * height)
	{
		*width=this->base_width;
		*height=this->base_height;
	}
	
	void set_size(unsigned int width, unsigned int height)
	{
		this->child->reinit(this->child, width, height, this->depth, 60);
	}
	
	void set_vsync(double fps)
	{
		this->child->set_sync(this->child, fps!=0);
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
extern const driver_video video_opengl_old_desc;

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
#ifdef VIDEO_OPENGL
	video_opengl_old_desc,
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
