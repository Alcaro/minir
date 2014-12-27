/*
retrostateverify - a specialized libretro frontend that verifies that a savestate contains all variables that are changed by retro_run

Compile with:
g++ -I. -std=c99 subproj/retrostateverify.cpp subproj/memdebug.cpp libretro.cpp dylib.cpp memory.cpp -ldl -lrt -DDYLIB_POSIX -DWINDOW_MINIMAL window-none.cpp -Os -s -o retrostate
The program runs only on Linux. You will not have any success on Windows, unless you're willing to write about 200 lines of ugly code.

Run with:
gdb --args retrostate /path/to/core.so /path/to/rom.bin frames_per_round frames_between_rounds rounds
example: gdb --args ./retrostate roms/testcore_libretro.so - 5 60 30
All framecounts are actually random numbers in the range [given, given*2].

If a problem is found, the relevant address will be printed, and execution will be stopped. For best results, use gdb.

If the address is in the DATA or BSS section, tracking it down is easy:
 info symbol <address> (example: 'info symbol 0x7fff12345678')
 (Try this first. If it doesn't work, it's heap.)
If it's on the heap, you'll have to do more:
 breakpoint tr_malloc
 run (if the program is still running, say Yes to restarting)
 continue <id>
 bt
where <id> is the number after the # in the failure report.

Failure report format:
FAILURE (<type>) at <addr> [<addrbase> + <offset>/<size>, #<id>]: <expected>!=<actual>
<type> is 'root' or 'cascaded'. There is only one of the types for any given run; both show up, but
  any 'root' shadows all 'cascade' as one incorrect value very often drags thousands more with it.
 'root' means a variable that had wrong value after loading the savestate, and continued being wrong.
 'cascaded' is an address that has the wrong value, but was right when the savestate was loaded.
  This usually means that the problem source was wrong too, but fixed itself.
  Run the program again and it will use another number of frames, hopefully showing the real cause.
<addr> is the faulting address.
<addrbase> is the start of the allocation containing the faulting address.
 Note that the DATA and BSS sections count as only one allocation each.
<offset> is the difference between <addr> and <addrbase>.
<size> is how big the relevant allocation is.
 DATA and BSS sections are large, and multiples of 4096 bytes; other allocations are usually, but not always, small.
 (A large non-DATA/BSS allocation would be the framebuffer. However, it is possible for it to be on both stack and BSS.)
<id> is the sequential number of the relevant malloc()/free().
<expected> and <actual> are what the relevant byte are, and what they should be, respectively.
You usually only need <type>, <addr> and <id>.

False positives have been observed:
- _GLOBAL_OFFSET_TABLE_ has given weird results for unknown reasons.
- Failures have been observed cascading back into volatile arrays (e.g. framebuffer). This makes them show up as guilty.

rm retrostate; g++ -I. subproj/retrostateverify.cpp subproj/memdebug.cpp libretro.cpp dylib.cpp memory.cpp -ldl -lrt -DDYLIB_POSIX -DWINDOW_MINIMAL window-none.cpp -Og -g -o retrostate; gdb --args ./retrostate roms/snes9x_libretro.so ~/smw.smc 5 60 30

won't work on Windows
*/

#include "minir.h"
#include "libretro.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <signal.h>

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
	uint8_t * flags;
#define FL_BAD_CASCADE 1
#define FL_BAD_ROOT 2
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
	if (!ptr) return;
#ifdef VERBOSE
printf("al %lu from %s -> %p\n", size, context, ptr);
#endif
	tail->next=malloc(sizeof(struct meminf));
	tail=tail->next;
	tail->next=NULL;
	tail->ptr=(uint8_t*)ptr;
	tail->init=(uint8_t*)malloc(size);
	tail->expected=(uint8_t*)malloc(size);
	tail->flags=(uint8_t*)malloc(size);
	memset(tail->flags, 0, size);
	tail->size=size;
	tail->alloc_id=++alloc_id;
	totsize+=size;
}

static void tr_free(void* prev)
{
#ifdef VERBOSE
printf("fr %p from %s\n", prev, context);
#endif
	struct meminf * inf=&mem;
	while (inf->next->ptr != prev) inf=inf->next;
	struct meminf * infnext=inf->next;
	inf->next=inf->next->next;
	if (tail==infnext) tail=inf;
	inf=infnext;
	
	totsize-=inf->size;
	free(inf->init);
	free(inf->expected);
	free(inf->flags);
	free(inf);
}

