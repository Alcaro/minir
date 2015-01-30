#include "io.h"
#include <string.h>
#include <stdlib.h>

#undef this

namespace {
class inputkb_none : public inputkb {
	uint32_t features() { return 0; }
	~inputkb_none(){}
};
inputkb* inputkb_create_none(uintptr_t windowhandle) { return new inputkb_none(); }
};

const inputkb::driver inputkb::driver_none={ "None", inputkb_create_none, 0 };

const inputkb::driver* const inputkb::drivers[]={
#ifdef INPUT_RAWINPUT
	&driver_rawinput,
#endif
#ifdef INPUT_UDEV
	&driver_udev,
#endif
#ifdef INPUT_GDK
	&driver_gdk,
#endif
#ifdef INPUT_XINPUT2
	&driver_xinput2,
#endif
#ifdef INPUT_DIRECTINPUT
	&driver_directinput,
#endif
#ifdef INPUT_X11
	&driver_x11,
#endif
	&driver_none,
	NULL
};
