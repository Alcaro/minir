#include "minir.h"
#undef malloc
#undef realloc
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "libretro.h"

//how address conversion works
//----------------------------
//
//preparation:
//for each mapping:
// if both 'len' and 'select' are zero, whine
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
// create variable 'disconnect_mask' for mapping, set it to add_bits_down(len)
// clear any bit in 'disconnect' that's clear in 'disconnect_mask'
// while the highest set bit in 'disconnect' is directly below the lowest clear bit in 'disconnect_mask', move it over
// in any future reference to 'disconnect', use 'disconnect_mask' too
// math:
//  disconnect_mask = add_bits_down(len-1)
//  disconnect &= disconnect_mask
//  while ((~disconnect_mask)>>1 & disconnect)
//  {
//   disconnect_mask >>= 1
//   disconnect &= disconnect_mask
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
//  find how long this mapping is linear, that is how far we can go while still ensuring that moving one guest byte
//   still moves one physical byte (use the most strict of 'length', 'disconnect', 'select', and other mappings)
//  put start and size of linear area, as well as where it points, in the cache
//  return this address, ignoring the above linearity calculations
// else:
//  fill in an 1 in the lowest hole permitted by 'length' or 'disconnect', try again
//  if we're out of holes, use next mapping
//  if there is no next mapping, segfault because all bytes must be mapped somewhere.

//in this file, 'address' means address in any emulated address space (also known as guest address),
// and 'offset' means offset to any memory block (physical address)

//For compatibility with RetroArch, this file has the following restrictions, in addition to the global rules:
//- Do not call any function from minir.h; force the user of the object to do that. The only allowed
// parts of minir.h are STATIC_ASSERT, struct minircheats_model and friends, and UNION_BEGIN and friends.
//- Do not dynamically change the interface; use a switch. If that becomes a too big pain, stick a
// function pointer in minircheats_model_impl.
//- No C++ incompatibilities, except using 'this' as variable name. malloc return values must be casted.

static size_t add_bits_down(size_t n)
{
	STATIC_ASSERT(sizeof(size_t)==4 || sizeof(size_t)==8, fix_this_function);
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
	return n^(n>>1);
}

static uint8_t popcount32(uint32_t i)
{
//I don't know what __builtin_popcount does, but using the bithack instead makes the entire program (SSE2 path) ~10% faster.
//#ifdef __GNUC__
//	return __builtin_popcount(i);
//#else
	//from http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel, public domain
	i = i - ((i >> 1) & 0x55555555);                    // reuse input as temporary
	i = (i & 0x33333333) + ((i >> 2) & 0x33333333);     // temp
	return ((i + ((i >> 4) & 0xF0F0F0F)) * 0x1010101) >> 24; // count
//#endif
}

static uint8_t popcount64(uint64_t v)
{
//#ifdef __GNUC__
//	return __builtin_popcountll(v);
//#else
	//http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel again
#define T uint64_t
	v = v - ((v >> 1) & (T)~(T)0/3);                           // temp
	v = (v & (T)~(T)0/15*3) + ((v >> 2) & (T)~(T)0/15*3);      // temp
	v = (v + (v >> 4)) & (T)~(T)0/255*15;                      // temp
	v = (T)(v * ((T)~(T)0/255)) >> (sizeof(T) - 1) * CHAR_BIT; // count
#undef T
	return v;
//#endif
}

static uint8_t popcountS(size_t i)
{
	STATIC_ASSERT(sizeof(size_t)==4 || sizeof(size_t)==8, fix_this_function);
	if (sizeof(size_t)==4) return popcount32(i);
	if (sizeof(size_t)==8) return popcount64(i);
	return 0;//unreachable
}

#define div_rndup(a,b) (((a)+(b)-1)/(b))

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

static uint32_t readmem(const unsigned char * ptr, unsigned int nbytes, bool bigendian)
{
	if (nbytes==1) return *ptr;
	if (bigendian)
	{
		uint32_t ret=0;
		while (nbytes--)
		{
			ret=(ret<<8)|(*ptr++);
		}
		return ret;
	}
	else
	{
		uint32_t ret=0;
		ptr+=nbytes;
		while (nbytes--)
		{
			ret=(ret<<8)|(*--ptr);
		}
		return ret;
	}
}

static void writemem(unsigned char * ptr, unsigned int nbytes, bool bigendian, uint32_t value)
{
	if (nbytes==1)
	{
		*ptr=value;
		return;
	}
	if (bigendian)
	{
		while (nbytes--)
		{
			(*ptr++)=value;
			value>>=8;
		}
	}
	else
	{
		ptr+=nbytes;
		while (nbytes--)
		{
			(*--ptr)=value;
			value>>=8;
		}
	}
}

static const uint32_t signs[]={0xFFFFFF80, 0xFFFF8000, 0xFF800000, 0x80000000};

static uint32_t signex(uint32_t val, unsigned int nbytes, bool signextend)
{
	if (signextend && (val&signs[nbytes-1])) return (val|signs[nbytes-1]);
	else return val;
}

