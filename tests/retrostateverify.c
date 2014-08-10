//retrostateverify - a libretro frontend that verifies that a savestate contains all variables that are changed by retro_run
//usage: retroprofile corepath rompath tries
//example: ./retroprofile roms/gambatte_libretro.so roms/zeldaseasons.gbc 10
//NOTE: not finished, doesn't actually verify anything.

#include "minir.h"
#include "libretro.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
/*
gcc -I. -std=c99 tests/retrostateverify.c tests/memdebug.c libretro.c dylib.c memory.c -ldl -lrt -DDYLIB_POSIX -DWINDOW_MINIMAL window-none.c -Os -s -o retrostate
./retrostate roms/gambatte_libretro.so roms/zeldaseasons.gbc 15

won't work on Windows
*/

struct memdebug {
	void (*malloc)(void* ptr, size_t size);
	void (*realloc)(void* prev, void* ptr, size_t size);
	void (*free)(void* prev);
};
void memdebug_init(struct memdebug * i);

const char * context;

static void tr_malloc(void* ptr, size_t size)
{
	printf("al %lu from %s\n", size, context);
}

static void tr_realloc(void* prev, void* ptr, size_t size)
{
	
}

static void tr_free(void* prev)
{
	
}

void no_video(struct video * this, unsigned int width, unsigned int height, const void * data, unsigned int pitch) {}
void no_audio(struct audio * this, unsigned int numframes, const int16_t * samples) {}
int16_t rand_input(struct libretroinput * this, unsigned port, unsigned device, unsigned index, unsigned id)
{
	if (device==RETRO_DEVICE_JOYPAD) return rand()&1;
	return 0;
}

int randr(int lower, int upper)
{
	unsigned int range=upper-lower+1;
	return lower + rand()%range;
}

int main(int argc, char * argv[])
{
	struct memdebug i={ tr_malloc, tr_realloc, tr_free };
	memdebug_init(&i);
	context="main";
	
	//window_init(&argc, &argv);
	unsigned int iterations=atoi(argv[3]);
	
	time_t starttime=time(NULL);
	
	for (unsigned int i=0;i<iterations;i++)
	{
		printf("%u/%u\r", i, iterations);
		fflush(stdout);
		srand(starttime+i);//this should generate multiple different 
		
		context="libretro_create";
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
		noinput.query=rand_input;
		
		core->attach_interfaces(core, &novideo, &noaudio, &noinput);
		
		context="libretro->load_rom";
		if (!core->load_rom(core, NULL,0, argv[2]))
		{
			puts("Couldn't load ROM.");
			return 1;
		}
		
		context="libretro->run";
		
		unsigned int frames=randr(30, 60);
		
		for (unsigned int i=0;i<frames;i++)
		{
			core->run(core);
		}
		
		context="libretro->free";
		core->free(core);
	}
}
