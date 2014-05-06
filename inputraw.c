#include "minir.h"
#include <string.h>
#include <stdlib.h>
#include "libretro.h"

static unsigned int keyboard_num_keyboards(struct inputraw * this) { return 0; }
static unsigned int keyboard_num_keys(struct inputraw * this) { return 1; }
static bool keyboard_poll(struct inputraw * this, unsigned int kb_id, unsigned char * keys) { keys[0]=0; return true; }
static void keyboard_get_map(struct inputraw * this, const unsigned int ** keycode_to_libretro,
                                                     const unsigned int ** libretro_to_keycode)
{
	static const unsigned int * fakemap=NULL;
	if (!fakemap) fakemap=calloc(RETROK_LAST, sizeof(unsigned int));
	*keycode_to_libretro=fakemap;
	*libretro_to_keycode=fakemap;
}
static void free_(struct inputraw * this) {}

struct inputraw * _inputraw_create_none(uintptr_t windowhandle)
{
	static struct inputraw this={ keyboard_num_keyboards, keyboard_num_keys, keyboard_poll, keyboard_get_map, free_ };
	return &this;
}

struct inputraw * inputraw_create(const char * backend, uintptr_t windowhandle)
{
#ifdef INPUT_X11_XINPUT2
	if (!strcmp(backend, "X11-XInput2")) return _inputraw_create_x11_xinput2(windowhandle);
#endif
#ifdef INPUT_RAWINPUT
	if (!strcmp(backend, "RawInput")) return _inputraw_create_rawinput(windowhandle);
#endif
#ifdef INPUT_X11
	if (!strcmp(backend, "X11")) return _inputraw_create_x11(windowhandle);
#endif
#ifdef INPUT_DIRECTINPUT
	if (!strcmp(backend, "DirectInput")) return _inputraw_create_directinput(windowhandle);
#endif
	if (!strcmp(backend, "None")) return _inputraw_create_none(windowhandle);
	return NULL;
}

const char * const * inputraw_supported_backends()
{
	static const char * backends[]={
#ifdef INPUT_X11_XINPUT2
		"X11-XInput2",
#endif
#ifdef INPUT_RAWINPUT
		"RawInput",
#endif
#ifdef INPUT_X11
		"X11",
#endif
#ifdef INPUT_DIRECTINPUT
		"DirectInput",
#endif
		"None",
		NULL
	};
	return backends;
}