static uint32_t readmemext(const unsigned char * ptr, unsigned int nbytes, bool bigendian, bool signextend)
{
	return signex(readmem(ptr, nbytes, bigendian), nbytes, signextend);
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
	uint8_t * ptr;
	uint8_t * prev;
	
#define SIZET_BITS (sizeof(size_t)*8)
	size_t * show;//the top row in each of those has value 1; bottom is 0x80000000
#define SIZE_PAGE_LOW 0x2000//2^13, 8192
	uint16_t * show_treelow;//one entry per SIZE_PAGE_LOW bits of mem, counts number of set bits within these bytes
#define SIZE_PAGE_HIGH 0x400000//2^22, 1048576
	uint32_t * show_treehigh;//same format as above
	size_t show_tot;//this applies to the entire memory block
	//both page sizes must be powers of 2
	//LOW must be in the range [SIZET_BITS .. 32768]
	//HIGH must be larger than LOW, and equal to or lower than 2^31
	
	size_t len;
	//No attempt has been made to care about performance for mem blocks larger than 2^32; I don't think there are any of those.
	//That, and performance will be trash at other places too for large mem blocks. Manipulating 2^29 bytes in 'show' isn't cheap,
	// and neither is manipulating the actual 2^32 bytes.
	
	unsigned int addrspace;
	
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

struct addressspace {
	char name[9];
	
	uint8_t addrlen;//how many hex digits are needed for an address here (1..16, 6 for SNES; the actual number of bits isn't used)
	//char padding[2];
	
	unsigned int nummap;
	struct mapping * map;
};


struct cheat_impl {
	unsigned int memid;
	//char padding[4];
	size_t offset;
	
	unsigned int changetype :2;
	unsigned int datsize :3;//only values 1..4 are allowed, but it's easier to give an extra bit than adding 1 on every use.
	bool issigned :1;
	bool enabled :1;
	bool restore :1;
	//char padding2[2];
	
	uint32_t value;//for cht_const: value it's forced to remain at
	               //for inconly/deconly: value of previous frame
	               //for once: cht_once does not get a cheat_impl
	uint32_t orgvalue;//value to restore to if the cheat is disabled
	
	char* desc;
};


enum { threadfunc_nothing, threadfunc_search };
struct minircheats_model_impl {
	struct minircheats_model i;
	
	struct addressspace * addrspaces;
	unsigned int numaddrspace;
	
	unsigned int nummem;
	struct memblock * mem;
	
	uint8_t search_datsize;
	bool search_signed;
	bool prev_enabled;
	bool addrspace_case_sensitive;
	
	size_t search_lastrow;
	size_t search_lastmempos;
	
	void* addrcache_ptr;
	size_t addrcache_start;
	size_t addrcache_end;
	size_t addrcache_offset;
	
	struct cheat_impl * cheats;
	unsigned int numcheats;
	unsigned int search_lastblock;//moved elsewhere due to alignment
	
	unsigned char numthreads;//1 for no threading.
	unsigned char threadfunc;
	
	unsigned char threadsearch_compfunc;
	bool threadsearch_comptoprev;
	uint32_t threadsearch_compto;
	
	char * lastcheat;
};



static void free_mem(struct minircheats_model_impl * this);
static void free_cheats(struct minircheats_model_impl * this);

static void set_memory(struct minircheats_model * this_, const struct retro_memory_descriptor * memory, unsigned int nummemory)
{
for (unsigned int i=0;i<nummemory;i++)
{
printf("desc: fl=%X pt=%p of=%zx st=%zx se=%zx di=%zx le=%zx sp=%s\n",
(unsigned int)memory[i].flags,
memory[i].ptr,
memory[i].offset,
memory[i].start,
memory[i].select,
memory[i].disconnect,
memory[i].len,
memory[i].addrspace);
}
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	free_mem(this);
	free_cheats(this);
	this->addrspace_case_sensitive=false;
	//all the weird math in this file is explained at the top
	for (unsigned int i=0;i<nummemory;i++)
	{
		const struct retro_memory_descriptor * desc=&memory[i];
		
		struct addressspace * addr;
		unsigned int addrspace;
		for (addrspace=0;addrspace<this->numaddrspace;addrspace++)
		{
			if (!strcmp(this->addrspaces[addrspace].name, desc->addrspace ? desc->addrspace : "")) break;
		}
		if (addrspace == this->numaddrspace)
		{
			this->numaddrspace++;
			this->addrspaces=(struct addressspace*)realloc(this->addrspaces, sizeof(struct addressspace)*this->numaddrspace);
			addr=&this->addrspaces[addrspace];
			strcpy(addr->name, desc->addrspace ? desc->addrspace : "");
			for (int i=0;addr->name[i];i++)
			{
				if (islower(addr->name[i])) this->addrspace_case_sensitive=true;
				if (!isalnum(addr->name[i]) && addr->name[i]!='_' && addr->name[i]!='-') abort();//invalid character in the name
			}
			addr->nummap=0;
			addr->map=NULL;
		}
		addr=&this->addrspaces[addrspace];
		
		struct memblock * mem;
		unsigned int memid;
		for (memid=0;memid<this->nummem;memid++)
		{
			if (this->mem[memid].ptr==desc->ptr) break;
		}
		if (memid == this->nummem)
		{
			this->nummem++;
			this->mem=(struct memblock*)realloc(this->mem, sizeof(struct memblock)*this->nummem);
			mem=&this->mem[memid];
			memset(mem, 0, sizeof(struct memblock));
			mem->ptr=(uint8_t*)desc->ptr;
			mem->prev=NULL;
			mem->len=0;
			mem->showinsearch=!(desc->flags & RETRO_MEMDESC_CONST);
			mem->align=false;//TODO
			mem->bigendian=(desc->flags & RETRO_MEMDESC_BIGENDIAN);
			mem->addrspace=addrspace;
		}
		mem=&this->mem[memid];
		if (desc->len > mem->len) mem->len=desc->len;
		
		addr->nummap++;
		addr->map=(struct mapping*)realloc(addr->map, sizeof(struct mapping)*addr->nummap);
		struct mapping * map=&addr->map[addr->nummap-1];
		map->memid=memid;
		map->start=desc->start;
		map->select=desc->select;
		map->disconnect=desc->disconnect;
		map->len=desc->len;
		map->offset=desc->offset;
	}
	
	//at this point:
	//addr->addrlen is uninitialized
	//map->select is possibly zero
	//map->len is possibly zero
	//map->variable_bits is uninitialized
	//map->disconnect_mask is uninitialized
	//map->has_overlaps is false
	//other things are correct
	
	for (unsigned int i=0;i<this->numaddrspace;i++)
	{
		struct addressspace * addr=&this->addrspaces[i];
		size_t top_addr=1;//to avoid trouble if the size is 0. A zero-bit address space isn't an address space at all, anyways.
		for (unsigned int i=0;i<addr->nummap;i++)
		{
			struct mapping * map=&addr->map[i];
			top_addr|=map->select;
			
			//start+len is garbage if the length isn't a power of 2, if disconnect is nonzero, or if len is larger than select
			//but in that case, select is the one we want, so we can ignore this.
			if (map->select==0) top_addr |= map->start+map->len-1;
		}
		top_addr=add_bits_down(top_addr);
		
		for (unsigned int i=0;i<addr->nummap;i++)
		{
			struct mapping * map=&addr->map[i];
			if (map->select==0)
			{
				if (map->len==0) abort();//select==0 and len==0 is bad
				if (map->len & (map->len-1)) abort();//select==0 and len not power of two
				map->select=top_addr&~inflate(add_bits_down(map->len-1), map->disconnect);
			}
			if (!map->len)
			{
				map->len=add_bits_down(reduce(top_addr&~map->select, map->disconnect))+1;
			}
			if (map->len > this->mem[map->memid].len) this->mem[map->memid].len=map->len;
			if (map->start & ~map->select) abort();//this combination is invalid
			
			while (reduce(top_addr&~map->select, map->disconnect)>>1 > map->len-1)
			{
				map->disconnect|=highest_bit(top_addr&~map->select&~map->disconnect);
			}
			
			map->variable_bits=(map->disconnect&~map->select);
			
			//this may look like it can be rewritten as topbit(len)<<popcount(disconnect & topbit(len)-1);,
			// but it'd be painful if the shift adds more disconnected bits
			if (map->len & (map->len-1)) map->variable_bits|=highest_bit(inflate(map->len, map->disconnect));
			
			map->disconnect_mask=add_bits_down(map->len-1);
			map->disconnect&=map->disconnect_mask;
			while ((~map->disconnect_mask)>>1 & map->disconnect)
			{
				map->disconnect_mask >>= 1;
				map->disconnect &= map->disconnect_mask;
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
		
		addr->addrlen=div_rndup(popcountS(top_addr), 4);
	}
}

static bool addr_guest_to_phys(struct minircheats_model_impl * this, unsigned int addrspace, size_t addr, unsigned int * memid, size_t * offset)
{
	for (unsigned int i=0;i<this->addrspaces[addrspace].nummap;i++)
	{
		struct mapping * map=&this->addrspaces[addrspace].map[i];
		if (((map->start ^ addr) & map->select) == 0)
		{
			struct memblock * mem=&this->mem[map->memid];
			if (!mem->ptr) return false;
			addr=reduce((addr - map->start)&map->disconnect_mask, map->disconnect);
			if (addr >= mem->len) addr-=highest_bit(addr);
			*memid = map->memid;
			*offset = addr+map->offset;
			return true;
		}
	}
	return false;
}

static size_t addr_phys_to_guest(struct minircheats_model_impl * this, void* ptr, size_t offset, unsigned int addrspace)
//this one can't fail
{
	if (ptr==this->addrcache_ptr && offset>=this->addrcache_start && offset<this->addrcache_end)
	{
		return offset-this->addrcache_start+this->addrcache_offset;
	}
	struct mapping * map=this->addrspaces[addrspace].map;
	while (true)
	{
		//there is no end condition if this byte shows up nowhere - such a situation is forbidden
		struct memblock * mem=&this->mem[map->memid];
		size_t thisaddr;
		if (mem->ptr!=ptr || map->offset>offset || offset-map->offset > map->len) goto wrongmapping;
		thisaddr=inflate(offset, map->disconnect)+map->start;
		if (false)
		{
		add_a_bit: ;
			size_t canadd=(~thisaddr & map->variable_bits);//bits that can change
			if (!canadd) goto wrongmapping;
			canadd&=-canadd;//bit that should change
			thisaddr|=canadd;
			thisaddr&=~((canadd-1) & map->variable_bits);//clear lower bits
			
			//check if adding that bit allows len to screw us over
			size_t tryaddr=reduce((thisaddr - map->start)&map->disconnect_mask, map->disconnect);
			if (tryaddr >= mem->len) goto add_a_bit;
		}
		if (map->has_overlaps)
		{
			struct mapping * prevmap=this->addrspaces[addrspace].map;
			while (prevmap < map)
			{
				if (((prevmap->start ^ thisaddr) & prevmap->select) == 0)
				{
					goto add_a_bit;
				}
				prevmap++;
			}
		}
//TODO:
//  find how long this mapping is linear, that is how far we can go while still ensuring that moving one guest byte still moves one physical byte
//   (use the most strict of 'length', 'disconnect', 'select', and other mappings)
//  put start and size of linear area, as well as where it points, in the cache
//  return this address, ignoring the above linearity calculations
//void* addrcache_ptr;
//size_t addrcache_start;
//size_t addrcache_end;
//size_t addrcache_offset;
		return thisaddr;
	wrongmapping:
		map++;
		continue;
	}
}

static bool addr_parse(struct minircheats_model_impl * this, const char * rawaddr, const char * * remainder, unsigned int * addrspace, size_t * addr)
{
	unsigned int minblklen=0;
	for (unsigned int i=0;isalpha(rawaddr[i]);i++)
	{
		if (rawaddr[i]<'A' || rawaddr[i]>'F') minblklen=i;
	}
	if (this->addrspace_case_sensitive)
	{
		for (unsigned int i=0;i<this->numaddrspace;i++)
		{
			size_t addrnamelen=strlen(this->addrspaces[i].name);
			if (addrnamelen>=minblklen && !strncmp(rawaddr, this->addrspaces[i].name, addrnamelen))
			{
				*addrspace=i;
				*addr=strtoul(rawaddr+addrnamelen, (char**)remainder, 16);
				return true;
			}
		}
	}
	for (unsigned int i=0;i<this->numaddrspace;i++)
	{
		size_t addrnamelen=strlen(this->addrspaces[i].name);
		if (addrnamelen>=minblklen && !strncasecmp(rawaddr, this->addrspaces[i].name, addrnamelen))
		{
			*addrspace=i;
			*addr=strtoul(rawaddr+addrnamelen, (char**)remainder, 16);
			return true;
		}
	}
	return false;
}



static void search_prev_set_to_cur(struct minircheats_model_impl * this)
{
	for (unsigned int i=0;i<this->nummem;i++)
	{
		if (this->mem[i].prev) memcpy(this->mem[i].prev, this->mem[i].ptr, this->mem[i].len);
	}
}

static size_t prev_get_size(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	size_t size=0;
	for (unsigned int i=0;i<this->nummem;i++)
	{
		if (this->mem[i].showinsearch) size+=this->mem[i].len;
	}
	return size;
}

static void prev_set_enabled(struct minircheats_model * this_, bool enable)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	for (unsigned int i=0;i<this->nummem;i++)
	{
		free(this->mem[i].prev);
		this->mem[i].prev=NULL;
	}
	this->prev_enabled=enable;
	for (unsigned int i=0;i<this->nummem;i++)
	{
		if (this->mem[i].showinsearch) this->mem[i].prev=(uint8_t*)malloc(this->mem[i].len);
	}
	search_prev_set_to_cur(this);
}

static bool prev_get_enabled(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	return this->prev_enabled;
}



static void search_get_pos(struct minircheats_model_impl * this, size_t visrow, unsigned int * memblk, size_t * mempos)
{
	//TODO: can searching for lastrow+1 be made faster?
	if (visrow==this->search_lastrow)
	{
		*memblk=this->search_lastblock;
		*mempos=this->search_lastmempos;
		return;
	}
	this->search_lastrow=visrow;
	
	struct memblock * mem=this->mem;
	while (visrow >= mem->show_tot)
	{
		visrow-=mem->show_tot;
		mem++;
	}
	
	*memblk = (mem - this->mem);
	
	size_t bigpage=0;
	while (visrow >= mem->show_treehigh[bigpage])
	{
		visrow-=mem->show_treehigh[bigpage];
		bigpage++;
	}
	
	size_t smallpage = bigpage*(SIZE_PAGE_HIGH/SIZE_PAGE_LOW);
	while (visrow >= mem->show_treelow[smallpage])
	{
		visrow-=mem->show_treelow[smallpage];
		smallpage++;
	}
	
	size_t * bits = mem->show + smallpage*(SIZE_PAGE_LOW/SIZET_BITS);
	while (true)
	{
		unsigned int bitshere=popcountS(*bits);
		if (visrow >= bitshere)
		{
			bits++;
			visrow-=bitshere;
		}
		else break;
	}
	
	size_t lastbits=*bits;
	unsigned int lastbitcount=0;
	while (visrow || !(lastbits&1))
	{
		if (lastbits&1) visrow--;
		lastbits>>=1;
		lastbitcount++;
	}
	
	*mempos = (bits - mem->show)*SIZET_BITS + lastbitcount;
	
	this->search_lastblock=*memblk;
	this->search_lastmempos=*mempos;
}

static void search_show_all(struct memblock * mem, unsigned int datsize)
{
	if (!mem->show) return;
	
	unsigned int len=div_rndup(mem->len, SIZET_BITS);
	memset(mem->show, 0xFF, len*sizeof(size_t));
	mem->show[len-1]>>=(mem->len&(SIZET_BITS-1));
	
	len=div_rndup(mem->len, SIZE_PAGE_LOW);
	for (unsigned int j=0;j<len;j++) mem->show_treelow[j]=SIZE_PAGE_LOW;
	if (mem->len&(SIZE_PAGE_LOW-1)) mem->show_treelow[len-1]=mem->len&(SIZE_PAGE_LOW-1);
	
	len=div_rndup(mem->len, SIZE_PAGE_HIGH);
	for (unsigned int j=0;j<len;j++) mem->show_treehigh[j]=SIZE_PAGE_HIGH;
	if (mem->len&(SIZE_PAGE_HIGH-1)) mem->show_treehigh[len-1]=mem->len&(SIZE_PAGE_HIGH-1);
	
	mem->show_tot=mem->len;
}

static void search_ensure_mem_exists(struct minircheats_model_impl * this)
{
	for (unsigned int i=0;i<this->nummem;i++)
	{
		struct memblock * mem=&this->mem[i];
		if (mem->showinsearch && !mem->show)
		{
			mem->show=(size_t*)malloc(div_rndup(mem->len, SIZET_BITS)*sizeof(size_t));
			mem->show_treelow=(uint16_t*)malloc(div_rndup(mem->len, SIZE_PAGE_LOW)*sizeof(uint16_t));
			mem->show_treehigh=(uint32_t*)malloc(div_rndup(mem->len, SIZE_PAGE_HIGH)*sizeof(uint32_t));
			
			search_show_all(mem, this->search_datsize);
		}
	}
}

static void search_reset(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	for (unsigned int i=0;i<this->nummem;i++)
	{
		struct memblock * mem=&this->mem[i];
		search_show_all(mem, this->search_datsize);
		if (mem->prev) memcpy(mem->prev, mem->ptr, mem->len);
	}
}

static void search_set_datsize(struct minircheats_model * this_, unsigned int datsize)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	this->search_datsize=datsize;
	//TODO: update the ghost values that change due to data size
}

static void search_set_signed(struct minircheats_model * this_, bool issigned)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	this->search_signed=issigned;
}

