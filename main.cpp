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

static const enum iotype dev_kb_input[] = { io_user, io_end };
static const enum iotype dev_kb_output[] = { io_thread, io_multi, io_keyboard, io_end };
static const struct devinfo dev_kb_info = { "Keyboard", dev_kb_input, NULL, dev_kb_output, NULL };

class dev_kb : public device {
	inputkb* core;
	
public:
	dev_kb() : device(dev_kb_info), core(NULL) {}
	dev_kb(inputkb* core) : device(dev_kb_info), core(core) { core->set_kb_cb(bind_this(&dev_kb::key_cb)); }
	
	/*private*/ void set_core(inputkb* core)
	{
		delete this->core;
		this->core = core;
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
		emit_button(eid_make(keyboard, code), down);
	}
};


//class dev_video : public minir::device {
//	dev_video(){}
//	
//	video* core;
//	
//public:
//	dev_video(video* core) { this->core=core; }
//	
//	void ev_video(size_t id, unsigned width, unsigned height, const void * data, size_t pitch)
//	{
//		this->core->draw_2d(width, height, data, pitch);
//	}
//	
//	~dev_video() { delete this->core; }
//};


#include "minir.h"
//namespace minir {
static const enum iotype dev_vgamepad_input[] = {
	io_button, io_button, io_button, io_button, io_button, io_button, io_button, io_button,
	io_button, io_button, io_button, io_button, io_button, io_button, io_button, io_button,
	io_end };
static const char * const dev_vgamepad_input_names[] = {
	"Up", "Down", "Left", "Right", "A", "B", "X", "Y",
	"Start", "Select", "L", "R", "L2", "R2", "L3", "R3"
	};
static const enum iotype dev_vgamepad_output[] = { io_gamepad, io_end };
static const struct devinfo dev_vgamepad_info = { "VGamepad", dev_vgamepad_input, dev_vgamepad_input_names, dev_vgamepad_output, NULL };

class dev_vgamepad : public device {
public:
	dev_vgamepad() : device(dev_vgamepad_info) {}
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
		emit_button(eid_make(0, map[eid_id(event)]), down);
	}
};
//}


static const enum iotype dev_core_input[] = { io_frame, io_gamepad, io_end };
static const enum iotype dev_core_output[] = { io_is_core, io_video, io_audio, io_end };
static const struct devinfo dev_core_info = { "Core", dev_core_input, NULL, dev_core_output, NULL };

class dev_core : public device {
	dev_core();
	
	libretro* core;
	
	void c_vid2d_where(unsigned int width, unsigned int height, void* * data, size_t* pitch)
	{
		//TODO (it's currently unused)
	}
	
	void c_vid2d(unsigned int width, unsigned int height, const void* data, size_t pitch)
	{
		//devmgr::event ev(devmgr::event::ty_video);
		//ev.secondary=false;
		//ev.video.width=width;
		//ev.video.height=height;
		//
		//if (data)
		//{
		//	ev.video.data=malloc(sizeof(uint32_t)*width*height);
		//	ev.video.pitch=sizeof(uint32_t)*width;
		//	
		//	video::copy_2d((void*)ev.video.data, sizeof(uint32_t)*width, data, pitch, sizeof(uint32_t)*width, height);
		//}
		//else
		//{
		//	ev.video.data=NULL;
		//	ev.video.pitch=0;
		//}
		//
		//dispatch(ev);
	}
	
	void c_audio(const int16_t* data, size_t frames)
	{
		
	}
	
public:
	dev_core(libretro* core) : device(dev_core_info)
	{
		this->core=core;
		
		this->core->set_video(bind_this(&dev_core::c_vid2d_where), bind_this(&dev_core::c_vid2d));
		this->core->set_audio(bind_this(&dev_core::c_audio));
	}
	
	void ev_frame()
	{
		core->run();
	}
	
	//void ev_state_save(const devmgr::event& ev)
	//{
	//	//TODO
	//}
	
	//void ev_state_load(const devmgr::event& ev)
	//{
	//	//TODO
	//}
	
	//TODO
	void ev_button(uint32_t event, bool down)
	{
		printf("%.8X=%i\n",event,down);
		//core->input_gamepad(ev.gamepad.device, ev.gamepad.button, ev.gamepad.down);
	}
	
	~dev_core() { delete this->core; }
};


void errorhandler(const char * str) { puts(str); }

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
contents->set_error_handler(bind(errorhandler));
puts("init=8");
#ifdef __linux__
#define INPUTKB_ID 1 // skip udev, it requires root
#else
#define INPUTKB_ID 0
#endif
	const char * map[16]={
		"keyboard.up", "keyboard.down", "keyboard.left", "keyboard.right",
		"keyboard.x", "keyboard.z", "keyboard.s", "keyboard.a",
		"keyboard.return", "keyboard.space", "keyboard.q", "keyboard.w",
"vgamepad.aaa+vgamepad.bbb,    vgamepad.ccc+vgamepad.ddd"};
	contents->add_device(new dev_vgamepad(), map);
	contents->add_device(new dev_kb(inputkb::drivers[INPUTKB_ID]->create(view->get_window_handle())));
	contents->add_device(new dev_core(core));
	
puts("init=9");
	contents->map_devices();
	
puts("init=10");
	wnd->set_visible(wnd, true);
	
puts("init=11");
	while (wnd->is_visible(wnd))
	{
		//TODO: contents->frame() on another thread
		window_run_iter();
		contents->frame();
static int i=0;if(i++==2000)break;
	}
	
	return 0;
}

}
}

int main(int argc, char * argv[]) { minir::main_wrap(argc, argv); }
