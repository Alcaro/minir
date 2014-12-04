#include "io.h"
#include <string.h>
#include <stdlib.h>

#undef this

namespace {
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
extern const driver_inputkb inputkb_xinput2_desc;
extern const driver_inputkb inputkb_directinput_desc;
extern const driver_inputkb inputkb_x11_desc;
extern const driver_inputkb inputkb_none_desc;

const driver_inputkb * list_inputkb[]={
#ifdef INPUT_RAWINPUT
	&inputkb_rawinput_desc,
#endif
#ifdef INPUT_UDEV
	&inputkb_udev_desc,
#endif
#ifdef INPUT_GDK
	&inputkb_gdk_desc,
#endif
#ifdef INPUT_XINPUT2
	&inputkb_xinput2_desc,
#endif
#ifdef INPUT_DIRECTINPUT
	&inputkb_directinput_desc,
#endif
#ifdef INPUT_X11
	&inputkb_x11_desc,
#endif
	&inputkb_none_desc,
	NULL
};