static void thread_do_search(struct minircheats_model_impl * this, unsigned int threadid);
static void thread_finish_search(struct minircheats_model_impl * this);
static void search_do_search(struct minircheats_model * this_, enum cheat_compfunc compfunc, bool comptoprev, uint32_t compto)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	search_ensure_mem_exists(this);
	
	this->threadsearch_compfunc=compfunc;
	this->threadsearch_comptoprev=comptoprev;
	this->threadsearch_compto=compto;
	
	uint32_t signadd=0;
	if (this->search_signed)
	{
		signadd=signs[this->search_datsize-1];
	}
	this->threadsearch_compto-=signadd;
	
	if (this->numthreads==1)
	{
		thread_do_search(this, 0);
		thread_finish_search(this);
	}
	else this->threadfunc=threadfunc_search;
}

#ifdef __SSE2__
#include <emmintrin.h>
#define FAST_ALIGN 1
#else
#if defined(__x86_64__) || defined(__i386__) || defined(__i486__) || defined(__i686__)
#define FAST_ALIGN 1
#else
#define FAST_ALIGN sizeof(size_t)
#endif
//this constant will, when multiplied by a value of the form 0000000a 0000000b 0000000c 0000000d (native endian), transform
//it into abcd???? ???????? ????????? ???????? (big endian), which can then be shifted down
//works for uniting up to 8 bits
//works on crazy-endian systems too
//does, however, assume a rather powerful optimizer
static size_t calc_bit_shuffle_constant()
{
	union {
		uint8_t a[8];
		size_t b;
	} v;
	v.a[0]=0x80; v.a[1]=0x40; v.a[2]=0x20; v.a[3]=0x10;
	v.a[4]=0x08; v.a[5]=0x04; v.a[6]=0x02; v.a[7]=0x01;
	//v.a[0]=0x01; v.a[1]=0x02; v.a[2]=0x04; v.a[3]=0x08;
	//v.a[4]=0x10; v.a[5]=0x20; v.a[6]=0x40; v.a[7]=0x80;
	return v.b;
}
#endif

