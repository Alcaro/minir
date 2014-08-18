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
./retrostate roms/testcore_libretro.so - 5 60 30
//usage: retrostate /path/to/core.so /path/to/rom.gbc frames_per_round frames_between_rounds rounds
//all framecounts are actually random numbers in the range [given, given*2]

rm retrostate; gcc -I. -std=c99 tests/retrostateverify.c tests/memdebug.c libretro.c dylib.c memory.c -ldl -lrt -DDYLIB_POSIX -DWINDOW_MINIMAL window-none.c -Og -g -o retrostate; ./retrostate roms/testcore_libretro.so - 5 60 30

won't work on Windows
*/

//plan:
//run 2..5 frames with random input
//input is kept in a ring buffer of 256 frames
//loop:
//save all core memory; also make a savestate
//run 2..5 frames with random input
//randomize all core memory that has changed
//load savestate
//run same number of frames with same input
//compare all internal variables; if any are different, scream
//run 5..10 frames with random input
//end loop

//These three will be called for every malloc/etc done in the program.
//dlopen will send the DATA and BSS segments to the malloc handler.
//calloc will also call malloc. Allocating anything larger than size_t is impossible, but that hasn't been possible since the 90s anyways.
//realloc is guaranteed to only be done for reallocations. If either 'ptr' or 'size' are NULL/0, malloc or free will be called instead, as appropriate.
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

uint16_t input_bits;

void no_video(struct video * this, unsigned int width, unsigned int height, const void * data, unsigned int pitch) {}
void no_audio(struct audio * this, unsigned int numframes, const int16_t * samples) {}
int16_t queue_input(struct libretroinput * this, unsigned port, unsigned device, unsigned index, unsigned id)
{
	if (device==RETRO_DEVICE_JOYPAD)
	{
		if (port==0 && index==0) return input_bits>>id & 1;
	}
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
	unsigned int perround=atoi(argv[3]);
	unsigned int betweenround=atoi(argv[4]);
	unsigned int rounds=atoi(argv[5]);
	
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
	noinput.query=queue_input;
	
	core->attach_interfaces(core, &novideo, &noaudio, &noinput);
	
	context="libretro->load_rom";
	if (!core->load_rom(core, NULL,0, argv[2]))
	{
		puts("Couldn't load ROM.");
		return 1;
	}
	
	context="libretro->state_size";
	size_t statesize=core->state_size(core);
	void* state=i.s_malloc(statesize);
	
	for (unsigned int i=0;i<rounds;i++)
	{
		context="libretro->run (1)";
		printf("%i/%i\r", i, rounds); fflush(stdout);
		for (unsigned int skip=randr(betweenround, betweenround*2);skip;skip--)
		{
			core->run(core);
		}
		unsigned int framesthisround=randr(perround, perround*2);
		uint16_t input_list[framesthisround];
		for (unsigned int i=0;i<framesthisround;i++) input_list[i]=randr(0, 65535);
		context="libretro->state_save";
		core->state_save(core, state, statesize);
		context="libretro->run (2)";
		for (unsigned int i=0;i<framesthisround;i++)
		{
			input_bits=input_list[i];
			core->run(core);
		}
		//TODO: save everything
		context="libretro->state_load";
		core->state_load(core, state, statesize);
		context="libretro->run (3)";
		for (unsigned int i=0;i<framesthisround;i++)
		{
			input_bits=input_list[i];
			core->run(core);
		}
		//TODO: compare everything
	}
	
	context="libretro->free";
	core->free(core);
}
