#include "minir.h"
//#undef malloc
//#undef realloc
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//#define malloc(x) malloc(x+128)
//#define realloc(x,y) realloc(x,y+128)

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
//  find how long this mapping is linear, that is how far we can go while still ensuring that moving one guest byte still moves one physical byte
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
	n=(add_bits_down(n)>>1)+1;
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

#define div_rndup(a,b) (((a)+(b)-1)/(b))

static uint32_t readmem(const unsigned char * ptr, unsigned int nbytes, bool bigendian)
{
	if (nbytes==1) return *ptr;
	if (bigendian)
	{
		unsigned int ret=0;
		while (nbytes--)
		{
			ret=(ret<<8)|(*ptr++);
		}
		return ret;
	}
	else
	{
		unsigned int ret=0;
		while (nbytes--)
		{
			ret|=(*ptr++)<<(nbytes*8);
		}
		return ret;
	}
}

//static void writemem(unsigned char * ptr, enum cheat_size nbytes, bool bigendian, uint32_t value)
//{
//	if (nbytes==1)
//	{
//		*ptr=value;
//		return;
//	}
//	__builtin_trap();
//}

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
	unsigned char * ptr;
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
	
	size_t len;
	//No attempt has been made to care about performance for mem blocks larger than 2^32; I don't think there are any of those.
	//That, and performance will be trash at other places too for large mem blocks. Manipulating 2^29 bytes in 'show' isn't cheap.
	
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

struct addrspace {
	char name[9];
	
	uint8_t addrlen;//equal to (numbits+3)/4
	unsigned int numbits;//valid values are 1..64; 24 for SNES
	
	unsigned int nummap;
	struct mapping * map;
};

struct minircheats_model_impl {
	struct minircheats_model i;
	
	struct addrspace * addrspaces;
	unsigned int numaddrspace;
	
	unsigned int nummem;
	struct memblock * mem;
	
	uint8_t search_datsize;
	bool search_signed;
	bool prev_enabled;
	//char padding[2];
	
	size_t search_lastmempos;
	size_t search_lastrow;
	
	void* addrcache_ptr;
	size_t addrcache_start;
	size_t addrcache_end;
	size_t addrcache_offset;
	unsigned int addrcache_addrspace;
	unsigned int search_lastblock;//moved to reduce padding
};



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
			this->mem=realloc(this->mem, sizeof(struct memblock)*this->nummem);
			mem=&this->mem[memid];
			memset(mem, 0, sizeof(struct memblock));
			mem->ptr=desc->ptr;
			mem->len=desc->len;
			mem->showinsearch=!(desc->flags & LIBRETRO_MEMFLAG_CONST);
			mem->align=(desc->flags & LIBRETRO_MEMFLAG_ALIGNED);
			mem->bigendian=(desc->flags & LIBRETRO_MEMFLAG_BIGENDIAN);
			mem->addrspace=addrspace;
		}
		mem=&this->mem[memid];
		
		addr->nummap++;
		addr->map=realloc(addr->map, sizeof(struct mapping)*addr->nummap);
		struct mapping * map=&addr->map[addr->nummap-1];
		map->memid=memid;
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
			
			//this may look like it can be rewritten as topbit(len)<<popcount(disconnect & topbit(len)-1);,
			// but it'd be painful if the shift adds more disconnected bits
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
		addr->addrlen=div_rndup(addr->numbits, 4);
	}
}

static bool conv_guest_phys(struct minircheats_model_impl * this, unsigned int addrspace, size_t addr, unsigned char * * ptr, size_t * out)
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
			*out = addr+map->offset;
			*ptr = mem->ptr;
			return true;
		}
	}
	return false;
}

static void conv_phys_guest(struct minircheats_model_impl * this, void* ptr, size_t offset, unsigned int * addrspace, size_t * addr)
//this one can't fail
{
	//void* addrcache_ptr;
	//size_t addrcache_start;
	//size_t addrcache_end;
	//size_t addrcache_offset;
	//unsigned int addrcache_addrspace;
	if (ptr==this->addrcache_ptr && offset>=this->addrcache_start && offset<this->addrcache_end)
	{
		*addrspace=this->addrcache_addrspace;
		*addr=offset-this->addrcache_start+this->addrcache_offset;
	}
(void)conv_guest_phys;
*addrspace=0;
*addr=offset;
	//for (unsigned int i=0;i<
}



static void prev_set_to_cur(struct minircheats_model_impl * this)
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
		if (this->mem[i].showinsearch) this->mem[i].prev=malloc(this->mem[i].len);
	}
	prev_set_to_cur(this);
}

static bool prev_get_enabled(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	return this->prev_enabled;
}