static void thread_do_search(struct minircheats_model_impl * this, unsigned int threadid)
{
	uint8_t compfunc = this->threadsearch_compfunc;
	bool comptoprev = this->threadsearch_comptoprev;
	uint32_t compto = this->threadsearch_compto;
	
	//enum cheat_compfunc { cht_lt, cht_gt, cht_lte, cht_gte, cht_eq, cht_neq };
	unsigned char compfunc_perm[]={cht_lt, cht_lte, cht_lte, cht_lt, cht_eq, cht_eq};
	bool compfunc_exp=(compfunc&1);
	unsigned char compfunc_fun=compfunc_perm[compfunc];
	
	unsigned int datsize=this->search_datsize;//caching this here gives a ~15% speed boost
	
	uint32_t signadd=0;
	if (this->search_signed)
	{
		signadd=signs[this->search_datsize-1];
	}
	
#if __SSE2__
	//SSE comparisons are signed; we'll have to flip the sign if we prefer unsigned. (Or if we want signed, XOR with zero.)
	__m128i signflip=_mm_set1_epi8(this->search_signed ? 0x00 : 0x80);
#else
	size_t bitmerge=calc_bit_shuffle_constant();
	size_t compto_byterep=compto*(~(size_t)0/255);
	size_t signadd_byterep=signadd*(~(size_t)0/255);
#endif
	
	unsigned int workid=0;
	for (unsigned int i=0;i<this->nummem;i++)
	{
		struct memblock * mem=&this->mem[i];
		if (mem->show_tot==0) continue;
		
		bool bigendian=mem->bigendian;
		
		size_t pagepos=0;
		while (pagepos<mem->len)
		{
			if (threadid==workid && mem->show_treehigh[pagepos/SIZE_PAGE_HIGH]!=0)
			{
				size_t worklen=SIZE_PAGE_HIGH;
				if (worklen+SIZE_PAGE_HIGH > mem->len) worklen=mem->len-pagepos;
				
				size_t pos=0;
				while (pos<worklen)
				{
					if (!(pos&SIZE_PAGE_LOW) && mem->show_treelow[(pagepos+pos)/SIZE_PAGE_LOW]==0)
					{
						pos+=SIZE_PAGE_LOW;
						continue;
					}
					size_t show=mem->show[(pagepos+pos)/SIZET_BITS];
					if (show==0)
					{
						pos+=SIZET_BITS;
						continue;
					}
					
					const unsigned char * ptr=mem->ptr+pagepos+pos;
					const unsigned char * ptrprev=mem->prev+pagepos+pos;
					
//printf("%i %zx/%zx %i\n",datsize,pagepos+pos+SIZET_BITS,mem->len,(((uintptr_t)ptr)&(FAST_ALIGN-1)));exit(0);
					if (datsize==1 && pagepos+pos+SIZET_BITS <= mem->len && (((uintptr_t)ptr)&(FAST_ALIGN-1)) == 0)
					{
						unsigned int deleted=0;
#if __SSE2__
						size_t eq=0;
						size_t lt=0;
						__m128i* ptrS=(__m128i*)ptr;
						size_t keep;
						if (comptoprev)
						{
							__m128i* ptrprevS=(__m128i*)ptrprev;
							for (int i=0;i<SIZET_BITS/16;i++)
							{
								__m128i a=_mm_loadu_si128(ptrS);
								__m128i b=_mm_load_si128(ptrprevS);
								
								a=_mm_xor_si128(a, signflip);
								b=_mm_xor_si128(b, signflip);
								
								if (compfunc_fun<=cht_lte) keep |= _mm_movemask_epi8(_mm_cmplt_epi8(a, b)) << ((size_t)i*16);
								if (compfunc_fun>=cht_lte) keep |= _mm_movemask_epi8(_mm_cmpeq_epi8(a, b)) << ((size_t)i*16);
								
								ptrS++;
								ptrprevS++;
							}
						}
						else
						{
							for (int i=0;i<SIZET_BITS/16;i++)
							{
								__m128i a=_mm_loadu_si128(ptrS);
								__m128i b=_mm_set1_epi8(compto);
								
								a=_mm_xor_si128(a, signflip);
								b=_mm_xor_si128(b, signflip);
								
								if (compfunc_fun<=cht_lte) keep |= _mm_movemask_epi8(_mm_cmplt_epi8(a, b)) << ((size_t)i*16);
								if (compfunc_fun>=cht_lte) keep |= _mm_movemask_epi8(_mm_cmpeq_epi8(a, b)) << ((size_t)i*16);
								
								ptrS++;
							}
						}
						
						keep^=-compfunc_exp;
						deleted=popcountS(show&~keep);
						show&=keep;
#else
						const size_t* ptrS=(size_t*)ptr;
						const size_t* ptrprevS=(size_t*)ptrprev;
						size_t neq=0;
						size_t lte=0;
						for (unsigned int bits=0;bits<SIZET_BITS;bits+=sizeof(size_t))
						{
							//warning - ugly math ahead
//repeated 16bit pattern
#define rep16(x) (~(size_t)0/65535*(x))
#define rep8(x) rep16((x)*0x0101)
							
							STATIC_ASSERT(sizeof(size_t)<=8, fix_this_function);
							
							size_t val1=*(ptrS++);
							size_t val2=(comptoprev ? *(ptrprevS++) : compto_byterep);
							
							val1+=signadd_byterep;
							val2+=signadd_byterep;
							
							
							size_t tmp=(val1^val2);
							//tmp now contains nonzero for different bytes, and zero for same bytes
							tmp|=tmp>>4;
							tmp|=tmp>>2;
							tmp|=tmp>>1;
							tmp&=rep8(0x01);
							//tmp now contains 01 for different bytes, and 00 for same bytes
							neq |= (tmp*bitmerge) >> (sizeof(size_t)*(8-1)) << bits;
							
							
							//compare half of the values at the time; we need a ninth bit for each compared byte,
							// and the only real way to do that is to do half at the time.
							size_t tmp1=(val1 &  rep16(0x00FF));
							size_t tmp2=(val2 | ~rep16(0x00FF));
							size_t lte_bits = (tmp2-tmp1)>>8 & rep16(0x0001);
							
							tmp1=(val1>>8 &  rep16(0x00FF));
							tmp2=(val2>>8 | ~rep16(0x00FF));
							lte_bits |= (tmp2-tmp1) & rep16(0x0100);
							
							lte |= (lte_bits*bitmerge) >> (sizeof(size_t)*(8-1)) << bits;
						}
						
						size_t remove;
						if (compfunc_fun==cht_eq) remove=neq;//we'll add tilde to both the others, in exchange for not having tilde on equal
						if (compfunc_fun==cht_lt) remove=~(neq&lte);
						if (compfunc_fun==cht_lte) remove=~lte;
						remove^=-compfunc_exp;
						deleted=popcountS(show&remove);
						show&=~remove;
#endif
						mem->show_treehigh[(pos+pagepos)/SIZE_PAGE_HIGH]-=deleted;
						mem->show_treelow[(pos+pagepos)/SIZE_PAGE_LOW]-=deleted;
						mem->show[(pos+pagepos)/SIZET_BITS]=show;
						pos+=SIZET_BITS;
					}
					else//TODO: speed this up once I've figured out which data sizes I want.
					{
						unsigned int deleted=0;
						for (size_t bit=0;bit<SIZET_BITS;bit++)
						{
							if (show & (1<<bit))
							{
								uint32_t val;
								val=readmem(ptr+bit, datsize, bigendian);//not readmemext - we're handling the sign ourselves
								
								uint32_t other;
								if (comptoprev) other=readmem(ptrprev+bit, datsize, bigendian);
								else other=compto;
								
								val+=signadd;//unsigned overflow is defined to wrap
								other+=signadd;//it'll blow up if the child system doesn't use two's complement, but I don't think there are any of those.
								
								bool res=false;//yes, this is ugly; it's faster this way
								if (compfunc_fun<=cht_lte) res|=(val<other);
								if (compfunc_fun>=cht_lte) res|=(val==other);
								if (res == compfunc_exp)
								{
									show&=~(1<<bit);
									deleted++;
								}
							}
						}
						mem->show_treehigh[(pos+pagepos)/SIZE_PAGE_HIGH]-=deleted;
						mem->show_treelow[(pos+pagepos)/SIZE_PAGE_LOW]-=deleted;
						mem->show[(pos+pagepos)/SIZET_BITS]=show;
						pos+=SIZET_BITS;
					}
				}
				if (mem->prev) memcpy(mem->prev+pagepos, mem->ptr+pagepos, worklen);
			}
			workid=(workid+1)%this->numthreads;
			pagepos+=SIZE_PAGE_HIGH;
		}
	}
}

