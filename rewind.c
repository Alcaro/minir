#include "minir.h"
#include <stdlib.h>
#include <string.h>

//Format per frame:
//size nextstart;
//repeat {
//  uint16 numchanged; // everything is counted in units of uint16
//  if (numchanged) {
//    uint16 numunchanged; // skip these before handling numchanged
//    uint16[numchanged] changeddata;
//  }
//  else
//  {
//    uint32 numunchanged;
//    if (!numunchanged) break;
//  }
//}
//size thisstart;
//
//The start offsets point to 'nextstart' of any given compressed frame.
//Each uint16 is stored native endian; anything that claims any other endianness refers to the endianness of this specific item.
//The uint32 is stored little endian.
//Each size value is stored native endian if alignment is not enforced; if it is, they're little endian.
//The start of the buffer contains a size pointing to the end of the buffer; the end points to its start.
//Wrapping is handled by returning to the start of the buffer if the compressed data could potentially hit the edge;
//if the compressed data could potentially overwrite the tail pointer, the tail retreats until it can no longer collide.
//This means that on average, ~2*maxcompsize is unused at any given moment.

#if __SSE2__
#define PADDING 32//2*sizeof(__m128i)
#else
#define PADDING sizeof(size_t)
#endif

struct rewindstack_impl {
	struct rewindstack i;
	
	char * data;
	size_t capacity;
	char * head;//Reading and writing is done here.
	char * tail;//If head comes close to this, discard a frame.
	
	char * thisblock;
	char * nextblock;
	bool thisblock_valid;
	
	size_t blocksize;//This one is rounded up from reset::blocksize.
	size_t maxcompsize;//size_t+(blocksize+131071)/131072*(blocksize+u16+u16)+u16+u32+size_t (yes, the math is a bit ugly)
	
	unsigned int entries;
};

static void reset(struct rewindstack * this_, size_t blocksize, size_t capacity)
{
	struct rewindstack_impl * this=(struct rewindstack_impl*)this_;
	
	int newblocksize=((blocksize-1)|(sizeof(uint16_t)-1))+1;
	if (this->blocksize!=newblocksize)
	{
		this->blocksize=newblocksize;
		
		const int maxcblkcover=UINT16_MAX*sizeof(uint16_t);
		const int maxcblks=(this->blocksize+maxcblkcover-1)/maxcblkcover;
		this->maxcompsize=this->blocksize + maxcblks*sizeof(uint16_t)*2 + sizeof(uint16_t)+sizeof(uint32_t) + sizeof(size_t)*2;
		
		free(this->thisblock);
		free(this->nextblock);
		this->thisblock=calloc(this->blocksize+sizeof(uint16_t)*4+PADDING, 1);
		this->nextblock=calloc(this->blocksize+sizeof(uint16_t)*4+PADDING, 1);
		//Force in a different byte at the end, so we don't need to check bounds in the innermost loop (it's expensive).
		//There is also a large amount of data that's the same, to stop the other scan
		//There is also some padding at the end. This is so we don't read outside the buffer end if we're reading in large blocks;
		// it doesn't make any difference to us, but sacrificing 16 bytes to get Valgrind happy is worth it.
		*(uint16_t*)(this->thisblock+this->blocksize+sizeof(uint16_t)*3)=0xFFFF;
		*(uint16_t*)(this->nextblock+this->blocksize+sizeof(uint16_t)*3)=0x0000;
	}
	
	if (capacity!=this->capacity)
	{
		free(this->data);
		this->data=malloc(capacity);
		this->capacity=capacity;
	}
	
	this->head=this->data+sizeof(size_t);
	this->tail=this->data+sizeof(size_t);
	
	this->thisblock_valid=false;
	
	this->entries=0;
}

static void * push_begin(struct rewindstack * this_)
{
	struct rewindstack_impl * this=(struct rewindstack_impl*)this_;
	//We need to ensure we have an uncompressed copy of the last pushed state, or we could
	// end up applying a 'patch' to wrong savestate, and that'd blow up rather quickly.
	if (!this->thisblock_valid)
	{
		if (this_->pull(this_))
		{
			this->thisblock_valid=true;
			this->entries++;
		}
	}
	return this->nextblock;
}

#if __SSE2__
	//The input can be anything in the range FFFF0001 to FFFFFFFF, inclusive. Other values, including 0, are known impossible.
#if defined(__GNUC__)
static inline int compat_ctz(unsigned int x)
{
	return __builtin_ctz(x);
}
#elif defined(_MSC_VER)
#error should be tested
//#include <intrin.h>
//#pragma intrinsic(_BitScanForward)
static inline int compat_ctz(unsigned int x)
{
	unsigned long index;
	_BitScanForward(&index, x);
	return index;
}
#else
static inline int compat_ctz(unsigned int x)
{
	// Only checks at nibble granularity, because that's what we need.
	if (x&0x00ff) return (x&0x000f) ? 0 : 4;
	else return (x&0x0f00) ? 8 : 12;
}
#endif

