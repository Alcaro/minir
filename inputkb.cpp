#include "minir.h"
#include <string.h>
#include <stdlib.h>
#include "libretro.h"

#undef this

inputkb* inputkb_create_none(uintptr_t windowhandle) { return new inputkb(); }

namespace {

static struct inputraw * inputraw_create(const char * backend, uintptr_t windowhandle)
{
#ifdef INPUT_XINPUT2
	if (!strcmp(backend, "XInput2")) return _inputraw_create_xinput2(windowhandle);
#endif
#ifdef INPUT_X11
	if (!strcmp(backend, "X11")) return _inputraw_create_x11(windowhandle);
#endif
#ifdef INPUT_DIRECTINPUT
	if (!strcmp(backend, "DirectInput")) return _inputraw_create_directinput(windowhandle);
#endif
	return NULL;
}


//terrible spacing - it's temp code, no point wasting time on making it proper
class inputkb_compat : public inputkb {
	struct inputraw * ir;
	function<void(unsigned int keyboard, int scancode, int libretrocode, bool down, bool changed)> key_cb;
	unsigned char state[32][1024];
	
public:
	inputkb_compat(struct inputraw * ir);
	void set_callback(function<void(unsigned int keyboard, int scancode, int libretrocode, bool down, bool changed)> key_cb);
	void poll();
	~inputkb_compat();
};

inputkb_compat::inputkb_compat(struct inputraw * ir)
{
	this->ir=ir;
	memset(this->state, 0, sizeof(this->state));
}

void inputkb_compat::set_callback(function<void(unsigned int keyboard, int scancode, int libretrocode, bool down, bool changed)> key_cb)
{
	this->key_cb=key_cb;
}

void inputkb_compat::poll()
{
	unsigned char newstate[1024];
	memset(newstate, 0, sizeof(newstate));
	for (unsigned int i=0;i<this->ir->keyboard_num_keyboards(this->ir);i++)
	{
		if (!this->ir->keyboard_poll(this->ir, i, newstate)) memset(newstate, 0, sizeof(newstate));
		for (unsigned int j=0;j<1024;j++)
		{
			if (newstate[j]!=this->state[i][j])
			{
				this->state[i][j]=newstate[j];
				this->key_cb(i, j, inputkb_translate_scan(j), this->state[i][j], true);
			}
		}
	}
}

inputkb_compat::~inputkb_compat()
{
	this->ir->free(this->ir);
}

static unsigned int return1(struct inputraw * This) { return 1; }

};

void _inputraw_x11_keyboard_create_shared(struct inputraw * This)
{
	This->keyboard_num_keyboards=return1;
	This->keyboard_num_keys=NULL;
	This->keyboard_get_map=NULL;
}
void _inputraw_windows_keyboard_create_shared(struct inputraw * This)
{
	_inputraw_x11_keyboard_create_shared(This);
}

inputkb* inputkb_create(const char * backend, uintptr_t windowhandle)
{
#ifdef INPUT_RAWINPUT
	if (!strcmp(backend, "RawInput")) return inputkb_create_rawinput(windowhandle);
#endif
#ifdef INPUT_UDEV
	if (!strcmp(backend, "udev")) return inputkb_create_udev(windowhandle);
#endif
#ifdef INPUT_GDK
	if (!strcmp(backend, "GDK")) return inputkb_create_gdk(windowhandle);
#endif
	if (!strcmp(backend, "None")) return inputkb_create_none(windowhandle);
	
	struct inputraw * raw=inputraw_create(backend,windowhandle);
	
	if (!raw) return NULL;
	inputkb_translate_init();
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
		"X11",//no-multi no-auto initial no-inputkb
#endif
#ifdef INPUT_DIRECTINPUT
		"DirectInput",//no-multi no-auto initial no-inputkb
#endif
		"None",
		NULL
	};
	return backends;
}
