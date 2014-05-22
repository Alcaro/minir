#include "minir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//how address conversion works
//----------------------------
//
//preparation:
//for each mapping:
// if both 'len' and 'select' are zero, use len=1
// if 'select' is zero, fill it in with len-1
// 
// if a bit is set in start but not in select (start & ~select) is nonzero, panic
//  panicing can be done with abort()
// 
// calculate the highest possible address in this address space
//  math: top_addr|=select
// top_addr=add_bits_down(top_addr)
//
//for each mapping:
// if 'len' is zero, fill it in with the number of bytes defined by this mapping
//  math: add_bits_down(reduce(~select&top_addr, disconnect))+1
// while reduce(highest possible address for this mapping, disconnect) is greater than len, disconnect the top still-connected bit
//  math: while (reduce(~select&top_addr, disconnect)>>1 > len-1) disconnect|=highest_bit(~select&top_addr&~disconnect)
// 
// create variable 'variable_bits' for mapping; it contains which bits could change in the guest address and still refer to the same byte
// set variable_bits to all disconnected and not selected bits
// if 'len' is not a power of two:
//  inflate 'len' with 'disconnect', take the highest bit of that, and add it to variable_bits
//  (do not optimize to topbit(len)<<popcount(disconnect & topbit(len)-1); it's painful if the shift adds more disconnected bits)
// 
// create variable 'disconnect_mask' for mapping, set it to ~add_bits_down(len)
// clear any bit in 'disconnect' that's set in 'disconnect_mask'
// while the highest set bit in 'disconnect' is directly below the lowest set bit in 'disconnect_mask', move it over
// in any future reference to 'disconnect', use 'disconnect_mask' too
// math:
//  disconnect_mask = ~add_bits_down(len)
//  disconnect &=~ disconnect_mask
//  while (disconnect_mask>>1 & disconnect)
//  {
//   disconnect_mask |= disconnect_mask>>1
//   disconnect &=~ disconnect_mask
//  }
// 
// check if any previous mapping claims any byte claimed by this mapping
//  math: there is a collision if (A.select & B.select & (A.start^B.start)) == 0
// if not, mark the mapping as such, and avoid the inner loop when converting physical to guest
//
//
//guest to physical:
//pick the first mapping where start and select match
//if address is NULL, return failure
//subtract start, pick off disconnect, apply len, add offset
//
//
//physical to guest:
//check cache
// if hit, subtract cached difference and return this address
//select all mappings which map this address ('ptr' and 'offset'/'length')
//for each:
// subtract offset
// do nothing with length
// fill in disconnect with zeroes
// add start
// check all previous mappings, to see if this is the first one to claim this address; if so:
//  find how long this mapping is linear, that is how far we can go while moving one guest byte still moves one physical byte
//   (use the most strict of 'length', 'disconnect', 'select', and other mappings)
//  put start and size of linear area, as well as where it points, in the cache
//  return this address, ignoring the above linearity calculations
// else:
//  fill in an 1 in the lowest hole permitted by 'length' or 'disconnect', try again
//  if we're out of holes, use next mapping
//  if there is no next mapping, segfault because all bytes must be mapped somewhere.

STATIC_ASSERT(sizeof(size_t)==4 || sizeof(size_t)==8, fix_this_function);
static size_t add_bits_down(size_t n)
{
	n|=(n>> 1);
	n|=(n>> 2);
	n|=(n>> 4);
	n|=(n>> 8);
	n|=(n>>16);
	if (sizeof(size_t)>4) n|=(n>>16>>16);//double shift to avoid warnings on 32bit (it's dead code, but compilers suck)
	return n;
}