static void thread_finish_search(struct minircheats_model_impl * this)
{
	for (unsigned int i=0;i<this->nummem;i++)
	{
		struct memblock * mem=&this->mem[i];
		if (mem->show_tot==0) continue;
		mem->show_tot=0;
		for (unsigned int i=0;i<=(mem->len-1)/SIZE_PAGE_HIGH;i++)
		{
			mem->show_tot+=mem->show_treehigh[i];
		}
	}
}

static size_t search_get_num_rows(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	search_ensure_mem_exists(this);
	size_t numrows=0;
	for (unsigned int i=0;i<this->nummem;i++) numrows+=this->mem[i].show_tot;
	return numrows;
}

static void search_get_vis_row(struct minircheats_model * this_, unsigned int row, char * addr, uint32_t * val, uint32_t * prevval)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	
	unsigned int memblk;
	size_t mempos;
	search_get_pos(this, row, &memblk, &mempos);
	struct memblock * mem=&this->mem[memblk];
	
	if (addr)
	{
		size_t p_addr=addr_phys_to_guest(this, mem->ptr, mempos, mem->addrspace);
		sprintf(addr, "%s%.*lX", this->addrspaces[mem->addrspace].name, this->addrspaces[mem->addrspace].addrlen, p_addr);
	}
	if (val)     *val  =  readmemext(mem->ptr +mempos, this->search_datsize, mem->bigendian, this->search_signed);
	if (prevval) *prevval=readmemext(mem->prev+mempos, this->search_datsize, mem->bigendian, this->search_signed);
}





