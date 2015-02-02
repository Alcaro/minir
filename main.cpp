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

class dev_kb : public devmgr::device {
	dev_kb(){}
	
	inputkb* core;
	
	/*private*/ void key_cb(unsigned int keyboard, unsigned int scancode, unsigned int libretrocode, bool down)
	{
		devmgr::event::keyboard* ev=new devmgr::event::keyboard();
		ev->secondary=false;
		ev->deviceid=keyboard;
		ev->scancode=scancode;
		ev->libretrocode=libretrocode;
		ev->down=down;
		dispatch(ev);
	}
	
public:
	dev_kb(inputkb* core) { this->core=core; }
	
	uint32_t features() { return 0; }
	
	/*private*/ void set_core(inputkb* core)
	{
		delete this->core;
		
		this->core=core;
		init();
	}
	
	void init()
	{
		register_events((this->core->features()&inputkb::f_auto ? 0 : devmgr::e_frame), 0);//this can ask for no events - this is fine
		this->core->set_kb_cb(bind_this(key_cb));
		this->core->refresh();
	}
	
	void ev_frame(devmgr::event::frame* ev)
	{
		if (this->core) this->core->poll();
	}
	
	~dev_kb() { delete this->core; }
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
		if (ev->data) printf("%.8X\n",*(uint32_t*)ev->data);
		else printf("--------\n");
	}
	
	void c_audio(const int16_t* data, size_t frames)
	{
		
	}
	
public:
	dev_core(libretro* core)
	{
		this->core=core;
		
		this->core->set_video(bind_this(c_vid2d_where), bind_this(c_vid2d));
		this->core->set_audio(bind_this(c_audio));
	}
	
	devtype type() { return t_core; }
	
	void init()
	{
		puts("W");
		register_events(0, devmgr::e_frame | devmgr::e_savestate | devmgr::e_keyboard | devmgr::e_mouse | devmgr::e_gamepad);
	}
	
	virtual void ev_frame(devmgr::event::frame* ev)
	{
		puts("A");
		core->run();
	}
	
	virtual void ev_state_save(devmgr::event::state_save* ev)
	{
		//TODO
	}
	
	virtual void ev_state_load(devmgr::event::state_load* ev)
	{
		//TODO
	}
	
	//TODO
	virtual void ev_keyboard(devmgr::event::keyboard* ev) {}
	virtual void ev_mousemove(devmgr::event::mousemove* ev) {}
	virtual void ev_mousebutton(devmgr::event::mousebutton* ev) {}
	
	virtual void ev_gamepad(devmgr::event::gamepad* ev)
	{
		core->input_gamepad(ev->device, ev->button, ev->down);
	}
	
	~dev_core() { delete this->core; }
};

int main_wrap(int argc, char * argv[])
{
	window_init(&argc, &argv);
return old_main(argc, argv);
	
	class g : public devmgr::device {
	public:
		void init() { register_events(devmgr::e_frame, devmgr::e_frame); }
		//void ev_frame(devmgr::event::frame* ev) { printf("FRAME%i\n",ev->secondary); }
	};
	class gp : public devmgr::device {
	public:
		devtype type() { return t_primary; }
		void init() { register_events(devmgr::e_frame|devmgr::e_keyboard, devmgr::e_frame); }
		//void ev_frame(devmgr::event::frame* ev) { printf("PFRAME%i\n",ev->secondary); }
		void ev_keyboard(devmgr::event::keyboard* ev) { printf("KB%i\n",ev->scancode); }
	};
	
	devmgr* contents=devmgr::create();
	contents->add_device(new g);
	contents->add_device(new gp);
	contents->add_device(new g);
	
	dev_kb* kb=new dev_kb(inputkb::drivers[0]->create(0));
	contents->add_device(kb);
	
	libretro* core=libretro::create("C:/Users/Alcaro/Desktop/minir/roms/gambatte_libretro.dll", NULL, NULL);
	core->load_rom(file::create("C:/Users/Alcaro/Desktop/minir/roms/zeldaseasons.gbc"));
	dev_core* coremgr=new dev_core(core);
	contents->add_device(coremgr);
	
	while (true)
	{
		window_run_iter();
		contents->frame();
		thread_sleep(20*1000);
	}
	
	return 0;
}

}

int main(int argc, char * argv[]) { main_wrap(argc, argv); }
