#include "minir.h"
#ifdef INPUT_X11
#ifndef WNDPROT_X11
#error Cannot use this driver with this window protocol
#endif
#include <X11/Xlib.h>

namespace {

class inputkb_x11 : public inputkb {
public:
	~inputkb_x11() {}
	
	uint32_t features() { return f_public|f_pollable|f_remote; }
	
	void refresh() { poll(); }
	
	void poll()
	{
		unsigned char state[32];
		//this one always succeeds, so this check is pointless, but it feels better that way
		//http://cgit.freedesktop.org/xorg/lib/libX11/tree/src/QuKeybd.c
		if (!XQueryKeymap(window_x11_get_display()->display, (char*)state)) return;
		for (unsigned int i=0;i<256;i++)
		{
			this->key_cb(0, i, inputkb_translate_scan(i), state[i>>3]&(1<<(i&7)));
		}
	}
};

}

inputkb* inputkb_create_x11(unsigned long windowhandle)
{
	return new inputkb_x11();
}
#endif
