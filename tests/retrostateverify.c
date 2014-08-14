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
./retrostate roms/testcore_libretro.so - 60

won't work on Windows
*/

//These three will be called for every malloc/etc done in the program.
//dlopen will send the DATA and BSS segments to the malloc handler. calloc will also call malloc.
//realloc is guaranteed to only be done for reallocations. If either 'ptr' or 'size' are NULL/0, malloc or free will be called instead.
//Within these callbacks, further malloc/etc will not recurse.
//The s_ versions are set by memdebug_init; they act like normal malloc, but do not call the callbacks.
struct memdebug {
	void (*malloc)(void* ptr, size_t size);
	void (*free)(void* prev);
	void (*realloc)(void* prev, void* ptr, size_t size);
	
	void* (*s_malloc)(size_t size);
	void (*s_free)(void* prev);
	void* (*s_realloc)(void* prev, size_t size);
};
void memdebug_init(struct memdebug * i);


struct meminf {
	struct meminf * next;
	uint8_t * ptr;
	uint8_t * changing;
	uint8_t * laststate;
	size_t size;
	size_t alloc_id;
};
static struct meminf mem;
static struct meminf * tail;
static size_t alloc_id=0;

static const char * context;

static void tr_malloc(void* ptr, size_t size)
{
printf("al %lu from %s -> %p\n", size, context, ptr);
	tail->next=malloc(sizeof(struct meminf));
	tail=tail->next;
	tail->next=NULL;
	tail->ptr=ptr;
	tail->changing=calloc(size,1);
	tail->laststate=malloc(size);
	tail->size=size;
	tail->alloc_id=alloc_id++;
}

static void tr_free(void* prev)
{
printf("fr %p from %s\n", prev, context);
	struct meminf * inf=&mem;
	while (inf->next->ptr != prev) inf=inf->next;
	struct meminf * infnext=inf->next;
	inf->next=inf->next->next;
	if (tail==infnext) tail=inf;
	inf=infnext;
	
	free(infnext->changing);
	free(infnext->laststate);
	free(infnext);
}

static void tr_realloc(void* prev, void* ptr, size_t size)
{
	tr_free(prev);
	tr_malloc(ptr, size);
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
	struct memdebug i={ tr_malloc, tr_free, tr_realloc };
	memdebug_init(&i);
	context="main";
	tail=&mem;
//printf("bss=%p-%p data=%p-%p cdata=%p-%p\n",bss,bss+sizeof(bss),data,data+sizeof(data),cdata,cdata+sizeof(cdata));
//FILE* f=fopen("/proc/self/maps", "rt");
//fread(data, 1,65536, f);
//puts(data);
//return 1;
	
	//window_init(&argc, &argv);
	unsigned int frames=atoi(argv[3]);
	
	srand(time(NULL));
	
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
	for (unsigned int i=0;i<frames;i++)
	{
		core->run(core);
	}
	
	context="libretro->free";
	core->free(core);
}
