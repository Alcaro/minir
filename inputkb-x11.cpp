#include "io.h"
#ifdef INPUT_X11
#ifndef WNDPROT_X11
#error Cannot use this driver with this window protocol
#endif
#include <X11/Xlib.h>

namespace {
class inputkb_x11 : public inputkb {
public:
	static const uint32_t features = f_public|f_pollable|f_remote;
	
	void refresh() { poll(); }
	void poll()
	{
		uint8_t state[32];
		//this one always succeeds, so this check is pointless, but it feels better that way
		//http://cgit.freedesktop.org/xorg/lib/libX11/tree/src/QuKeybd.c
		if (!XQueryKeymap(window_x11.display, (char*)state)) return;
		for (unsigned int i=0;i<256;i++)
		{
			this->key_cb(0, i, translate_scan(i), state[i>>3]&(1<<(i&7)));
		}
	}
	
	static inputkb* create(uintptr_t windowhandle)
	{
		return new inputkb_x11();
	}
	
	~inputkb_x11() {}
};

}

const inputkb::driver inputkb::driver_x11={ "X11", inputkb_x11::create, inputkb_x11::features };
#endif
