#include "image.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static uint32_t * table_0rgb1555_xrgb8888=NULL;
static uint32_t * table_rgb565_xrgb8888=NULL;

static void create_tbl_0rgb1555_xrgb8888(void* location)
{
	if (table_0rgb1555_xrgb8888)
	{
		memcpy(location, table_0rgb1555_xrgb8888, sizeof(uint32_t)*65536);
		return;
	}
	
	uint32_t * table=(uint32_t*)location;
	
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

static void create_tbl_rgb565_xrgb8888(void* location)
{
	if (table_rgb565_xrgb8888)
	{
		memcpy(location, table_rgb565_xrgb8888, sizeof(uint32_t)*65536);
		return;
	}
	
	uint32_t * table=(uint32_t*)location;
	
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


static void convert_2_4(const struct image * src, struct image * dst)
{
	const uint32_t * conv=(const uint32_t*)image_get_convert_table(src->format, fmt_xrgb8888);
	
	uint16_t * in=(uint16_t*)src->pixels;
	uint32_t * out=(uint32_t*)dst->pixels;
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


static void convert_2_3(const struct image * src, struct image * dst)
{
	const uint32_t * conv=(const uint32_t*)image_get_convert_table(src->format, fmt_xrgb8888);
	
	uint16_t * in=(uint16_t*)src->pixels;
	uint8_t * out=(uint8_t*)dst->pixels;
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
void image_create_convert_table(videoformat srcfmt, videoformat dstfmt, void* dst)
{
	switch (FMT(srcfmt, dstfmt))
	{
		case FMT(fmt_0rgb1555, fmt_xrgb8888): create_tbl_0rgb1555_xrgb8888(dst); break;
		case FMT(fmt_rgb565,   fmt_xrgb8888): create_tbl_rgb565_xrgb8888(dst); break;
	}
}

const void * image_get_convert_table(videoformat srcfmt, videoformat dstfmt)
{
	switch (FMT(srcfmt, dstfmt))
	{
#define IMPL(src, dst, size) \
		case FMT(fmt_##src, fmt_##dst): \
			if (!table_##src##_##dst) \
			{ \
				void * tmp=malloc(size); \
				create_tbl_##src##_##dst(tmp); \
				table_##src##_##dst=(uint32_t*)tmp; \
			} \
			return table_##src##_##dst;
	
	IMPL(0rgb1555, xrgb8888, sizeof(uint32_t)*65536);
	IMPL(rgb565, xrgb8888, sizeof(uint32_t)*65536);
#undef IMPL
	}
	return NULL;
}

void image_convert(const struct image * src, struct image * dst)
{
	switch (FMT(src->format, dst->format))
	{
		case FMT(fmt_0rgb1555, fmt_0rgb1555):
		case FMT(fmt_rgb565, fmt_rgb565):
		case FMT(fmt_rgb888, fmt_rgb888):
		case FMT(fmt_xrgb8888, fmt_xrgb8888):
		case FMT(fmt_argb8888, fmt_argb8888):
		{
			unsigned int pixelsize=videofmt_byte_per_pixel(src->format);
			unsigned int linelen=pixelsize*src->width;
			
			uint8_t * srcdata=(uint8_t*)src->pixels;
			uint8_t * dstdata=(uint8_t*)dst->pixels;
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
		
		case FMT(fmt_0rgb1555, fmt_xrgb8888):
		case FMT(fmt_rgb565, fmt_xrgb8888):
			convert_2_4(src, dst);
			break;
		case FMT(fmt_0rgb1555, fmt_rgb888):
		case FMT(fmt_rgb565, fmt_rgb888):
			convert_2_3(src, dst);
			break;
		default: ; char *e=0; *e=0;
	}
}





void convert_resize_2_2_self(const struct image * src, struct image * dst)
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

void convert_resize_2_4(const struct image * src, struct image * dst)
{
	const uint32_t * conv=(const uint32_t*)image_get_convert_table(src->format, fmt_xrgb8888);
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

void convert_resize_4_4_self(const struct image * src, struct image * dst)
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
	switch (FMT(src->format, dst->format))
	{
		case FMT(fmt_0rgb1555, fmt_0rgb1555):
		case FMT(fmt_rgb565, fmt_rgb565):
			convert_resize_2_2_self(src, dst);
			break;
		
		case FMT(fmt_0rgb1555, fmt_xrgb8888):
		case FMT(fmt_rgb565, fmt_xrgb8888):
			convert_resize_2_4(src, dst);
			break;
		
		case FMT(fmt_xrgb8888, fmt_xrgb8888):
		case FMT(fmt_argb8888, fmt_argb8888):
			convert_resize_4_4_self(src, dst);
			break;
		
		default: ; char *e=0; *e=0;
	}
}
