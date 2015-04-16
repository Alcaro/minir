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

namespace minir {
namespace {

//TODO: move those out
class dev_kb : public minir::device {
	inputkb* core;
	
public:
	void ev_init(uintptr_t windowhandle)
	{
		emit_thread_enable();
		core = inputkb::drivers[
#ifdef __linux__
1
#else
0
#endif
			]->create(windowhandle);
		core->set_kb_cb(bind_this(&dev_kb::key_cb));
	}
	
	void ev_frame()
	{
		this->core->poll();
	}
	
	~dev_kb() { delete this->core; }
	
private:
	/*private*/ void key_cb(unsigned int keyboard, unsigned int scancode, unsigned int libretrocode, bool down)
	{
		unsigned int code;
		if (libretrocode) code = libretrocode;
		else code = scancode+1024;
		emit_button(EV_MAKE(keyboard, code), down);
		//devmgr::event ev(devmgr::event::ty_keyboard);
		//ev.secondary = false;
		//ev.keyboard.deviceid = keyboard;
		//ev.keyboard.scancode = scancode;
		//ev.keyboard.libretrocode = libretrocode;
		//ev.keyboard.down = down;
		//dispatch_async(ev); // this is sometimes called on the device manager thread, but not always, and it works no matter which thread it is on
	}
};
declare_devinfo(kb, "Keyboard", (io_user), (io_multi, io_keyboard), 0);


class dev_video : public minir::device {
	dev_video(){}
	
	video* core;
	
public:
	dev_video(video* core) { this->core=core; }
	
	void ev_video(size_t id, unsigned width, unsigned height, const void * data, size_t pitch)
	{
		this->core->draw_2d(width, height, data, pitch);
	}
	
	~dev_video() { delete this->core; }
};


//class dev_core : public devmgr::device {
//	dev_core(){}
	
//	libretro* core;
	
//	void c_vid2d_where(unsigned int width, unsigned int height, void* * data, size_t* pitch)
//	{
		//TODO (it's currently unused)
//	}
	
//	void c_vid2d(unsigned int width, unsigned int height, const void* data, size_t pitch)
//	{
//		devmgr::event ev(devmgr::event::ty_video);
//		ev.secondary=false;
//		ev.video.width=width;
//		ev.video.height=height;
		
//		if (data)
//		{
//			ev.video.data=malloc(sizeof(uint32_t)*width*height);
//			ev.video.pitch=sizeof(uint32_t)*width;
			
//			video::copy_2d((void*)ev.video.data, sizeof(uint32_t)*width, data, pitch, sizeof(uint32_t)*width, height);
//		}
//		else
//		{
//			ev.video.data=NULL;
//			ev.video.pitch=0;
//		}
		
//		dispatch(ev);
//	}
	
//	void c_audio(const int16_t* data, size_t frames)
//	{
		
//	}
	
//public:
//	dev_core(libretro* core)
//	{
//		this->core=core;
		
//		this->core->set_video(bind_this(&dev_core::c_vid2d_where), bind_this(&dev_core::c_vid2d));
//		this->core->set_audio(bind_this(&dev_core::c_audio));
//	}
	
//	devtype type() { return t_core; }
	
//	void attach()
//	{
//		register_events(0, devmgr::e_frame | devmgr::e_savestate | devmgr::e_keyboard | devmgr::e_mouse | devmgr::e_gamepad);
		
//	}
	
//	void ev_frame(const devmgr::event& ev)
//	{
//		core->run();
//	}
	
//	void ev_state_save(const devmgr::event& ev)
//	{
		//TODO
//	}
	
//	void ev_state_load(const devmgr::event& ev)
//	{
		//TODO
//	}
	
	//TODO
//	void ev_keyboard(const devmgr::event& ev) {}
//	void ev_mousemove(const devmgr::event& ev) {}
//	void ev_mousebutton(const devmgr::event& ev) {}
	
//	void ev_gamepad(const devmgr::event& ev)
//	{
//		core->input_gamepad(ev.gamepad.device, ev.gamepad.button, ev.gamepad.down);
//	}
	
//	~dev_core() { delete this->core; }
//};


#include "minir.h"
class dev_gamepad : public minir::device {
public:
	void ev_button(uint32_t event, bool down)
	{
		static const uint8_t map[16]={
/* RETRO_DEVICE_ID_JOYPAD_UP     */  4,
/* RETRO_DEVICE_ID_JOYPAD_DOWN   */  5,
/* RETRO_DEVICE_ID_JOYPAD_LEFT   */  6,
/* RETRO_DEVICE_ID_JOYPAD_RIGHT  */  7,
/* RETRO_DEVICE_ID_JOYPAD_A      */  8,
/* RETRO_DEVICE_ID_JOYPAD_B      */  0,
/* RETRO_DEVICE_ID_JOYPAD_X      */  9,
/* RETRO_DEVICE_ID_JOYPAD_Y      */  1,
/* RETRO_DEVICE_ID_JOYPAD_START  */  3,
/* RETRO_DEVICE_ID_JOYPAD_SELECT */  2,
/* RETRO_DEVICE_ID_JOYPAD_L      */ 10,
/* RETRO_DEVICE_ID_JOYPAD_R      */ 11,
/* RETRO_DEVICE_ID_JOYPAD_L2     */ 12,
/* RETRO_DEVICE_ID_JOYPAD_R2     */ 13,
/* RETRO_DEVICE_ID_JOYPAD_L3     */ 14,
/* RETRO_DEVICE_ID_JOYPAD_R3     */ 15,
			};
		emit_button(EV_MAKE(0, map[EV_ID(event)]), down);
	}
};
declare_devinfo_n(gamepad, "Gamepad",
	(io_button, io_button, io_button, io_button, io_button, io_button, io_button, io_button,
	 io_button, io_button, io_button, io_button, io_button, io_button, io_button, io_button),
	("Up", "Down", "Left", "Right", "A", "B", "X", "Y", "Start", "Select", "L", "R", "L2", "R2", "L3", "R3"),
	(io_gamepad), (NULL),
	0);

/*
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
	
	void ev_button(const devmgr::event& bev)
	{
		devmgr::event gev(devmgr::event::ty_gamepad);
		gev.secondary = true;
		gev.gamepad.device = 0;
		gev.gamepad.button = bev.button.id;
		gev.gamepad.down = bev.button.down;
		dispatch(gev);
	}
};
*/


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
}

int main(int argc, char * argv[]) { minir::main_wrap(argc, argv); }
