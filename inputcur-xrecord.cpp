#include "io.h"

#ifdef INPUTCUR_XRECORD
namespace {
class inputcursor_xrecord : public inputcursor {
public:

bool construct(uintptr_t windowhandle) { return false; }
void refresh() {}
void poll() {}

void move(unsigned int mouse, signed int x, signed int y)
{
	XWarpPointer(window_x11.display, None, window_x11.root, 0,0, 0,0, x,y);
}

~inputcursor_xrecord(){}

};
inputcursor* inputcursor_create_xrecord(uintptr_t windowhandle)
{
	inputcursor_xrecord* ret=new inputcursor_xrecord;
	if (!ret->construct(windowhandle))
	{
		delete ret;
		return NULL;
	}
	return ret;
}

};

const inputcursor::driver inputcursor::driver_xrecord={ "XRecord", inputcursor_create_xrecord, 0 };
#endif
