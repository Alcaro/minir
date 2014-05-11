#include "minir.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

struct cheat {
	unsigned char memblk;
	
	unsigned int changemode :2;
	unsigned int size :3;//only values 1..4 are allowed, but it's easier to give an extra bit than adding 1 on every use.
	bool is_signed :1;
	bool enabled :1;
	bool restore :1;
	//char padding2[2];
	
	unsigned int orgvalue;//value to restore to if disabled
	unsigned int prevvalue;//value of last frame, for inconly/deconly
};

struct memblock {
	unsigned int len;
	unsigned char * ptr;
	
	char* name;
	unsigned int addrlen;
	uint32_t start;
	uint32_t mask;
	
	unsigned char * prev;
	
	//don't make a define for the size of this one, we'd need a typedef too
	uint32_t * show;//the top row in each of those has value 1
#define SIZE_PAGE_LOW 0x2000//8192
	uint16_t * show_treelow;//one entry per SIZE_PAGE_LOW bits of mem, counts number of set bits within these bytes
#define SIZE_PAGE_HIGH 0x400000//1048576
	uint32_t * show_treehigh;//same format as above
	//both page sizes must be powers of 2
	//LOW must be in the range [32 .. 32768]
	//HIGH must be larger than LOW
	uint32_t show_tot;//one uint32 per block of mem
	
	bool last3 :1;//these three contain the last three show flags, if some are removed due to not having that many bytes available
	bool last2 :1;
	bool last1 :1;
	
	bool bigendian;
	bool showinsearch;
};

struct minircheats_model_impl {
	struct minircheats_model i;
	
	struct memblock * mem;
	unsigned int nummem;
	
	unsigned int numcheats;
	struct cheat * cheatlist;
	
	unsigned int lastrow;
	unsigned int lastblock;
	unsigned int lastmempos;
	
	unsigned int datsize :3;
	bool issigned :1;
	//char padding :4;
	//char padding[3];
};

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

//static void writemem(unsigned char * ptr, enum cheat_size nbytes, bool bigendian, uint32_t value)
//{
//	if (nbytes==1)
//	{
//		*ptr=value;
//		return;
//	}
//	__builtin_trap();
//}

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

static void delete_all_mem(struct minircheats_model_impl * this);

static void search_show_all(struct memblock * mem, unsigned int datsize);

static void getpos(struct minircheats_model_impl * this, unsigned int visrow, unsigned int * memblk, unsigned int * mempos)
{
	if (visrow==this->lastrow)
	{
		*memblk=this->lastblock;
		*mempos=this->lastmempos;
		return;
	}
	this->lastrow=visrow;
	
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
	
	*mempos = (bits-mem->show)*32 + lastbitcount;
	
	this->lastblock=*memblk;
	this->lastmempos=*mempos;
}

static bool search_get_bit(struct memblock * mem, unsigned int row)
{
	return (mem->show[row/32]&(1<<(row&31)));
}

//returns the previous value
static bool search_set_bit(struct memblock * mem, unsigned int row, bool value)
{
	bool prev=search_get_bit(mem, row);
	if (prev != value)
	{
		int add=(value ? 1 : -1);
		if (value) mem->show[row/32] |=  (1<<(row&31));
		else       mem->show[row/32] &=~ (1<<(row&31));
		mem->show_treelow[row/SIZE_PAGE_LOW]+=add;
		mem->show_treehigh[row/SIZE_PAGE_HIGH]+=add;
		mem->show_tot+=add;
	}
	return prev;
}

static void search_show_all(struct memblock * mem, unsigned int datsize)
{
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
	
	if (datsize>=4) mem->last3=search_set_bit(mem, mem->len-3, false);//these three are known to return true because I just set them to 0xFFFFFFFF
	if (datsize>=3) mem->last2=search_set_bit(mem, mem->len-2, false);
	if (datsize>=2) mem->last1=search_set_bit(mem, mem->len-1, false);
}

void set_core(struct minircheats_model * this_, struct libretro * core)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	delete_all_mem(this);
	
	const char * memdesc=core->get_memory_info(core);
	if (!memdesc) memdesc="2::0:0";
	unsigned int nummem=1;
	for (unsigned int i=0;memdesc[i];i++)
	{
		if (memdesc[i]==';') nummem++;
	}
	
	this->nummem=nummem;
	this->mem=malloc(sizeof(struct memblock)*nummem);
	
	for (unsigned int i=0;i<this->nummem;i++)
	{
		struct memblock * mem=&this->mem[i];
		
		mem->prev=NULL;
		mem->show=NULL;
		mem->show_treelow=NULL;
		mem->show_treehigh=NULL;
		mem->show_tot=0;
		
		mem->name=NULL;
		mem->bigendian=false;
		mem->showinsearch=true;
		
		while (isupper(*memdesc))
		{
			memdesc++;
		}
		if (!isdigit(*memdesc)) goto bad;
		
		unsigned int memblk=strtoul(memdesc, (char**)&memdesc, 10);
		core->get_memory(core, memblk, &mem->len, (void**)&mem->ptr);
		if (!mem->ptr) goto bad;
		if (*memdesc!=':') goto bad;
		memdesc++;
		
		unsigned int namelen;
		for (namelen=0;isupper(memdesc[i]);namelen++) ;
		mem->name=malloc(namelen+1);
		memcpy(mem->name, memdesc, namelen);
		mem->name[namelen]='\0';
		if (*memdesc!=':') goto bad;
		memdesc++;
		
		unsigned int addrlen;
		for (addrlen=0;isxdigit(memdesc[addrlen]);addrlen++) ;
		
		//this is mainly for the case when the core doesn't tell us its memory layout
		while (addrlen<8 && 1<<(addrlen*4) < mem->len) addrlen++;
		
		if (addrlen>8) goto bad;
		mem->addrlen=addrlen;
		mem->start=strtoul(memdesc, (char**)&memdesc, 16);
		if (*memdesc!=':') goto bad;
		memdesc++;
		
		for (addrlen=0;isxdigit(memdesc[addrlen]);addrlen++) ;
		if (addrlen>8) goto bad;
		mem->mask=strtoul(memdesc, (char**)&memdesc, 16);
		if (*memdesc && *memdesc!=';') goto bad;
		memdesc++;
		
		continue;
	bad:
		free(mem->name);
		i--;
		this->nummem--;
		memdesc=strchr(memdesc, ';')+1;
		continue;
	}
	
	//free(this->cheatlist);
	//this->numcheats=0;
	
	for (unsigned int i=0;i<this->nummem;i++)
	{
		struct memblock * mem=&this->mem[i];
		if (!mem->show)
		{
			unsigned int len=mem->len;
			mem->prev=malloc(len);
			memcpy(mem->prev, mem->ptr, len);
			
			mem->show=malloc(div_rndup(mem->len, 32)*sizeof(uint32_t));
			mem->show_treelow=malloc(div_rndup(mem->len, SIZE_PAGE_LOW)*sizeof(uint16_t));
			mem->show_treehigh=malloc(div_rndup(mem->len, SIZE_PAGE_HIGH)*sizeof(uint32_t));
			
			search_show_all(mem, this->datsize);
		}
	}
	
	this->lastrow=-1;
}