static void search_get_pos(struct minircheats_model_impl * this, size_t visrow, unsigned int * memblk, size_t * mempos)
{
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
	
	unsigned int bigpage=0;
	while (visrow >= mem->show_treehigh[bigpage])
	{
		visrow-=mem->show_treehigh[bigpage];
		bigpage++;
	}
	
	unsigned int smallpage = bigpage*(SIZE_PAGE_HIGH/SIZE_PAGE_LOW);
	while (visrow >= mem->show_treehigh[bigpage])
	{
		visrow-=mem->show_treehigh[bigpage];
		bigpage++;
	}
	
	uint32_t * bits = mem->show + smallpage*(SIZE_PAGE_LOW/32);
	while (true)
	{
		unsigned int bitshere=popcount32(*bits);
		if (visrow >= bitshere)
		{
			bits++;
			visrow-=bitshere;
		}
		else break;
	}
	
	uint32_t lastbits=*bits;
	unsigned int lastbitcount=0;
	while (visrow || !(lastbits&1))
	{
		if (lastbits&1) visrow--;
		lastbits>>=1;
		lastbitcount++;
	}
	
	*mempos = (bits - mem->show)*32 + lastbitcount;
	
	this->search_lastblock=*memblk;
	this->search_lastmempos=*mempos;
}

static void search_show_all(struct memblock * mem, unsigned int datsize)
{
	if (!mem->show) return;
	
	unsigned int len=div_rndup(mem->len, 32);
	memset(mem->show, 0xFF, len*sizeof(uint32_t));
	if (mem->len&31) mem->show[len-1]=0xFFFFFFFF>>(32-(mem->len&31));
	
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
			mem->show=malloc(div_rndup(mem->len, 32)*sizeof(uint32_t));
			mem->show_treelow=malloc(div_rndup(mem->len, SIZE_PAGE_LOW)*sizeof(uint16_t));
			mem->show_treehigh=malloc(div_rndup(mem->len, SIZE_PAGE_HIGH)*sizeof(uint32_t));
			
			search_show_all(mem, this->search_datsize);
		}
	}
}

static void search_reset(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	for (unsigned int i=0;i<this->nummem;i++)
	{
		search_show_all(&this->mem[i], this->search_datsize);
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

static void search_do_search(struct minircheats_model * this_, enum cheat_compfunc compfunc, bool comptoprev, unsigned int compto)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	search_ensure_mem_exists(this);
	
	uint32_t signadd=0;
	if (this->search_signed)
	{
		signadd=signs[this->search_datsize-1];
	}
	compto-=signadd;
	
	for (unsigned int i=0;i<this->nummem;i++)
	{
		struct memblock * mem=&this->mem[i];
		if (mem->show_tot==0) continue;
		unsigned int pos=0;
		while (pos<mem->len)
		{
			if (!(pos&SIZE_PAGE_HIGH) && mem->show_treehigh[pos/SIZE_PAGE_HIGH]==0)
			{
				pos+=SIZE_PAGE_HIGH;
				continue;
			}
			if (!(pos&SIZE_PAGE_LOW) && mem->show_treelow[pos/SIZE_PAGE_LOW]==0)
			{
				pos+=SIZE_PAGE_LOW;
				continue;
			}
			if (mem->show[pos/32]==0)
			{
				pos+=32;
				continue;
			}
			
			unsigned int show=mem->show[pos/32];
			unsigned int deleted=0;
			for (unsigned int bit=0;bit<32;bit++)
			{
				if (show & (1<<bit))
				{
					uint32_t val=readmem(mem->ptr+pos+bit, this->search_datsize, mem->bigendian);
					
					uint32_t other;
					if (comptoprev) other=readmem(mem->prev+pos+bit, this->search_datsize, mem->bigendian);
					else other=compto;
					
					val+=signadd;//unsigned overflow is defined to wrap
					other+=signadd;//it'll blow up if the child system doesn't use two's complement, but I don't think there are any of those.
					
					bool delete=false;
					if (compfunc==cht_lt)  delete=!(val<other);
					if (compfunc==cht_gt)  delete=!(val>other);
					if (compfunc==cht_lte) delete=!(val<=other);
					if (compfunc==cht_gte) delete=!(val>=other);
					if (compfunc==cht_eq)  delete=!(val==other);
					if (compfunc==cht_neq) delete=!(val!=other);
					if (delete)
					{
						show&=~(1<<bit);
						deleted++;
					}
				}
			}
			mem->show_tot-=deleted;
			mem->show_treehigh[pos/SIZE_PAGE_HIGH]-=deleted;
			mem->show_treelow[pos/SIZE_PAGE_LOW]-=deleted;
			mem->show[pos/32]=show;
			pos+=32;
		}
		if (mem->prev) memcpy(mem->prev, mem->ptr, mem->len);
	}
}

static unsigned int search_get_num_rows(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	search_ensure_mem_exists(this);
	unsigned int numrows=0;
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
		unsigned int addrspace;
		size_t p_addr;
		conv_phys_guest(this, mem->ptr, mempos, &addrspace, &p_addr);
		sprintf(addr, "%s%.*lX", this->addrspaces[addrspace].name, this->addrspaces[addrspace].addrlen, p_addr);
	}
	if (val)     *val  =  readmemext(mem->ptr +mempos, this->search_datsize, mem->bigendian, this->search_signed);
	if (prevval) *prevval=readmemext(mem->prev+mempos, this->search_datsize, mem->bigendian, this->search_signed);
}





static void delete_all_mem(struct minircheats_model_impl * this)
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

static void free_(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	delete_all_mem(this);
	//FIXME
	free(this);
}

const struct minircheats_model_impl minircheats_model_base = {{
	set_memory,
	prev_get_size, prev_set_enabled, prev_get_enabled,
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