#include <emmintrin.h>
// There's no equivalent in libc, you'd think so ... std::mismatch exists, but it's not optimized at all. :(
static inline size_t find_change(const uint16_t * a, const uint16_t * b)
{
	const __m128i * a128=(const __m128i*)a;
	const __m128i * b128=(const __m128i*)b;
	
	while (true)
	{
		__m128i v0 = _mm_loadu_si128(a128);
		__m128i v1 = _mm_loadu_si128(b128);
		__m128i c = _mm_cmpeq_epi32(v0, v1);
		uint32_t mask = _mm_movemask_epi8(c);
		a128++;
		b128++;
		__m128i v0b = _mm_loadu_si128(a128);
		__m128i v1b = _mm_loadu_si128(b128);
		__m128i cb = _mm_cmpeq_epi32(v0b, v1b);
		uint32_t maskb = _mm_movemask_epi8(cb);
		if (mask != 0xffff || maskb != 0xffff) // Something has changed, figure out where.
		{
			if (mask == 0xffff) mask=maskb;
			else a128--;//ignore b128 since we'll return anyways
			size_t ret=(((char*)a128-(char*)a) | (compat_ctz(~mask))) >> 1;
			return (ret | (a[ret]==b[ret]));
		}
		a128++;
		b128++;
	}
}
#else
static inline size_t find_change(const uint16_t * a, const uint16_t * b)
{
	const uint16_t * a_org=a;
#ifdef NO_UNALIGNED_MEM
	while ((uintptr_t)a & (sizeof(size_t)-1) && *a==*b)
	{
		a++;
		b++;
	}
	if (*a==*b)
#endif
	{
		const size_t* a_big=(const size_t*)a;
		const size_t* b_big=(const size_t*)b;
		
		while (*a_big==*b_big)
		{
			a_big++;
			b_big++;
		}
		a=(const uint16_t*)a_big;
		b=(const uint16_t*)b_big;
		
		while (*a==*b)
		{
			a++;
			b++;
		}
	}
	return a-a_org;
}
#endif

static inline size_t find_same(const uint16_t * a, const uint16_t * b)
{
	const uint16_t * a_org=a;
#ifdef NO_UNALIGNED_MEM
	if ((uintptr_t)a & (sizeof(uint32_t)-1) && *a!=*b)
	{
		a++;
		b++;
	}
	if (*a!=*b)
#endif
	{
		//With this, it's random whether two consecutive identical words are caught.
		//Luckily, compression rate is the same for both cases, and three is always caught.
		//(We prefer to miss two-word blocks, anyways; it gives fewer iterations of the outer loop, as well as in the decompressor.)
		const uint32_t* a_big=(const uint32_t*)a;
		const uint32_t* b_big=(const uint32_t*)b;
		
		while (*a_big!=*b_big)
		{
			a_big++;
			b_big++;
		}
		a=(const uint16_t*)a_big;
		b=(const uint16_t*)b_big;
		
		if (a!=a_org && a[-1]==b[-1])
		{
			a--;
			b--;
		}
	}
	return a-a_org;
}

