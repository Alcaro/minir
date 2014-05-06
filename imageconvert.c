#include "minir.h"
#include <stdlib.h>
#include <string.h>

static uint32_t * table_15_32=NULL;
static uint32_t * table_16_32=NULL;

static void create_tbl_15_32(void* location)
{
	if (table_15_32)
	{
		memcpy(location, table_15_32, sizeof(uint32_t)*65536);
		return;
	}
	
	uint32_t * table=location;
	
	for (int r=0;r<32;r++)
	for (int g=0;g<32;g++)
	for (int b=0;b<32;b++)
	{
		int index=r*32*32+g*32+b;
		int col=(r<<19)|(g<<11)|(b<<3);
		col|=(col>>5)&0x070707;
		table[index]=col;
	}
	memset(table+32768, 0, sizeof(uint32_t)*32768);
}

static void create_tbl_16_32(void* location)
{
	if (table_16_32)
	{
		memcpy(location, table_16_32, sizeof(uint32_t)*65536);
		return;
	}
	
	uint32_t * table=location;
	
	for (int r=0;r<32;r++)
	for (int g=0;g<64;g++)
	for (int b=0;b<32;b++)
	{
		int index=r*64*32+g*32+b;
		int col=(r<<19)|(g<<10)|(b<<3);
		col|=(col>>5)&0x070007;
		col|=(col>>6)&0x000300;
		table[index]=col;
	}
}


static void convert_1516_32(const struct image * src, struct image * dst)
{
	const uint32_t * conv=image_get_convert_table(src->bpp, 32);
	
	uint16_t * in=src->pixels;
	uint32_t * out=dst->pixels;
	for (unsigned int y=0;y<src->height;y++)
	{
		for (unsigned int x=0;x<src->width;x++)
		{
			out[x]=conv[in[x]];
		}
		
		in+=src->pitch/sizeof(uint16_t);
		out+=dst->pitch/sizeof(uint32_t);
	}
}


static void convert_1516_24(const struct image * src, struct image * dst)
{
	const uint32_t * conv=image_get_convert_table(src->bpp, 32);
	
	uint16_t * in=src->pixels;
	uint8_t * out=dst->pixels;
	for (unsigned int y=0;y<src->height;y++)
	{
		for (unsigned int x=0;x<src->width;x++)
		{
			out[x*3+0]=conv[in[x]]>>0;
			out[x*3+1]=conv[in[x]]>>8;
			out[x*3+2]=conv[in[x]]>>16;
		}
		
		in+=src->pitch/sizeof(uint16_t);
		out+=dst->pitch/sizeof(uint8_t);
	}
}


#define FMT(src, dst) ((src)<<8 | (dst))
void image_create_convert_table(unsigned int srcbpp, unsigned int dstbpp, void* dst)
{
	switch (FMT(srcbpp, dstbpp))
	{
		case FMT(15, 32): create_tbl_15_32(dst); break;
		case FMT(16, 32): create_tbl_16_32(dst); break;
	}
}

const void * image_get_convert_table(unsigned int srcbpp, unsigned int dstbpp)
{
	switch (FMT(srcbpp, dstbpp))
	{
#define IMPL(src, dst, size) \
		case FMT(src, dst): \
			if (!table_##src##_##dst) \
			{ \
				void * tmp=malloc(size); \
				create_tbl_##src##_##dst(tmp); \
				table_##src##_##dst=tmp; \
			} \
			return table_##src##_##dst;
	
	IMPL(15, 32, sizeof(uint32_t)*65536);
	IMPL(16, 32, sizeof(uint32_t)*65536);
#undef IMPL
	}
	return NULL;
}

void image_convert(const struct image * src, struct image * dst)
{
	switch (FMT(src->bpp, dst->bpp))
	{
		case FMT(15, 15):
		case FMT(16, 16):
		case FMT(24, 24):
		case FMT(32, 32):
		case FMT(33, 33):
		{
			unsigned int pixelsize=((src->bpp)<=16?2: (src->bpp)==24?3: 4);
			unsigned int linelen=pixelsize*src->width;
			
			char * srcdata=src->pixels;
			char * dstdata=dst->pixels;
			unsigned int srcpitch=src->pitch;
			unsigned int dstpitch=dst->pitch;
			
			for (unsigned int y=src->height;y>0;y--)//loop from height to 1; we don't use y in the loop
			{
				memcpy(dstdata, srcdata, linelen);
				srcdata+=srcpitch;
				dstdata+=dstpitch;
			}
			break;
		}
		
		case FMT(15, 32):
		case FMT(16, 32):
			convert_1516_32(src, dst);
			break;
		case FMT(15, 24):
		case FMT(16, 24):
			convert_1516_24(src, dst);
			break;
		default: ; char *e=0; *e=0;
	}
}





void convert_resize_1516_self(const struct image * src, struct image * dst)
{
	float xstep=(float)src->width/dst->width;
	float ystep=(float)src->height/dst->height;
	for (unsigned int y=0;y<dst->height;y++)
	{
		const uint16_t* srcdat=((uint16_t*)src->pixels)+((unsigned int)(ystep*y))*src->pitch/sizeof(uint16_t);
		uint16_t* dstdat=((uint16_t*)dst->pixels)+(y*dst->pitch/sizeof(uint16_t));
		
		float xpos=(float)0.5/dst->width;
		for (unsigned int x=0;x<dst->width;x++)
		{
			*(dstdat++)=srcdat[(unsigned int)xpos];
			xpos+=xstep;
		}
	}
}

void convert_resize_1516_32(const struct image * src, struct image * dst)
{
	const uint32_t * conv=image_get_convert_table(src->bpp, 32);
	float xstep=(float)src->width/dst->width;
	float ystep=(float)src->height/dst->height;
	for (unsigned int y=0;y<dst->height;y++)
	{
		const uint16_t* srcdat=((uint16_t*)src->pixels)+((unsigned int)(ystep*y))*src->pitch/sizeof(uint16_t);
		uint32_t* dstdat=((uint32_t*)dst->pixels)+(y*dst->pitch/sizeof(uint32_t));
		
		float xpos=(float)0.5/dst->width;
		for (unsigned int x=0;x<dst->width;x++)
		{
			*(dstdat++)=conv[srcdat[(unsigned int)xpos]];
			xpos+=xstep;
		}
	}
}

void convert_resize_3233_self(const struct image * src, struct image * dst)
{
	float xstep=(float)src->width/dst->width;
	float ystep=(float)src->height/dst->height;
	for (unsigned int y=0;y<dst->height;y++)
	{
		const uint32_t* srcdat=((uint32_t*)src->pixels)+((unsigned int)(ystep*y))*src->pitch/sizeof(uint32_t);
		uint32_t* dstdat=((uint32_t*)dst->pixels)+(y*dst->pitch/sizeof(uint32_t));
		
		float xpos=(float)0.5/dst->width;
		for (unsigned int x=0;x<dst->width;x++)
		{
			*(dstdat++)=srcdat[(unsigned int)xpos];
			xpos+=xstep;
		}
	}
}

void image_convert_resize(const struct image * src, struct image * dst)
{
	switch (FMT(src->bpp, dst->bpp))
	{
		case FMT(15, 15):
		case FMT(16, 16):
		{
			convert_resize_1516_self(src, dst);
			break;
		}
		
		case FMT(32, 32):
		case FMT(33, 33):
		{
			convert_resize_3233_self(src, dst);
			break;
		}
		
		case FMT(15, 32):
		case FMT(16, 32):
			convert_resize_1516_32(src, dst);
			break;
		default: ; char *e=0; *e=0;
	}
}
