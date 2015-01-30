#include "minir.h"
#include "window.h"
//#include "file.h"
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
	dev_kb() { this->core=NULL; }
	
	uint32_t features() { return 0; }
	
	/*private*/ void set_core(inputkb* core)
	{
		delete this->core;
		
		this->core=core;
		this->core->set_kb_cb(bind_this(&dev_kb::key_cb));
		this->core->refresh();
		register_events((this->core->features()&inputkb::f_auto ? devmgr::e_frame : 0), 0);//asking for no events is fine
	}
	
	void init()
	{
		if (this->core) this->core->refresh();
	}
	
	void ev_frame(devmgr::event::frame* ev)
	{
		if (this->core) this->core->poll();
	}
	
	~dev_kb() { delete this->core; }
};

class dev_core : public devmgr::device {
	
};

int main_wrap(int argc, char * argv[])
{
	window_init(&argc, &argv);
return old_main(argc, argv);
	
	class g : public devmgr::device {
	public:
		uint32_t features() { return 0; }
		void init() { register_events(devmgr::e_frame, devmgr::e_frame); }
		//void ev_frame(devmgr::event::frame* ev) { printf("FRAME%i\n",ev->secondary); }
	};
	class gp : public devmgr::device {
	public:
		uint32_t features() { return f_primary; }
		void init() { register_events(devmgr::e_frame|devmgr::e_keyboard, devmgr::e_frame); }
		//void ev_frame(devmgr::event::frame* ev) { printf("PFRAME%i\n",ev->secondary); }
		void ev_keyboard(devmgr::event::keyboard* ev) { printf("KB%i\n",ev->scancode); }
	};
	
	devmgr* contents=devmgr::create();
	contents->add_device(new g);
	contents->add_device(new gp);
	contents->add_device(new g);
	
	dev_kb* kb=new dev_kb();
	contents->add_device(kb);
	
	kb->set_core(inputkb::drivers[1]->create(0));
	
	while (true)
	{
		window_run_iter();
		contents->frame();
		thread_sleep(100*1000);
	}
	
	return 0;
}

}

int main(int argc, char * argv[]) { main_wrap(argc, argv); }