static void thread_enable(struct minircheats_model * this_, unsigned int numthreads)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	this->numthreads=numthreads;
}

static unsigned int thread_get_count(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	return this->numthreads;
}

static void thread_do_work(struct minircheats_model * this_, unsigned int threadid)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	switch (this->threadfunc)
	{
		case threadfunc_nothing: break;
		case threadfunc_search: thread_do_search(this, threadid); break;
	}
}

static void thread_finish_work(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	switch (this->threadfunc)
	{
		case threadfunc_nothing: break;
		case threadfunc_search: thread_finish_search(this); break;
	}
}





//TODO
static bool cheat_read(struct minircheats_model * this_, const char * addr, unsigned int datsize, uint32_t * val)
{
	return false;
}

//TODO
static int cheat_find_for_addr(struct minircheats_model * this_, unsigned int datsize, const char * addr)
{
	return -1;
}

static unsigned int cheat_get_count(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	return this->numcheats;
}



static bool cheat_set(struct minircheats_model * this_, int pos, const struct cheat * newcheat)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	const char * remainder;
	unsigned int addrspace;
	size_t addr;
	if (!addr_parse(this, newcheat->addr, &remainder, &addrspace, &addr)) return false;
	if (*remainder) return false;
	
	unsigned int memid;
	size_t offset;
	if (!addr_guest_to_phys(this, addrspace, addr, &memid, &offset)) return false;
