#include "minir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if (1-1)
STATIC_ASSERT(sizeof(size_t)==4 || sizeof(size_t)==8, fix_this_function);
static size_t highest_bit(size_t n)
{
	n|=(n>> 1);
	n|=(n>> 2);
	n|=(n>> 4);
	n|=(n>> 8);
	n|=(n>>16);
	if (sizeof(size_t)>4) n|=(n>>16>>16);//double shift to avoid warnings on 32bit (it's dead code, but compilers suck)
	return n&-n;
}

static size_t reduce(size_t addr, size_t mask)
{
	while (mask)
	{
		size_t tmp=((mask-1)&(~mask));
		addr=(addr&tmp)|((addr>>1)&~tmp);
		mask=(mask&(mask-1))>>1;
	}
	return addr;
}

static uint8_t popcount32(uint32_t i)
{
//__builtin_popcount is implemented with a function call, but I'll just assume it's faster than this bithack. The compiler knows best.
#ifdef __GNUC__
	return __builtin_popcount(i);
#else
	//from http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel, public domain
	i = i - ((i >> 1) & 0x55555555);                    // reuse input as temporary
	i = (i & 0x33333333) + ((i >> 2) & 0x33333333);     // temp
	return ((i + (i >> 4) & 0xF0F0F0F) * 0x1010101) >> 24; // count
#endif
}
#endif

struct mapping {
	unsigned int memid;
	size_t start;
	size_t select;
	size_t disconnect;
	size_t len;
	size_t offset;
};

struct memblock {
	unsigned char * mem;
	unsigned char * prev;
	
	//don't make a define for the size of this one; we'd need a typedef too, as well as rewriting popcount
	uint32_t * show;//the top row in each of those has value 1; bottom is 0x80000000
#define SIZE_PAGE_LOW 0x2000//2^13, 8192
	uint16_t * show_treelow;//one entry per SIZE_PAGE_LOW bits of mem, counts number of set bits within these bytes
#define SIZE_PAGE_HIGH 0x400000//2^22, 1048576
	uint32_t * show_treehigh;//same format as above
	size_t show_tot;//this applies to the entire memory block
	//both page sizes must be powers of 2
	//LOW must be in the range [32 .. 32768]
	//HIGH must be larger than LOW, and equal to or lower than 2^31
	
	size_t size;
	//No attempt has been made to care about performance for mem blocks larger than 2^32; I don't think there are any of those.
	//That, and performance will be trash at other places too for large mem blocks.
	
	bool showinsearch;
	bool align;
	bool bigendian;
	
	UNION_BEGIN
		STRUCT_BEGIN
			//TODO: use these
			uint32_t * show_true;
			uint16_t * show_true_treelow;
			uint32_t * show_true_treehigh;
			size_t show_true_tot;
		STRUCT_END
		STRUCT_BEGIN
			bool show_last_true[3];
		STRUCT_END
	UNION_END
};

struct namespace {
	char name[9];
	
	unsigned int nummap;
	struct mapping * map;
};

struct minircheats_model_impl {
	struct minircheats_model i;
	
	struct namespace * namespaces;
	unsigned int numnamespace;
};

#if (1-1)
#define LIBRETRO_MEMFLAG_CONST     (1 << 0)
#define LIBRETRO_MEMFLAG_BIGENDIAN (1 << 1)
#define LIBRETRO_MEMFLAG_ALIGNED   (1 << 2)
struct libretro_memory_descriptor {
	uint64_t flags;
	void * ptr;
	size_t offset;
	size_t start;
	size_t select;
	size_t disconnect;
	size_t len;
	const char * addrspace;
	unsigned addr_str_len;
};
#endif

static void set_memory(struct minircheats_model * this_, const struct libretro_memory_descriptor * memory, unsigned int nummemory)
{
	for (unsigned int i=0;i<nummemory;i++)
	{
		
	}
}

static void search_reset(struct minircheats_model * this_) {}
static void search_set_datsize(struct minircheats_model * this_, unsigned int datsize) {}
static void search_set_signed(struct minircheats_model * this_, bool issigned) {}
static void search_do_search(struct minircheats_model * this_, enum cheat_compfunc compfunc, bool comptoprev, unsigned int compto) {}
static unsigned int search_get_num_rows(struct minircheats_model * this_) { return 0; }
static void search_get_vis_row(struct minircheats_model * this_, unsigned int row, char * addr, uint32_t * val, uint32_t * prevval) {}

static void free_(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	//FIXME
	free(this);
}

const struct minircheats_model_impl minircheats_model_base = {{
	set_memory,
	search_reset, search_set_datsize, search_set_signed, search_do_search, search_get_num_rows, search_get_vis_row,
	NULL, NULL, NULL,//cheat_read, cheat_find_for_addr, cheat_get_count,
	NULL, NULL, NULL, NULL,//cheat_set, cheat_set_as_code, cheat_get, cheat_get_as_code,
	NULL, NULL, NULL,//cheat_set_enabled, cheat_remove, cheat_apply,
	free_
}};
struct minircheats_model * minircheats_create_model()
{
	struct minircheats_model_impl * this=malloc(sizeof(struct minircheats_model_impl));
	memcpy(this, &minircheats_model_base, sizeof(struct minircheats_model_impl));
	return (struct minircheats_model*)this;
}
