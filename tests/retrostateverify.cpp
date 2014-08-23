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
g++ -I. -std=c99 tests/retrostateverify.cpp tests/memdebug.cpp libretro.cpp dylib.cpp memory.cpp -ldl -lrt -DDYLIB_POSIX -DWINDOW_MINIMAL window-none.cpp -Os -s -o retrostate
./retrostate roms/testcore_libretro.so - 5 60 30
//usage: retrostate /path/to/core.so /path/to/rom.gbc frames_per_round frames_between_rounds rounds
//all framecounts are actually random numbers in the range [given, given*2]

rm retrostate; g++ -I. tests/retrostateverify.cpp tests/memdebug.cpp libretro.cpp dylib.cpp memory.cpp -ldl -lrt -DDYLIB_POSIX -DWINDOW_MINIMAL window-none.cpp -Og -g -o retrostate; ./retrostate roms/testcore_libretro.so - 5 60 30

won't work on Windows
*/

//plan:
//run 2..5 frames with random input
//input is kept in a ring buffer of 256 frames
//loop:
//save all core memory to INITIAL; also make a savestate
//run 2..5 frames with random input
//save all core memory to EXPECTED
//load savestate
//save all core memory to INITIAL_POST
//run same number of frames with same input
//save all core memory to EXPECTED_POST
//compare stuff, see next comment
//run 5..10 frames with random input
//end loop

//after saving EXPECTED_POST:
//check for differences between EXPECTED and EXPECTED_POST
// if there are any, check same address in INITIAL and INITIAL_POST
//  if different there too, loudly explain that this address is broken
//   we can't just compare INITIAL with INITIAL_POST because some things are recalculated each frame (e.g. framebuffer)
//   on the other hand, we don't want to compare only EXPECTED/POST; differences that matter in INITIAL/POST will spread to everywhere
//  if same, ignore it for now
// if there are no matching differences in INITIAL/POST, print the differences in EXPECTED/POST
//stack is ignored because it is recreated each frame; libco threads are malloc'd and therefore analyzed

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
	uint8_t * init;
	uint8_t * expected;
	size_t size;
	size_t alloc_id;
};
static struct meminf mem;
static struct meminf * tail;
static size_t alloc_id=0;
static size_t totsize=0;

static const char * context;

static void tr_malloc(void* ptr, size_t size)
{
printf("al %lu from %s -> %p\n", size, context, ptr);
	tail->next=malloc(sizeof(struct meminf));
	tail=tail->next;
	tail->next=NULL;
	tail->ptr=(uint8_t*)ptr;
	tail->init=(uint8_t*)malloc(size);
	tail->expected=(uint8_t*)malloc(size);
	tail->size=size;
	tail->alloc_id=alloc_id++;
	totsize+=size;
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
	
	totsize-=inf->size;
	free(inf->init);
	free(inf->expected);
	free(inf);
}

uint16_t input_bits;

void no_video(struct video * this, unsigned int width, unsigned int height, const void * data, unsigned int pitch) {}
void no_audio(struct audio * this, unsigned int numframes, const int16_t * samples) {}
int16_t queue_input(struct libretroinput * this, unsigned port, unsigned device, unsigned index, unsigned id)
{
	if (device==RETRO_DEVICE_JOYPAD)
	{
		if (port==0 && index==0) return input_bits>>id & 1;//I don't know in which order this ends up. But it doesn't matter either.
	}
	return 0;
}

int randr(int lower, int upper)
{
	unsigned int range=upper-lower+1;
	return lower + rand()%range;
}

void copy_mem()
{
	
}

void xor_mem()
{
	
}

int main(int argc, char * argv[])
{
	struct memdebug i={ tr_malloc, tr_free };
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
	
	unsigned int seed=(argv[6] ? atoi(argv[6]) : time(NULL));
	srand(seed);
	
	context="libretro_create";
	struct libretro * core=libretro_create(argv[1], NULL, NULL);
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
	if (!statesize) abort();
	void* state=i.s_malloc(statesize);
	
	printf("Savestate size is: %lu.\n", statesize);
	printf("Total core memory used is: %lu.\n", totsize);
	
	for (unsigned int i=0;i<rounds;i++)
	{
		//run a few frames at random
		context="libretro->run (1)";
		printf("%i/%i\r", i, rounds); fflush(stdout);
		for (unsigned int skip=randr(betweenround, betweenround*2);skip;skip--)
		{
			//input_bits=randr(0, 65535);
			input_bits=0;
			core->run(core);
		}
		
		//prepare input, save state
		unsigned int framesthisround=randr(perround, perround*2);
		uint16_t input_list[framesthisround];
		for (unsigned int i=0;i<framesthisround;i++) input_list[i]=randr(0, 65535);
		context="libretro->state_save";
		if (!core->state_save(core, state, statesize)) abort();
		
#define handle_mem(oper, to) \
		{ \
			struct meminf * item=mem.next; \
			while (item) \
			{ \
				for (size_t i=0;i<item->size;i++) \
				{ \
					item->to[i] oper item->ptr[i]; \
				} \
				item = item->next; \
			} \
		}
		//save state INITIAL
		handle_mem(=, init);
		
		//run for a few frames
		context="libretro->run (2)";
		for (unsigned int i=0;i<framesthisround;i++)
		{
			//input_bits=input_list[i];
			input_bits=0;
			core->run(core);
		}
		
		//save state EXPECTED
		handle_mem(=, expected);
		
		context="libretro->state_load";
		if (!core->state_load(core, state, statesize)) abort();
		
		//save state INITIAL_POST
		handle_mem(^=, init);
		
		context="libretro->run (3)";
		for (unsigned int i=0;i<framesthisround;i++)
		{
			//input_bits=input_list[i];
			input_bits=0;
			core->run(core);
		}
		
		//we could save state EXPECTED_POST here, but let's just compare it instead. It's good enough.
		{
			struct meminf * item=mem.next;
			while (item)
			{
				for (size_t i=0;i<item->size;i++)
				{
					if (item->expected[i]!=item->ptr[i])
					{
						printf("%p+%lu[%lu]: %.2X!=%.2X\n", item->ptr, i, item->size, item->expected[i], item->ptr[i]);
						//break;
					}
				}
				item = item->next;
			}
		}
		//save state EXPECTED_POST
		//handle_mem(^=, expected);
		
		//compare stuff
	}
	
	context="libretro->free";
	core->free(core);
}