static void search_reset(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	for (unsigned int i=0;i<this->nummem;i++)
	{
		search_show_all(&this->mem[i], this->datsize);
	}
}

static void search_set_datsize(struct minircheats_model * this_, unsigned int datsize)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	for (unsigned int i=0;i<this->nummem;i++)
	{
		struct memblock * mem=&this->mem[i];
		if (this->datsize>=4) search_set_bit(mem, mem->len-3, mem->last3);
		if (this->datsize>=3) search_set_bit(mem, mem->len-2, mem->last2);
		if (this->datsize>=2) search_set_bit(mem, mem->len-1, mem->last1);
	}
	this->datsize=datsize;
	for (unsigned int i=0;i<this->nummem;i++)
	{
		struct memblock * mem=&this->mem[i];
		if (this->datsize>=4) mem->last3=search_set_bit(mem, mem->len-3, false);
		if (this->datsize>=3) mem->last2=search_set_bit(mem, mem->len-2, false);
		if (this->datsize>=2) mem->last1=search_set_bit(mem, mem->len-1, false);
	}
}

static void search_set_signed(struct minircheats_model * this_, bool issigned)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	this->issigned=issigned;
}

static void search_do_search(struct minircheats_model * this_,
                             enum cheat_compfunc compfunc, bool comptoprev, unsigned int compto)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	uint32_t signadd=0;
	if (this->issigned)
	{
		signadd=signs[this->datsize-1];
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
					uint32_t val=readmem(mem->ptr+pos+bit, this->datsize, mem->bigendian);
					
					uint32_t other;
					if (comptoprev) other=readmem(mem->prev+pos+bit, this->datsize, mem->bigendian);
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
		memcpy(mem->prev, mem->ptr, mem->len);
	}
}

static unsigned int search_get_num_rows(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	unsigned int numrows=0;
	for (unsigned int i=0;i<this->nummem;i++) numrows+=this->mem[i].show_tot;
	return numrows;
}

static void search_get_vis_row(struct minircheats_model * this_, unsigned int row,
                               char * addr, uint32_t * val, uint32_t * prevval)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	
	unsigned int memblk;
	uint32_t mempos;
	getpos(this, row, &memblk, &mempos);
	struct memblock * mem=&this->mem[memblk];
	
	if (addr) sprintf(addr, "%s%.*X", mem->name, mem->addrlen, mem->start+mempos);
	if (val)     *val  =  readmemext(mem->ptr +mempos, this->datsize, mem->bigendian, this->issigned);
	if (prevval) *prevval=readmemext(mem->prev+mempos, this->datsize, mem->bigendian, this->issigned);
}

static void delete_all_mem(struct minircheats_model_impl * this)
{
	for (unsigned int i=0;i<this->nummem;i++)
	{
		free(this->mem[i].name);
		free(this->mem[i].prev);
		free(this->mem[i].show);
		free(this->mem[i].show_treelow);
		free(this->mem[i].show_treehigh);
	}
	free(this->mem);
	this->mem=NULL;
	this->nummem=0;
}

static void free_(struct minircheats_model * this_)
{
	struct minircheats_model_impl * this=(struct minircheats_model_impl*)this_;
	delete_all_mem(this);
	free(this);
}

const struct minircheats_model_impl minircheats_model_base = {{
	set_core,
	search_reset, search_set_datsize, search_set_signed, search_do_search, search_get_num_rows, search_get_vis_row,
	NULL, NULL, NULL,//cheat_read, cheat_parse, cheat_build
	NULL, NULL, NULL,//cheat_find_for_addr, cheat_add, cheat_replace
	NULL, NULL, NULL,//cheat_replace, cheat_set_enabled, cheat_remove, cheat_get
	NULL,//cheat_apply
	free_
}};
struct minircheats_model * minircheats_create_model()
{
	struct minircheats_model_impl * this=malloc(sizeof(struct minircheats_model_impl));
	memcpy(this, &minircheats_model_base, sizeof(struct minircheats_model_impl));
	return (struct minircheats_model*)this;
}
