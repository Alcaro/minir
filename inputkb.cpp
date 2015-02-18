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


#include "libretro.h"
#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

//See rescompile.c for the actual key names.
static const unsigned char g_keynames_comp[]={
#define KEYNAMES_COMP
#include "obj/generated.cpp"
#undef KEYNAMES_COMP
};
static char g_keynames_decomp[KEYNAMES_DECOMP_LEN];
static const char * g_keynames[RETROK_LAST];

const char * const * inputkb::keynames()
{
	if (!g_keynames[RETROK_BACKSPACE])
	{
		tinfl_decompress_mem_to_mem(g_keynames_decomp, sizeof(g_keynames_decomp), g_keynames_comp, sizeof(g_keynames_comp), 0);
		char * tmp=g_keynames_decomp;
		for (unsigned int i=0;i<RETROK_LAST;i++)
		{
			g_keynames[i]=*tmp ? tmp : NULL;
			tmp+=strlen(tmp)+1;
		}
	}
	return g_keynames;
}
