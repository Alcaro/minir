#include "minir.h"
#include <stdlib.h>
#include <stdio.h>
/*
gcc -I. -std=c99 tests/retroprofile.c libretro.c dylib.c memory.c -ldl -lrt -DWINDOW_MINIMAL window-none.c -Os -s -o retroprofile
mv retroprofile ~/bin
*/

void no_video(struct video * this, unsigned int width, unsigned int height, const void * data, unsigned int pitch) {}
void no_audio(struct audio * this, unsigned int numframes, const int16_t * samples) {}
int16_t no_input(struct libretroinput * this, unsigned port, unsigned device, unsigned index, unsigned id) { return 0; }

int main(int argc, char * argv[])
{
	//window_init(&argc, &argv);
	unsigned int frames=atoi(argv[3]);
	
	uint64_t t_init=window_get_time();
	struct libretro * core=libretro_create(argv[1], NULL, false);
	if (!core)
	{
		puts("Couldn't load core.");
		return 1;
	}
	
	static struct video novideo;
	novideo.draw=no_video;
	
	static struct audio noaudio;
	noaudio.render=no_audio;
	
	static struct libretroinput noinput;
	noinput.query=no_input;
	
	core->attach_interfaces(core, &novideo, &noaudio, &noinput);
	
	if (!core->load_rom(core, argv[2]))
	{
		puts("Couldn't load ROM.");
		return 1;
	}
	
	uint64_t t_run=window_get_time();
	
	for (unsigned int i=0;i<frames;i++)
	{
		printf("%i/%i\r", i, frames);
		core->run(core);
	}
	uint64_t t_run_done=window_get_time();
	
	core->free(core);
	uint64_t t_end=window_get_time();
	
	printf("Time to load ROM: %luus\n", t_run-t_init);
	if (frames)
	{
		printf("Time per frame: %luus / ", (t_run_done-t_run)/frames);
		printf("Frames per second: %f\n", (float)frames*1000000/(t_run_done-t_run));
	}
	printf("Time to exit: %luus\n", t_end-t_run_done);
	printf("Total time: %luus\n", t_end-t_init);
}