//printf("conv %X->%X\n",addr,offset);
	
	if (pos<0) return true;
	
	if (newcheat->changetype == cht_once)
	{
		//FIXME: Just write to the given address.
		return true;
	}
	if (pos==this->numcheats)
	{
		this->numcheats++;
		this->cheats=(struct cheat_impl*)realloc(this->cheats, sizeof(struct cheat_impl)*this->numcheats);
		this->cheats[pos].desc=NULL;
		this->cheats[pos].enabled=false;
	}
	struct cheat_impl * cht=&this->cheats[pos];
	if (cht->enabled && cht->restore)
	{
		//TODO: Restore
	}
	cht->memid=memid;
	cht->offset=offset;
	cht->changetype=newcheat->changetype;
	cht->datsize=newcheat->datsize;
	cht->issigned=newcheat->issigned;
	cht->enabled=newcheat->enabled;
	cht->restore=true;
	cht->orgvalue=readmem(this->mem[cht->memid].ptr+cht->offset, cht->datsize, this->mem[cht->memid].bigendian);
	if (newcheat->changetype == cht_const) cht->value=newcheat->val;
	else cht->value=cht->orgvalue;
	if (cht->desc != newcheat->desc)
	{
		free(cht->desc);
		cht->desc=(newcheat->desc ? strdup(newcheat->desc) : NULL);
	}
	return true;
}