const uint8_t* vidbuffer=NULL;
size_t vidbufsize=0;
void trace_video(struct video * this, unsigned int width, unsigned int height, const void * data, unsigned int pitch)
{
	if (!vidbuffer)
	{
		vidbuffer=(const uint8_t*)data;
		vidbufsize=pitch*height;
	}
	else if (vidbuffer!=(const uint8_t*)data || vidbufsize!=height*pitch)
	{
		vidbufsize=0;
	}
}

void no_audio(struct audio * this, unsigned int numframes, const int16_t * samples) {}

uint16_t input_bits;
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

void die(const char * why)
{
	puts(why);
	abort();
}

int main(int argc, char * argv[])
{
	tr_malloc(NULL, 0);
	
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
	if (!core) die("Couldn't load core.");
	
	static struct video novideo;
	novideo.draw=trace_video;
	
	static struct audio noaudio;
	noaudio.render=no_audio;
	
	static struct libretroinput noinput;
	noinput.query=queue_input;
	
	core->attach_interfaces(core, &novideo, &noaudio, &noinput);
	
	context="libretro->load_rom";
	if (!core->load_rom(core, NULL,0, argv[2])) die("Couldn't load ROM.");
	
	context="libretro->state_size";
	size_t statesize=core->state_size(core);
	if (!statesize) die("Core must support savestates.");
	void* state=i.s_malloc(statesize);
	
	printf("Savestate size is: %lu.\n", statesize);
	printf("Total core memory used is: %lu.\n", totsize);
	printf("Seed used: %u.\n", seed);
	
	bool anyroots=false;
	bool anyfailures=false;
	for (unsigned int thisround=0;thisround<rounds;thisround++)
	{
		//run a few frames at random
		context="libretro->run (1)";
		printf("%i/%i\r", thisround, rounds); fflush(stdout);
		for (unsigned int skip=randr(betweenround, betweenround*2);skip;skip--)
		{
			input_bits=randr(0, 65535);
			core->run(core);
		}
		
		//prepare input, save state
		unsigned int framesthisround=randr(perround, perround*2);
		uint16_t input_list[framesthisround];
		for (unsigned int i=0;i<framesthisround;i++) input_list[i]=randr(0, 65535);
		context="libretro->state_save";
		if (!core->state_save(core, state, statesize)) die("Core refuses to save state.");
		
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
			input_bits=input_list[i];
			core->run(core);
		}
		
		//save state EXPECTED
		handle_mem(=, expected);
		
		context="libretro->state_load";
		if (!core->state_load(core, state, statesize)) die("Core refuses to load state.");
		
		//save state INITIAL_POST
		handle_mem(^=, init);
		
		context="libretro->run (3)";
		for (unsigned int i=0;i<framesthisround;i++)
		{
			input_bits=input_list[i];
			core->run(core);
		}
		
		//we could save state EXPECTED_POST here, but let's compare it instead because we need to do that anyways.
		{
			struct meminf * item=mem.next;
			while (item)
			{
				for (size_t i=0;i<item->size;i++)
				{
					if (item->expected[i]!=item->ptr[i])
					{
						if (item->init[i]!=0)
						{
							item->flags[i]|=FL_BAD_ROOT;
							anyroots=true;
						}
						item->flags[i]|=FL_BAD_CASCADE;
						anyfailures=true;
					}
				}
				item = item->next;
			}
		}
	}
	
	if (anyfailures)
	{
		uint8_t flags_min = (anyroots ? FL_BAD_ROOT : FL_BAD_CASCADE);
		struct meminf * item=mem.next;
		while (item)
		{
			for (size_t i=0;i<item->size;i++)
			{
				if (item->flags[i] & flags_min)
				{
					size_t badstart=i;
					while (i<item->size && item->flags[i] & flags_min) i++;
					i--;
					if (i==badstart)
					{
						printf("FAILURE (%s) at %p, #%lu [%p + %lu/%lu]\n", anyroots ? "root" : "cascaded",
						       item->ptr+i, item->alloc_id, item->ptr, i, item->size);
					}
					else
					{
						printf("FAILURE (%s) at %p-%p, #%lu [%p + %lu-%lu/%lu]\n", anyroots ? "root" : "cascaded",
						       item->ptr+badstart, item->ptr+i, item->alloc_id, item->ptr, badstart, i, item->size);
					}
				}
			}
			item = item->next;
		}
		raise(SIGTRAP);//break while the .so is still loaded
	}
	else puts("No problems found.");
	
	context="exit";
	_Exit(0);
}
