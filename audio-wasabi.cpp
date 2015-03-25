#include "io.h"
#ifdef AUDIO_WASAPI
struct caudio * audio_create_wasapi(uintptr_t windowhandle, double samplerate, double latency)
{
	return NULL;
}
#endif