//TODO
static void cheat_get(struct minircheats_model * this_, unsigned int pos, struct cheat * newcheat)
{
}

static void cheat_remove(struct minircheats_model * this_, unsigned int pos)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	free(this->cheats[pos].desc);
	memmove(this->cheats+pos, this->cheats+pos+1, sizeof(struct cheat_impl)*(this->numcheats-pos-1));
	this->numcheats--;
}



static void cheat_apply(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	for (unsigned int i=0;i<this->numcheats;i++)
	{
		struct cheat_impl * cht=&this->cheats[i];
		if (!cht->enabled) continue;
		uint32_t signadd=(cht->issigned ? 0x80000000 : 0);
		if (cht->changetype!=cht_const)
		{
			uint32_t curval=readmem(this->mem[cht->memid].ptr+cht->offset, cht->datsize, this->mem[cht->memid].bigendian);
//printf("chg %X %X->%X\n",cht->offset,curval,cht->value);
			if (cht->changetype==cht_inconly && curval+signadd > cht->value+signadd) continue;
			if (cht->changetype==cht_deconly && curval+signadd < cht->value+signadd) continue;
//if (cht->changetype!=cht_const)
			cht->value=curval;
		}
		writemem(this->mem[cht->memid].ptr+cht->offset, cht->datsize, this->mem[cht->memid].bigendian, cht->value);
	}
}





//TODO: test
static const char * code_create(struct minircheats_model * this_, struct cheat * thecheat)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	free(this->lastcheat);
	//disable address signspec value direction SP desc
	this->lastcheat=(char*)malloc(1+strlen(thecheat->addr)+8+1+1+1+strlen(thecheat->desc)+1);
	//TODO: verify that addr points to anything
	const char * const chngtypenames[]={"", "+", "-", "."};
	sprintf(this->lastcheat, "%s%s%.*X%s%s%s%s", thecheat->enabled?"":"-", thecheat->addr,
	                         thecheat->datsize*2, thecheat->val, thecheat->issigned?"S":"", chngtypenames[thecheat->changetype],
	                         (thecheat->desc && *thecheat->desc) ? " " : "", (thecheat->desc ? thecheat->desc : ""));
	return this->lastcheat;
}

//TODO
static bool code_parse(struct minircheats_model * this, const char * code, struct cheat * thecheat)
{
	return false;
}





static void free_mem(struct minircheats_model_impl * this)
{
	for (unsigned int i=0;i<this->numaddrspace;i++)
	{
		free(this->addrspaces[i].map);
	}
	free(this->addrspaces);
	this->addrspaces=NULL;
	this->numaddrspace=0;
	
	for (unsigned int i=0;i<this->nummem;i++)
	{
		free(this->mem[i].prev);
		free(this->mem[i].show);
		free(this->mem[i].show_treelow);
		free(this->mem[i].show_treehigh);
		if (this->mem[i].align)
		{
			free(this->mem[i].show_true);
			free(this->mem[i].show_true_treelow);
			free(this->mem[i].show_true_treehigh);
		}
	}
	free(this->mem);
	this->mem=NULL;
	this->nummem=0;
}

static void free_cheats(struct minircheats_model_impl * this)
{
	for (unsigned int i=0;i<this->numcheats;i++)
	{
		free(this->cheats[i].desc);
	}
	this->cheats=NULL;
	this->numcheats=0;
}

static void free_(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	free_mem(this);
	free_cheats(this);
	free(this->lastcheat);
	free(this);
}

const struct minircheats_model_impl minircheats_model_base = {{
	set_memory,
	prev_get_size, prev_set_enabled, prev_get_enabled,
	search_reset, search_set_datsize, search_set_signed, search_do_search, search_get_num_rows, search_get_vis_row,
	thread_enable, thread_get_count, thread_do_work, thread_finish_work,
	cheat_read, cheat_find_for_addr, cheat_get_count,
	cheat_set, cheat_get,
	cheat_remove, cheat_apply,
	code_create, code_parse,
	free_
}};
struct minircheats_model * minircheats_create_model()
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)malloc(sizeof(struct minircheats_model_impl));
	memcpy(this, &minircheats_model_base, sizeof(struct minircheats_model_impl));
	this->numthreads=1;
	return (struct minircheats_model*)this;
}