static void push_end(struct rewindstack * this_)
{
	struct rewindstack_impl * this=(struct rewindstack_impl*)this_;
	if (this->thisblock_valid)
	{
		if (this->capacity<sizeof(size_t)+this->maxcompsize) return;
		
	recheckcapacity:;
		size_t headpos=(this->head-this->data);
		size_t tailpos=(this->tail-this->data);
		size_t remaining=(tailpos+this->capacity-sizeof(size_t)-headpos-1)%this->capacity + 1;
		if (remaining<=this->maxcompsize)
		{
			size_t tailpos;
			memcpy(&tailpos, this->tail, sizeof(size_t));
			this->tail=this->data + tailpos;
			this->entries--;
			goto recheckcapacity;
		}
		
		const char* old=this->thisblock;
		const char* new=this->nextblock;
		char* compressed=this->head+sizeof(size_t);
		
		//Begin compression code; 'compressed' will point to the end of the compressed data (excluding the prev pointer).
		const uint16_t * old16=(const uint16_t*)old;
		const uint16_t * new16=(const uint16_t*)new;
		uint16_t * compressed16=(uint16_t*)compressed;
		size_t num16s=this->blocksize/sizeof(uint16_t);
		while (num16s)
		{
			size_t skip=find_change(old16, new16);
			//size_t skip=find_change_b(old16, new16);
			//if (skip!=skip2) abort();
			
			if (skip>=num16s) break;
			old16+=skip;
			new16+=skip;
			num16s-=skip;
			
			if (skip>UINT16_MAX)
			{
				if (skip>UINT32_MAX)
				{
					//This will make it scan the entire thing again, but it only hits on 8GB unchanged
					//data anyways, and if you're doing that, you've got bigger problems.
					skip=UINT32_MAX;
				}
				*(compressed16++)=0;
				*(compressed16++)=skip;
				*(compressed16++)=skip>>16;
				skip=0;
				continue;
			}
			
			size_t changed=find_same(old16, new16);
			if (changed>UINT16_MAX) changed=UINT16_MAX;
			*(compressed16++)=changed;
			*(compressed16++)=skip;
			for (int i=0;i<changed;i++) compressed16[i]=old16[i];
			old16+=changed;
			new16+=changed;
			compressed16+=changed;
			num16s-=changed;
		}
		compressed16[0]=0;
		compressed16[1]=0;
		compressed16[2]=0;
		compressed=(char*)(compressed16+3);
		//End compression code.
		
		if (compressed-this->data+this->maxcompsize > this->capacity)
		{
			compressed=this->data;
			if (this->tail==this->data+sizeof(size_t)) this->tail=this->data + *(size_t*)this->tail;
		}
		size_t tmp=this->head-this->data;
		memcpy(compressed, &tmp, sizeof(size_t));
		compressed+=sizeof(size_t);
		
		tmp=compressed-this->data;
		memcpy(this->head, &tmp, sizeof(size_t));
		this->head=compressed;
	}
	else
	{
		this->thisblock_valid=true;
	}
	
	char * swap=this->thisblock;
	this->thisblock=this->nextblock;
	this->nextblock=swap;
	
	this->entries++;
}

static void push_cancel(struct rewindstack * this_)
{
	//struct rewindstack_impl * this=(struct rewindstack_impl*)this_;
	//We ignore this. push_begin just returns a pointer anyways.
}

static const void * pull(struct rewindstack * this_)
{
	struct rewindstack_impl * this=(struct rewindstack_impl*)this_;
	
	if (this->thisblock_valid)
	{
		this->thisblock_valid=false;
		this->entries--;
		return this->thisblock;
	}
	
	if (this->head==this->tail) return NULL;
	
	size_t start;
	memcpy(&start, this->head - sizeof(size_t), sizeof(size_t));
	this->head=this->data+start;
	
	const char * compressed=this->data+start+sizeof(size_t);
	char * out=this->thisblock;
	//Begin decompression code
	//out is the last pushed (or returned) state
	const uint16_t * compressed16=(const uint16_t*)compressed;
	uint16_t * out16=(uint16_t*)out;
	while (true)
	{
		uint16_t numchanged=*(compressed16++);
		if (numchanged)
		{
			out16+=*(compressed16++);
			//We could do memcpy, but it seems that memcpy has a constant-per-call overhead that actually shows up.
			//Our average size in here seems to be 8 or something.
			//Therefore, we do something with lower overhead.
			for (int i=0;i<numchanged;i++) out16[i]=compressed16[i];
			compressed16+=numchanged;
			out16+=numchanged;
		}
		else
		{
			uint32_t numunchanged=compressed16[0] | compressed16[1]<<16;
			if (!numunchanged) break;
			compressed16+=2;
			out16+=numunchanged;
		}
	}
	//End decompression code
	
	this->entries--;
	
	return this->thisblock;
}

static void capacity_f(struct rewindstack * this_, unsigned int * entries, size_t * bytes, bool * full)
{
	struct rewindstack_impl * this=(struct rewindstack_impl*)this_;
	
	size_t headpos=(this->head-this->data);
	size_t tailpos=(this->tail-this->data);
	size_t remaining=(tailpos+this->capacity-sizeof(size_t)-headpos-1)%this->capacity + 1;
	
	if (entries) *entries=this->entries;
	if (bytes) *bytes=(this->capacity-remaining);
	if (full) *full=(remaining<=this->maxcompsize*2);
}

static void free_(struct rewindstack * this_)
{
	struct rewindstack_impl * this=(struct rewindstack_impl*)this_;
	free(this->data);
	free(this->thisblock);
	free(this->nextblock);
	free(this);
}

struct rewindstack * rewindstack_create(size_t blocksize, size_t capacity)
{
	struct rewindstack_impl * this=malloc(sizeof(struct rewindstack_impl));
	this->i.reset=reset;
	this->i.push_begin=push_begin;
	this->i.push_end=push_end;
	this->i.push_cancel=push_cancel;
	this->i.pull=pull;
	this->i.capacity=capacity_f;
	this->i.free=free_;
	
	this->data=NULL;
	this->thisblock=NULL;
	this->nextblock=NULL;
	
	this->capacity=0;
	this->blocksize=0;
	
	reset((struct rewindstack*)this, blocksize, capacity);
	
	return (struct rewindstack*)this;
}
