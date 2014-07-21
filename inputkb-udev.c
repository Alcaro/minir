#include "minir.h"
#ifdef INPUT_UDEV
#include <linux/input.h>

struct inputkb * inputkb_create_udev(uintptr_t windowhandle)
{
	return NULL;
}
#endif
