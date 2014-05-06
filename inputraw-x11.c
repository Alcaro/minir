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

struct inputraw_x11 {
	struct inputraw i;
	
	Display* display;
	unsigned long windowhandle;
};

static bool keyboard_poll(struct inputraw * this_, unsigned int kb_id, unsigned char * keys)
{
	struct inputraw_x11 * this=(struct inputraw_x11*)this_;
	unsigned char state[32];
	//this one always succeeds, so this check is pointless, but it feels better that way
	//http://cgit.freedesktop.org/xorg/lib/libX11/tree/src/QuKeybd.c
	if (!XQueryKeymap(this->display, (char*)state)) return false;
	
	for (int i=0;i<256;i++)
	{
		int byte=(i>>3);
		int mask=(1<<(i&7));
		
		keys[i]=(state[byte]&mask);
//if(keys[i]&&!keycode_to_libretro_g[i]){
//int keysyms_per_keycode_return;
//KeySym* keysym=XGetKeyboardMapping(this->display, i, 1, &keysyms_per_keycode_return);
//printf("bad keycode=%.2X key=%.4X '%s'\n",i,(unsigned int)keysym[0],XKeysymToString(keysym[0]));
//XFree(keysym);}
	}
	return true;
}

static void free_(struct inputraw * this_)
{
	free(this_);
}

struct inputraw * _inputraw_create_x11(unsigned long windowhandle)
{
	struct inputraw_x11 * this=malloc(sizeof(struct inputraw_x11));
	_inputraw_x11_keyboard_create_shared((struct inputraw*)this);
	//this->i.keyboard_num_keyboards=keyboard_num_keyboards;
	//this->i.keyboard_num_keys=keyboard_num_keys;
	this->i.keyboard_poll=keyboard_poll;
	//this->i.keyboard_get_map=keyboard_get_map;
	this->i.free=free_;
	
	this->display=window_x11_get_display()->display;
	this->windowhandle=windowhandle;
	
	return (struct inputraw*)this;
}
#endif