static size_t highest_bit(size_t n)
{
	n=add_bits_down(n);
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

//Inverts reduce().
//reduce(inflate(x, y), y) == x (assuming no overflow)
//inflate(reduce(x, y), y) == x & ~y
static size_t inflate(size_t addr, size_t mask)
{
	while (mask)
	{
		size_t tmp=((mask-1)&(~mask));
		//to put in an 1 bit instead, OR in tmp+1
		addr=((addr&~tmp)<<1)|(addr&tmp);
		mask=(mask&(mask-1));
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

static uint8_t popcount64(uint64_t v)
{
#ifdef __GNUC__
	return __builtin_popcountll(v);
#else
#define T uint64_t
	//http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel again
	T c;
	v = v - ((v >> 1) & (T)~(T)0/3);                           // temp
	v = (v & (T)~(T)0/15*3) + ((v >> 2) & (T)~(T)0/15*3);      // temp
	v = (v + (v >> 4)) & (T)~(T)0/255*15;                      // temp
	c = (T)(v * ((T)~(T)0/255)) >> (sizeof(T) - 1) * CHAR_BIT; // count
	return c;
#undef T
#endif
}

STATIC_ASSERT(sizeof(size_t)==4 || sizeof(size_t)==8, fix_this_function);
static uint8_t popcountS(size_t i)
{
	if (sizeof(size_t)==4) return popcount32(i);
	if (sizeof(size_t)==8) return popcount64(i);
	return 0;
}

struct mapping {
	unsigned int memid;
	bool has_overlaps;//Whether any previous mapping overrides any part of this mapping.
	size_t start;
	size_t select;
	size_t disconnect;
	size_t disconnect_mask;
	size_t len;
	size_t offset;
	
	//which bits could potentially change and still refer to the same byte (not just the same mapping)
	//len not being a power of two gives one 'maybe' here
	size_t variable_bits;
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
	//That, and performance will be trash at other places too for large mem blocks. Manipulating 2^29 bytes in 'show' isn't cheap.
	
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

struct addrspace {
	char name[9];
	unsigned int numbits;//valid values are 1..64; 24 for SNES
	
	unsigned int nummap;
	struct mapping * map;
	
	unsigned int nummem;
	struct memblock * mem;
};

struct minircheats_model_impl {
	struct minircheats_model i;
	
	struct addrspace * addrspaces;
	unsigned int numaddrspace;
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
	//unsigned addr_str_len;//just calculate this from top 'select' of anything.
};
#endif

static void delete_all_mem(struct minircheats_model_impl * this);

static void set_memory(struct minircheats_model * this_, const struct libretro_memory_descriptor * memory, unsigned int nummemory)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	delete_all_mem(this);
	//see top of file for what all this math does
	for (unsigned int i=0;i<nummemory;i++)
	{
		const struct libretro_memory_descriptor * desc=&memory[i];
		
		struct addrspace * addr;
		unsigned int addrspace;
		for (addrspace=0;addrspace<this->numaddrspace;addrspace++)
		{
			if (!strcmp(this->addrspaces[addrspace].name, desc->addrspace ? desc->addrspace : "")) break;
		}
		if (addrspace == this->numaddrspace)
		{
			this->numaddrspace++;
			this->addrspaces=realloc(this->addrspaces, sizeof(struct addrspace)*this->numaddrspace);
			addr=&this->addrspaces[addrspace];
			strcpy(addr->name, desc->addrspace ? desc->addrspace : "");
			addr->numbits=1;
			addr->nummap=0;
			addr->map=NULL;
			addr->nummem=0;
			addr->mem=NULL;
		}
		addr=&this->addrspaces[addrspace];
		
		addr->nummap++;
		addr->map=realloc(addr->map, sizeof(struct mapping)*addr->nummap);
		struct mapping * map=&addr->map[addr->nummap-1];
//TODO: mess with addr->memblock
		map->memid=0;//FIXME
		map->start=desc->start;
		map->select=desc->select;
		map->disconnect=desc->disconnect;
		map->len=desc->len;
		map->offset=desc->offset;
	}
	
	//at this point:
	//addr->numbits is uninitialized
	//map->select is possibly zero
	//map->len is possibly zero
	//map->variable_bits is uninitialized
	//map->disconnect_mask is uninitialized
	//map->has_overlaps is false
	//other things are correct
	
	for (unsigned int i=0;i<this->numaddrspace;i++)
	{
		struct addrspace * addr=&this->addrspaces[i];
		size_t top_addr=1;//to avoid trouble if the size is 0. A zero-bit address space isn't an address space at all, anyways.
		for (unsigned int i=0;i<addr->nummap;i++)
		{
			struct mapping * map=&addr->map[i];
			top_addr|=map->select;
			
			//start+len is garbage if the length isn't a power of 2, if disconnect is nonzero, or if len is larger than select
			//but in that case, select is the one we want, so we can ignore this.
			if (!map->select) top_addr |= map->start+map->len-1;
		}
		top_addr=add_bits_down(top_addr);
		
		for (unsigned int i=0;i<addr->nummap;i++)
		{
			struct mapping * map=&addr->map[i];
			if (map->select==0)
			{
				if (map->len==0) map->len=1;
				map->select=top_addr&~add_bits_down(map->len-1);
			}
			if (!map->len) map->len=add_bits_down(reduce(map->variable_bits, map->disconnect))+1;
			if (map->start & ~map->select) abort();//this combination is invalid
			while (reduce(top_addr&~map->select, map->disconnect)>>1 > map->len-1)
			{
				map->disconnect|=highest_bit(top_addr&~map->select&~map->disconnect);
			}
			
			map->variable_bits=(map->disconnect&~map->select);
			
			//this can not be rewritten as topbit(len)<<popcount(disconnect & topbit(len)-1);
			// it'd be painful if the shift adds more disconnected bits
			if (map->len & (map->len-1)) map->variable_bits|=highest_bit(inflate(map->len, map->disconnect));
			
			map->disconnect_mask=~add_bits_down(map->len);
			map->disconnect&=~map->disconnect_mask;
			while (map->disconnect_mask>>1 & map->disconnect)
			{
				map->disconnect_mask |= map->disconnect_mask>>1;
				map->disconnect&=~map->disconnect_mask;
			}
			
			map->has_overlaps=false;
			for (unsigned int j=0;j<i;j++)
			{
				if ((addr->map[i].select & addr->map[j].select & (addr->map[i].start ^ addr->map[j].start)) == 0)
				{
					map->has_overlaps=true;
					break;
				}
			}
		}
		
		addr->numbits=popcountS(top_addr);
	}
}

static void search_reset(struct minircheats_model * this_) {}
static void search_set_datsize(struct minircheats_model * this_, unsigned int datsize) {}
static void search_set_signed(struct minircheats_model * this_, bool issigned) {}
static void search_do_search(struct minircheats_model * this_, enum cheat_compfunc compfunc, bool comptoprev, unsigned int compto) {}
static unsigned int search_get_num_rows(struct minircheats_model * this_) { return 0; }
static void search_get_vis_row(struct minircheats_model * this_, unsigned int row, char * addr, uint32_t * val, uint32_t * prevval) {}

static void delete_all_mem(struct minircheats_model_impl * this)
{
	free(this->addrspaces);
	this->addrspaces=NULL;
	this->numaddrspace=0;
}

static void free_(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	delete_all_mem(this);
	//FIXME
	free(this);
}

const struct minircheats_model_impl minircheats_model_base = {{
	set_memory,
	NULL, NULL, NULL,//prev_get_size, prev_set_enabled, prev_get_enabled
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
