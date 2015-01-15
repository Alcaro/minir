#include "io.h"

#ifdef INPUTCUR_X11
#include <X11/Xlib.h>

namespace {
class inputcursor_x11 : public inputcursor {
public:

static const uint32_t features = f_outside|f_move|f_background|f_remote|f_public;

//Bool XQueryPointer(Display *display, Window w, Window *root_return, Window *child_return,
// int *root_x_return, int *root_y_return, int *win_x_return, int *win_y_return, unsigned int *mask_return); 
void refresh()
{
	Window ignore1;
	Window ignore2;
	int x;
	int y;
	int ignore3;
	int ignore4;
	unsigned int buttons;
	XQueryPointer(window_x11.display, window_x11.root, &ignore1, &ignore2, &x, &y, &ignore3, &ignore4, &buttons);
	
	this->move_cb(0, x, y);
	this->button_cb(0, button::left, (buttons&Button1Mask));
	this->button_cb(0, button::right, (buttons&Button3Mask));
	this->button_cb(0, button::middle, (buttons&Button2Mask));
}

void poll() { refresh(); }

void move(unsigned int mouse, signed int x, signed int y)
{
	XWarpPointer(window_x11.display, None, window_x11.root, 0,0, 0,0, x,y);
}

};

inputcursor* inputcursor_create_x11(uintptr_t windowhandle) { return new inputcursor_x11(); }

};

const inputcursor::driver inputcursor::driver_x11={ "X11", inputcursor_create_x11, inputcursor_x11::features };
#endif
