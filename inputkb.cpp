#include "minir.h"
#include <string.h>
#include <stdlib.h>
#include "libretro.h"

#undef this

namespace {

/*
struct inputraw * inputraw_create(const char * backend, uintptr_t windowhandle)
{
#ifdef INPUT_XINPUT2
	if (!strcmp(backend, "XInput2")) return _inputraw_create_xinput2(windowhandle);
#endif
	return NULL;
}

class inputkb_compat : public inputkb {
	struct inputraw * ir;
	
public:
	inputkb_compat(struct inputraw * ir) { this->ir=ir; }
	~inputkb_compat();
	
	uint32_t features() { return ir->feat; }
	void refresh();
	void poll() { refresh(); }
};

void inputkb_compat::refresh()
{
	unsigned char state[1024];
	for (unsigned int i=0;i<this->ir->keyboard_num_keyboards(this->ir);i++)
	{
		memset(state, 0, sizeof(state));
		if (this->ir->keyboard_poll(this->ir, i, state))
		{
			for (unsigned int j=0;j<256;j++)
			{
				this->key_cb(i, j, inputkb_translate_scan(j), state[j]);
			}
		}
	}
}

inputkb_compat::~inputkb_compat()
{
	this->ir->free(this->ir);
}

unsigned int return1(struct inputraw * This) { return 1; }
*/



class inputkb_none : public inputkb {
	~inputkb_none(){}
	uint32_t features() { return 0; }
};
inputkb* inputkb_create_none(uintptr_t windowhandle) { return new inputkb_none(); }
};

const driver_inputkb inputkb_none_desc={ "None", inputkb_create_none, 0 };

extern const driver_inputkb inputkb_rawinput_desc;
extern const driver_inputkb inputkb_udev_desc;
extern const driver_inputkb inputkb_gdk_desc;
extern const driver_inputkb inputkb_x11_xinput2_desc;
extern const driver_inputkb inputkb_directinput_desc;
extern const driver_inputkb inputkb_x11_desc;
extern const driver_inputkb inputkb_none_desc;

const driver_inputkb list_inputkb[]={
#ifdef INPUT_RAWINPUT
	inputkb_rawinput_desc,
#endif
#ifdef INPUT_UDEV
	inputkb_udev_desc,
#endif
#ifdef INPUT_GDK
	inputkb_gdk_desc,
#endif
//#ifdef INPUT_XINPUT2
	//inputkb_x11_xinput2_desc,
//#endif
#ifdef INPUT_DIRECTINPUT
	inputkb_directinput_desc,
#endif
#ifdef INPUT_X11
	inputkb_x11_desc,
#endif
	inputkb_none_desc,
	{}
};

/*
void _inputraw_x11_keyboard_create_shared(struct inputraw * This)
{
	This->keyboard_num_keyboards=return1;
}
void _inputraw_windows_keyboard_create_shared(struct inputraw * This)
{
	_inputraw_x11_keyboard_create_shared(This);
}

inputkb* inputkb_create(const char * backend, uintptr_t windowhandle)
{
	inputkb_translate_init();
#ifdef INPUT_RAWINPUT
	if (!strcmp(backend, "RawInput")) return inputkb_create_rawinput(windowhandle);
#endif
#ifdef INPUT_UDEV
	if (!strcmp(backend, "udev")) return inputkb_create_udev(windowhandle);
#endif
#ifdef INPUT_GDK
	if (!strcmp(backend, "GDK")) return inputkb_create_gdk(windowhandle);
#endif
#ifdef INPUT_DIRECTINPUT
	if (!strcmp(backend, "DirectInput")) return inputkb_create_directinput(windowhandle);
#endif
#ifdef INPUT_X11
	if (!strcmp(backend, "X11")) return inputkb_create_x11(windowhandle);
#endif
	if (!strcmp(backend, "None")) return inputkb_create_none(windowhandle);
	
	struct inputraw * raw=inputraw_create(backend,windowhandle);
	if (!raw) return NULL;
	return new inputkb_compat(raw);
}



//Keyboard driver features (in order of importance to have or not have):
//Having a no-x is negative; x is positive.
//no-inputkb - Uses the legacy driver support. Implies various negative properties. Does not affect order, as it is fixable.
//no-multi - Can not differ between multiple keyboards.
//no-auto - Polls each frame. Not having this means that window_run() handles polling; the driver's poll function is empty.
//direct - Input comes directly from the kernel, as opposed to bouncing through various processes. Not applicable on Windows.
//no-public - Demands escalated privileges to work.
//no-global - Only works while the program is focused.
//initial - Can tell the state of the devices when the driver is initialized. The opposite is only seeing changes.
//no-remote - Can only listen to input from the local machine (as opposed to taking input over X11).
const char * const * inputkb_supported_backends()
{
	static const char * backends[]={
#ifdef INPUT_RAWINPUT
		"RawInput",//(null)
#endif
#ifdef INPUT_UDEV
		"udev",//direct no-public initial no-remote
		       //add no-auto if !defined(WINDOW_GTK3)
#endif
#ifdef INPUT_GDK
		"GDK",//no-global
#endif
#ifdef INPUT_XINPUT2
		"XInput2",//no-auto initial no-inputkb
#endif
#ifdef INPUT_X11
		"X11",//no-multi no-auto initial
#endif
#ifdef INPUT_DIRECTINPUT
		"DirectInput",//no-multi no-auto initial
#endif
		"None",
		NULL
	};
	return backends;
}
*/
