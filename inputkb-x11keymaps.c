#include "minir.h"
#ifdef WNDPROT_X11
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include "libretro.h"

//paragraph, the one left of 1, doesn't map to libretro

static bool initialized=false;
static int libretrofor[256];

struct {
	uint16_t libretro;
	uint16_t xkey;
} const map[]={
	{ RETROK_BACKSPACE, XK_BackSpace }, { RETROK_TAB, XK_Tab }, { RETROK_CLEAR, XK_Clear },
	{ RETROK_RETURN, XK_Return }, { RETROK_PAUSE, XK_Pause }, { RETROK_ESCAPE, XK_Escape },
	{ RETROK_SPACE, XK_space }, { RETROK_EXCLAIM, XK_exclam }, { RETROK_QUOTEDBL, XK_quotedbl },
	{ RETROK_HASH, XK_numbersign }, { RETROK_DOLLAR, XK_dollar }, { RETROK_AMPERSAND, XK_ampersand },
	{ RETROK_QUOTE, XK_apostrophe }, { RETROK_LEFTPAREN, XK_parenleft }, { RETROK_RIGHTPAREN, XK_parenright },
	{ RETROK_ASTERISK, XK_asterisk }, { RETROK_PLUS, XK_plus }, { RETROK_COMMA, XK_comma },
	{ RETROK_MINUS, XK_minus }, { RETROK_PERIOD, XK_period }, { RETROK_SLASH, XK_slash },
	{ RETROK_0, XK_0 }, { RETROK_1, XK_1 }, { RETROK_2, XK_2 },
	{ RETROK_3, XK_3 }, { RETROK_4, XK_4 }, { RETROK_5, XK_5 },
	{ RETROK_6, XK_6 }, { RETROK_7, XK_7 }, { RETROK_8, XK_8 },
	{ RETROK_9, XK_9 },
	{ RETROK_COLON, XK_colon }, { RETROK_SEMICOLON, XK_semicolon }, { RETROK_LESS, XK_less },
	{ RETROK_EQUALS, XK_equal }, { RETROK_GREATER, XK_greater }, { RETROK_QUESTION, XK_question },
	{ RETROK_AT, XK_at }, { RETROK_LEFTBRACKET, XK_bracketleft }, { RETROK_BACKSLASH, XK_backslash },
	{ RETROK_RIGHTBRACKET, XK_bracketright }, { RETROK_CARET, XK_asciicircum }, { RETROK_UNDERSCORE, XK_underscore },
	{ RETROK_BACKQUOTE, XK_grave }, { RETROK_BACKQUOTE, XK_dead_acute },
	{ RETROK_a, XK_a }, { RETROK_b, XK_b }, { RETROK_c, XK_c }, { RETROK_d, XK_d },
	{ RETROK_e, XK_e }, { RETROK_f, XK_f }, { RETROK_g, XK_g }, { RETROK_h, XK_h },
	{ RETROK_i, XK_i }, { RETROK_j, XK_j }, { RETROK_k, XK_k }, { RETROK_l, XK_l },
	{ RETROK_m, XK_m }, { RETROK_n, XK_n }, { RETROK_o, XK_o }, { RETROK_p, XK_p },
	{ RETROK_q, XK_q }, { RETROK_r, XK_r }, { RETROK_s, XK_s }, { RETROK_t, XK_t },
	{ RETROK_u, XK_u }, { RETROK_v, XK_v }, { RETROK_w, XK_w }, { RETROK_x, XK_x },
	{ RETROK_y, XK_y }, { RETROK_z, XK_z },
	{ RETROK_DELETE, XK_Delete },
	
	{ RETROK_KP0, XK_KP_0 }, { RETROK_KP1, XK_KP_1 }, { RETROK_KP2, XK_KP_2 },
	{ RETROK_KP3, XK_KP_3 }, { RETROK_KP4, XK_KP_4 }, { RETROK_KP5, XK_KP_5 },
	{ RETROK_KP6, XK_KP_6 }, { RETROK_KP7, XK_KP_7 }, { RETROK_KP8, XK_KP_8 },
	{ RETROK_KP9, XK_KP_9 },
	{ RETROK_KP_PERIOD, XK_KP_Separator }, { RETROK_KP_DIVIDE, XK_KP_Divide },
	{ RETROK_KP_MULTIPLY, XK_KP_Multiply }, { RETROK_KP_MINUS, XK_KP_Subtract },
	{ RETROK_KP_PLUS, XK_KP_Add }, { RETROK_KP_ENTER, XK_KP_Enter },
	{ RETROK_KP_EQUALS, XK_KP_Equal },
	
	{ RETROK_UP, XK_Up }, { RETROK_DOWN, XK_Down }, { RETROK_RIGHT, XK_Right }, { RETROK_LEFT, XK_Left },
	{ RETROK_INSERT, XK_Insert },//Delete is earlier
	{ RETROK_HOME, XK_Home }, { RETROK_END, XK_End },
	{ RETROK_PAGEUP, XK_Page_Up }, { RETROK_PAGEDOWN, XK_Page_Down },
	
	{ RETROK_F1, XK_F1 },   { RETROK_F2, XK_F2 },   { RETROK_F3, XK_F3 },   { RETROK_F4, XK_F4 },
	{ RETROK_F5, XK_F5 },   { RETROK_F6, XK_F6 },   { RETROK_F7, XK_F7 },   { RETROK_F8, XK_F8 },
	{ RETROK_F9, XK_F9 },   { RETROK_F10, XK_F10 }, { RETROK_F11, XK_F11 }, { RETROK_F12, XK_F12 },
	{ RETROK_F13, XK_F13 }, { RETROK_F14, XK_F14 }, { RETROK_F15, XK_F15 },
	
	{ RETROK_NUMLOCK, XK_Num_Lock }, { RETROK_CAPSLOCK, XK_Caps_Lock }, { RETROK_SCROLLOCK, XK_Scroll_Lock },
	{ RETROK_RSHIFT, XK_Shift_R }, { RETROK_LSHIFT, XK_Shift_L },
	{ RETROK_RCTRL, XK_Control_R }, { RETROK_LCTRL, XK_Control_L },
	{ RETROK_RALT, XK_Alt_R }, { RETROK_LALT, XK_Alt_L },
	{ RETROK_RMETA, XK_Meta_R }, { RETROK_LMETA, XK_Meta_L },
	{ RETROK_LSUPER, XK_Super_L }, { RETROK_RSUPER, XK_Super_R },
	{ RETROK_MODE, XK_Mode_switch }, { RETROK_COMPOSE, XK_Multi_key },
	
	{ RETROK_HELP, XK_Help }, { RETROK_PRINT, XK_Print }, { RETROK_SYSREQ, XK_Sys_Req },
	{ RETROK_BREAK, XK_Break }, { RETROK_MENU, XK_Menu }, //{ RETROK_POWER, x },
	{ RETROK_EURO, XK_EuroSign }, { RETROK_UNDO, XK_Undo },
	
	{ RETROK_RALT, XK_ISO_Level3_Shift },//AltGr
};

void inputkb_x11_translate_init()
{
	if (initialized) return;
	
	Display* display=window_x11_get_display()->display;
	
	for (unsigned int i=0;i<sizeof(libretrofor)/sizeof(*libretrofor);i++) libretrofor[i]=-1;
	
//printf("HI=%.4X\n",XKeycodeToKeysym(display,0x6c,0));
	unsigned int i=sizeof(map)/sizeof(*map);
	do {
		i--;
		int keycode=XKeysymToKeycode(display, map[i].xkey);
//if (map[i].libretro==RETROK_RALT) printf("%i %i %i\n",i,map[i].xkey,keycode);
		if (!keycode)
		{
			continue;//can't expect people to have all keys; for example, I lack Compose, and F13 through F15. (But who doesn't?)
		}
		//On the other hand, I have a lot of duplicates; PrintScreen and SysRq
		//are the same key. We'll map it to the one placed first in the table.
		//libretro_to_keycode_g[map[i].libretro]=keycode;
		libretrofor[keycode]=map[i].libretro;
	}
	while(i);
	
	initialized=true;
}

int inputkb_x11_translate_key(unsigned int keycode)
{
	return libretrofor[keycode];
}
#endif
