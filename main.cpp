#include "minir.h"
#include "window.h"
#include "file.h"
#include "io.h"
//#include "os.h"
//#include <stddef.h>
//#include <stdlib.h>
//#include <stdarg.h>
//#include <string.h>
//#include <stdio.h>
//#include <ctype.h>
//#include <time.h>

#define VERSION "0.92"

//#define rewind rewind_//go away, stdio
int old_main(int argc, char * argv[]);

namespace {

//TODO: move those out
class dev_kb : public devmgr::device {
	dev_kb(){}
	
	inputkb* core;
	
public:
	dev_kb(inputkb* core) { this->core=core; }
	
	/*private*/ void set_core(inputkb* core)
	{
		delete this->core;
		
		this->core=core;
		attach();
	}
	
	void attach()
	{
		register_events((this->core->features()&inputkb::f_auto ? 0 : devmgr::e_frame), 0);//this can ask for no events - this is fine
		this->core->set_kb_cb(bind_this(&dev_kb::key_cb));
		this->core->refresh();
	}
	
	void ev_frame(devmgr::event::frame* ev)
	{
		this->core->poll();
	}
	
	~dev_kb() { delete this->core; }
	
private:
	/*private*/ void key_cb(unsigned int keyboard, unsigned int scancode, unsigned int libretrocode, bool down)
	{
		devmgr::event::keyboard* ev=new devmgr::event::keyboard();
		ev->secondary = false;
		ev->deviceid = keyboard;
		ev->scancode = scancode;
		ev->libretrocode = libretrocode;
		ev->down = down;
		dispatch_async(ev); // this is sometimes called on the device manager thread, but not always, and it works no matter which thread it is on
	}
};


class dev_video : public devmgr::device {
	dev_video(){}
	
	video* core;
	
public:
	dev_video(video* core) { this->core=core; }
	
	/*private*/ void set_core(video* core)
	{
		delete this->core;
		
		this->core=core;
		attach();
	}
	
	void attach()
	{
		register_events(devmgr::e_video, 0);
	}
	
	void ev_video(devmgr::event::video* ev)
	{
		if (ev->data) this->core->draw_2d(ev->width, ev->height, ev->data, ev->pitch);
		else this->core->draw_repeat();
	}
	
	~dev_video() { delete this->core; }
};


class dev_core : public devmgr::device {
	dev_core(){}
	
	libretro* core;
	
	void c_vid2d_where(unsigned int width, unsigned int height, void* * data, size_t* pitch)
	{
		//TODO (it's currently unused)
	}
	
	void c_vid2d(unsigned int width, unsigned int height, const void* data, size_t pitch)
	{
		devmgr::event::video* ev=new devmgr::event::video;
		ev->secondary=false;
		ev->width=width;
		ev->height=height;
		
		if (data)
		{
			ev->data=malloc(sizeof(uint32_t)*width*height);
			ev->pitch=sizeof(uint32_t)*width;
			
			video::copy_2d((void*)ev->data, sizeof(uint32_t)*width, data, pitch, sizeof(uint32_t)*width, height);
		}
		else
		{
			ev->data=NULL;
			ev->pitch=0;
		}
		
		dispatch(ev);
	}
	
	void c_audio(const int16_t* data, size_t frames)
	{
		
	}
	
public:
	dev_core(libretro* core)
	{
		this->core=core;
		
		this->core->set_video(bind_this(&dev_core::c_vid2d_where), bind_this(&dev_core::c_vid2d));
		this->core->set_audio(bind_this(&dev_core::c_audio));
	}
	
	devtype type() { return t_core; }
	
	void attach()
	{
		register_events(0, devmgr::e_frame | devmgr::e_savestate | devmgr::e_keyboard | devmgr::e_mouse | devmgr::e_gamepad);
		
	}
	
	void ev_frame(devmgr::event::frame* ev)
	{
		core->run();
	}
	
	void ev_state_save(devmgr::event::state_save* ev)
	{
		//TODO
	}
	
	void ev_state_load(devmgr::event::state_load* ev)
	{
		//TODO
	}
	
	//TODO
	void ev_keyboard(devmgr::event::keyboard* ev) {}
	void ev_mousemove(devmgr::event::mousemove* ev) {}
	void ev_mousebutton(devmgr::event::mousebutton* ev) {}
	
	void ev_gamepad(devmgr::event::gamepad* ev)
	{
		core->input_gamepad(ev->device, ev->button, ev->down);
	}
	
	~dev_core() { delete this->core; }
};


const char * inputmap[16]={
	"KB::Z",     // B
	"KB::A",     // Y
	"KB::Space", // Select
	"KB::Return",// Start
	"KB::Up",    // Up
	"KB::Down",  // Down
	"KB::Left",  // Left
	"KB::Right", // Right
	"KB::X",     // A
	"KB::S",     // X
	"KB::Q",     // L
	"KB::W",     // R
	NULL, // L2
	NULL, // R2
	NULL, // L3
	NULL, // R3
};
class dev_inputmap : public devmgr::device {
public:
	void attach()
	{
		for (unsigned int i=0;i<16;i++)
		{
			register_button(inputmap[i], btn_change, i);
		}
	}
	
	void ev_button(devmgr::event::button* bev)
	{
		devmgr::event::gamepad* gev = new devmgr::event::gamepad();
		gev->device = 0;
		gev->button = bev->id;
		gev->down = bev->down;
		gev->secondary = true;
		dispatch(gev);
	}
};


int main_wrap(int argc, char * argv[])
{
puts("init=1");
	window_init(&argc, &argv);
#if !defined(NEW_MAIN) || defined(RELEASE)
return old_main(argc, argv);
#endif
	
#ifdef __linux__
#define HOME "/home/alcaro"
#else
#define HOME "C:/Users/Alcaro"
#endif
	
puts("init=2");
	libretro* core=libretro::create(HOME "/Desktop/minir/roms/gambatte_libretro" DYLIB_EXT, NULL, NULL);
puts("init=3");
	core->load_rom(file::create(HOME "/Desktop/minir/roms/zeldaseasons.gbc"));
puts("init=4");
	
	unsigned int videowidth=320;
	unsigned int videoheight=240;
	videoformat videodepth=fmt_rgb565;
	double videofps=60.0;
	core->get_video_settings(&videowidth, &videoheight, &videodepth, &videofps);
	
	widget_viewport* view;
puts("init=5");
	struct window * wnd=window_create(view=widget_create_viewport(videowidth, videoheight));
	
puts("init=6");
	video* vid=video::drivers[0]->create2d(view->get_window_handle());
	vid->initialize();
	vid->set_source(videowidth, videoheight, videodepth);
	vid->set_vsync(60);
	vid->set_dest_size(videowidth, videoheight);
	
puts("init=7");
	devmgr* contents=devmgr::create();
#ifdef __linux__
	contents->add_device(new dev_kb(inputkb::drivers[1]->create(view->get_window_handle())));
#else
	contents->add_device(new dev_kb(inputkb::drivers[0]->create(view->get_window_handle())));
#endif
	contents->add_device(new dev_video(vid));
	contents->add_device(new dev_core(core));
	contents->add_device(new dev_inputmap());
	
puts("init=8");
	wnd->set_visible(wnd, true);
	
puts("init=9");
	while (wnd->is_visible(wnd))
	{
		window_run_iter();
		contents->frame();
	}
	
	return 0;
}

}

int main(int argc, char * argv[]) { main_wrap(argc, argv); }
