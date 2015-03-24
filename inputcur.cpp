#include "io.h"
#include <string.h>
#include <stdlib.h>

#undef this

namespace {
class inputcursor_none : public inputcursor {
	void refresh() {}
	~inputcursor_none(){}
};
inputcursor* inputcursor_create_none(uintptr_t windowhandle) { return new inputcursor_none(); }
};

const inputcursor::driver inputcursor::driver_none={ "None", inputcursor_create_none, 0 };

const inputcursor::driver* const inputcursor::drivers[]={
#ifdef INPUTCUR_XRECORD
	&driver_xrecord,
#endif
#ifdef INPUTCUR_RAWINPUT
	&driver_rawinput,
#endif
#ifdef INPUTCUR_X11
	&driver_x11,
#endif
#ifdef INPUTCUR_WIN32
	&driver_w32msg,
#endif
	&driver_none,
	NULL
};
