#include "minir.h"
#ifdef INPUT_X11
#ifndef WNDPROT_X11
#error Cannot use this driver with this window protocol
#endif
#include <stdlib.h>
#include <string.h>
//#include <sys/ipc.h>
//#include <sys/shm.h>
#include <X11/Xlib.h>
//#include <X11/Xutil.h>
//#include <X11/Xatom.h>
//#include <X11/keysym.h>
//#include "libretro.h"

namespace {

class inputkb_x11 : public inputkb {
	unsigned char laststate[32];
	Display* display;
	//unsigned long windowhandle;
	
	function<void(unsigned int keyboard, int scancode, int libretrocode, bool down, bool changed)> key_cb;
	
public:
	inputkb_x11(uintptr_t windowhandle);
	void poll();
	
	void set_callback(function<void(unsigned int keyboard, int scancode, int libretrocode, bool down, bool changed)> key_cb)
	{
		this->key_cb = key_cb;
	}
	~inputkb_x11() {}
};

void inputkb_x11::poll()
{
	unsigned char state[32];
	//this one always succeeds, so this check is pointless, but it feels better that way
	//http://cgit.freedesktop.org/xorg/lib/libX11/tree/src/QuKeybd.c
	if (!XQueryKeymap(this->display, (char*)state)) return;
	for (int i=0;i<256;i++)
	{
		int byte=(i>>3);
		int mask=(1<<(i&7));
		
		if ((state[byte]&mask) != (this->laststate[byte]&mask))
		{
			this->key_cb(0, i, inputkb_translate_scan(i), state[byte]&mask, true);
		}
//if(keys[i]&&!keycode_to_libretro_g[i]){
//int keysyms_per_keycode_return;
//KeySym* keysym=XGetKeyboardMapping(this->display, i, 1, &keysyms_per_keycode_return);
//printf("bad keycode=%.2X key=%.4X '%s'\n",i,(unsigned int)keysym[0],XKeysymToString(keysym[0]));
//XFree(keysym);}
	}
	
	memcpy(this->laststate, state, sizeof(this->laststate));
}

inputkb_x11::inputkb_x11(unsigned long windowhandle)
{
	inputkb_translate_init();
	
	this->display=window_x11_get_display()->display;
	//this->windowhandle=windowhandle;
}

}

inputkb* inputkb_create_x11(unsigned long windowhandle)
{
	return new inputkb_x11(windowhandle);
}
#endif
