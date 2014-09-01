#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

#include "image.h"

#define this This
struct buffer {
	char* ptr;
	size_t at;
	size_t len;
};
#define buf_create(buf, size) do { struct buffer * this=(buf); this->len=(size); this->ptr=(char*)malloc(this->len); this->at=0; } while(0)
#define buf_reserve(buf, size) do { \
			struct buffer * this=(struct buffer*)(buf); \
			if (this->at+size > this->len) \
			{ \
				while (this->at+size > this->len) this->len*=2; \
				this->ptr=(char*)realloc(this->ptr, this->len); \
			} \
		} while(0)
#define buf_append_direct(buf, data, size) do { \
			struct buffer * this=(struct buffer*)(buf); \
			memcpy(this->ptr+this->at, (data), (size)); \
			this->at+=(size); \
		} while(0)
#define buf_append(buf, data, size) do { \
			buf_reserve((buf), (size)); \
			buf_append_direct((buf), (data), (size)); \
		} while(0)
#define buf_data(buf) (((struct buffer*)(buf))->ptr)
#define buf_len(buf) (((struct buffer*)(buf))->at)
#define buf_delete(buf) (free(((struct buffer*)(buf))->ptr))

#define buf_append_u8_d(buf, num) do { unsigned char newbuf[1]; newbuf[0]=(num); buf_append_direct((buf), newbuf, 1); } while(0)
#define buf_append_u16_d(buf, num) do { unsigned char newbuf[2]; \
				newbuf[0]=(num)>>8; \
				newbuf[1]=(num)>>0; \
				buf_append_direct((buf), newbuf, 2); } while(0)
#define buf_append_u32_d(buf, num) do { unsigned char newbuf[4]; \
				newbuf[0]=(num)>>24; \
				newbuf[1]=(num)>>16; \
				newbuf[2]=(num)>>8; \
				newbuf[3]=(num)>>0; \
				buf_append_direct((buf), newbuf, 4); } while(0)

#define buf_append_u8(buf, num) do { buf_reserve((buf), 1); buf_append_u8_d((buf), (num));  } while(0)
#define buf_append_u16(buf, num) do { buf_reserve((buf), 2); buf_append_u16_d((buf), (num));  } while(0)
#define buf_append_u32(buf, num) do { buf_reserve((buf), 4); buf_append_u32_d((buf), (num));  } while(0)

#define buf_append_str(buf, str) buf_append((buf), (str), strlen(str))
#define buf_finish_str(buf) buf_append((buf), "", 1)

static mz_bool chunk_append(const void* pBuf, int len, void* pUser)
{
	buf_append(pUser, pBuf, len);
	return true;
}

bool png_encode(const struct image * img, const char * * pngcomments,  void* * pngdata, unsigned int * pnglen)
{
	*pngdata=NULL;
	*pnglen=0;
	if (img->bpp!=15 && img->bpp!=16 && img->bpp!=24 && img->bpp!=32) return false;
	
	unsigned int width=img->width;
	unsigned int height=img->height;
	
	struct buffer buf;
	buf_create(&buf, 8192);
	
	int palette[256];
	int palettelen=0;
	
	static uint32_t * upconv=NULL;
	
	uint8_t * thislineraw=(uint8_t*)img->pixels;
	
	if (img->bpp==15 || img->bpp==16)
	{
		if (!upconv) upconv=(uint32_t*)malloc(sizeof(uint32_t)*65536);
		
		for (unsigned int i=0;i<65536;i++) upconv[i]=0xFFFFFFFF;
		for (unsigned int y=0;y<height;y++)
		{
			for (unsigned int x=0;x<width;x++)
			{
				int col16=((uint16_t*)thislineraw)[x];
				if (upconv[col16]==0xFFFFFFFF)
				{
					if (palettelen>=256) goto nopal_1516bpp;
					upconv[col16]=palettelen;
					if (img->bpp==15)
					{
						int col=(((col16>>10)&31)<<19)|(((col16>>5)&31)<<11)|(((col16>>0)&31)<<3);
						col|=(col>>5)&0x070707;
						palette[palettelen]=col;
					}
					else
					{
						int col=(((col16>>11)&31)<<19)|(((col16>>5)&63)<<10)|(((col16>>0)&31)<<3);
						col|=(col>>5)&0x070007;
						col|=(col>>6)&0x000300;
						palette[palettelen]=col;
					}
					palettelen++;
				}
			}
			thislineraw+=img->pitch;
		}
		
		if (0)
		{
		nopal_1516bpp:;
			palettelen=0;
			image_create_convert_table(img->bpp, 32, upconv);
		}
	}
	else if (img->bpp==24)
	{
		for (unsigned int y=0;y<height;y++)
		{
			for (unsigned int x=0;x<width;x++)
			{
				int col32=thislineraw[x*3+0]<<16|
				          thislineraw[x*3+1]<<8|
				          thislineraw[x*3+2]<<0;
				int i;
				for (i=0;i<palettelen;i++)
				{
					if (col32==palette[i]) break;
				}
				if (i==palettelen)
				{
					if (palettelen>=256) goto nopal_2432bpp;
					palette[i]=col32;
					palettelen++;
				}
			}
			thislineraw+=img->pitch;
		}
	}
	else if (img->bpp==32)
	{
		for (unsigned int y=0;y<height;y++)
		{
			for (unsigned int x=0;x<width;x++)
			{
				int col32=((uint32_t*)thislineraw)[x]&0x00FFFFFF;
				int i;
				for (i=0;i<palettelen;i++)
				{
					if (col32==palette[i]) break;
				}
				if (i==palettelen)
				{
					if (palettelen==256) goto nopal_2432bpp;
					palette[i]=col32;
					palettelen++;
				}
			}
			thislineraw+=img->pitch;
		}
		if (0)
		{
		nopal_2432bpp:;
			palettelen=0;
		}
	}
	
	uint8_t bits_per_channel=8;
	if (palettelen<=256) bits_per_channel=8;
	if (palettelen<=16) bits_per_channel=4;
	if (palettelen<=4) bits_per_channel=2;
	if (palettelen<=2) bits_per_channel=1;
	//it would be fun with a 0bpp screenshot, but it's not allowed. whatever, zeroes compress well
	if (palettelen==0) bits_per_channel=8;
	
	buf_append(&buf, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8);
#define CHUNK(name, data, len) do { \
			buf_reserve(&buf, 4+4+len+4); \
			buf_append_u32_d(&buf, len); \
			buf_append_direct(&buf, name, 4); \
			buf_append_direct(&buf, data, len); \
			unsigned int tmp=mz_crc32(0, NULL, 0); \
			tmp=mz_crc32(tmp, (uint8_t*)name, 4); \
			tmp=mz_crc32(tmp, (uint8_t*)data, len); \
			buf_append_u32_d(&buf, tmp); \
		} while(0)
#define EMPTY_CHUNK(name) do { \
			buf_reserve(&buf, 4+4+0+4); \
			buf_append_u32_d(&buf, 0); \
			buf_append_direct(&buf, name, 4); \
			unsigned int tmp=mz_crc32(0, NULL, 0); \
			tmp=mz_crc32(tmp, (uint8_t*)name, 4); \
			buf_append_u32_d(&buf, tmp); \
		} while(0)
	struct IHDR {
		uint8_t width[4];
		uint8_t height[4];
		uint8_t bitdepth;
		uint8_t colortype;
		uint8_t compression;
		uint8_t filter;
		uint8_t interlace;
		
		uint8_t padding[3];
	} IHDR = {
			{ (uint8_t)(width >>24), (uint8_t)(width >>16), (uint8_t)(width >>8), (uint8_t)(width >>0) },
			{ (uint8_t)(height>>24), (uint8_t)(height>>16), (uint8_t)(height>>8), (uint8_t)(height>>0) },
			bits_per_channel, (uint8_t)(palettelen?3:2), 0, 0, 0
		};
	if (sizeof(IHDR)!=16) { return false; }
	CHUNK("IHDR", &IHDR, 13);
	
	if (pngcomments)
	{
		while (*pngcomments)
		{
			unsigned int len1=strlen(pngcomments[0]);
			unsigned int len2=strlen(pngcomments[1]);
			char * data=(char*)malloc(len1+1+len2);
			strcpy(data, pngcomments[0]);
			memcpy(data+len1+1, pngcomments[1], len2);
			CHUNK("tEXt", data, len1+1+len2);
			free(data);
			pngcomments+=2;
		}
	}
	
	if (palettelen)
	{
		unsigned char palettepack[256*3];
		for (int i=0;i<palettelen;i++)
		{
			palettepack[i*3+0]=(palette[i]>>16)&0xFF;
			palettepack[i*3+1]=(palette[i]>>8 )&0xFF;
			palettepack[i*3+2]=(palette[i]    )&0xFF;
		}
		CHUNK("PLTE", palettepack, palettelen*3);
	}
	
	struct buffer chunkbuf;
	buf_create(&chunkbuf, 8192);
	
	tdefl_compressor d;
	//memset(&d,0,sizeof(d));
	tdefl_init(&d, chunk_append, &chunkbuf, TDEFL_DEFAULT_MAX_PROBES|TDEFL_WRITE_ZLIB_HEADER);
	
	thislineraw=(uint8_t*)img->pixels;
	
	size_t bpp=(palettelen?1:3);
	size_t bpl_unpacked=bpp*width;
	size_t bpl=((bpl_unpacked*bits_per_channel)+7)/8;
	
	uint8_t * prevlineplus=(uint8_t*)malloc(bpp+bpl);
	uint8_t * prevline=prevlineplus+bpp;
	memset(prevlineplus, 0, sizeof(bpp+bpl));
	
	uint8_t * thislineplus=(uint8_t*)malloc(bpp+bpl_unpacked);
	memset(thislineplus, 0, bpp);
	uint8_t * thisline=thislineplus+bpp;
	
	for (unsigned int y=0;y<height;y++)
	{
		if (palettelen)
		{
			switch (img->bpp)
			{
			case 15: case 16:
				for (unsigned int x=0;x<width;x++)
				{
					int col=upconv[((uint16_t*)thislineraw)[x]];
					thisline[x]=col;
				}
				break;
			case 24:
				for (unsigned int x=0;x<width;x++)
				{
					int col32=thislineraw[x*3+0]<<16|
					          thislineraw[x*3+1]<<8|
					          thislineraw[x*3+2]<<0;
					int i=0;
					while (true)
					{
						if (palette[i]==col32)
						{
							thisline[x]=i;
							break;
						}
						i++;
					}
				}
				break;
			case 32:
				for (unsigned int x=0;x<width;x++)
				{
					int col32=((uint32_t*)thislineraw)[x];
					int i=0;
					while (true)
					{
						if (palette[i]==col32)
						{
							thisline[x]=i;
							break;
						}
						i++;
					}
				}
				break;
			}
			switch (bits_per_channel)
			{
				case 1:
				for (unsigned int x=0;x<=width/8;x++)
				{
					int out=0;
					for (int p=0;p<8;p++)
					{
						out|=(thisline[x*8+p]<<(7-p));
					}
					thisline[x]=out;
				}
				break;
				case 2:
				for (unsigned int x=0;x<=width/4;x++)
				{
					int col1=thisline[x*4+0];
					int col2=thisline[x*4+1];
					int col3=thisline[x*4+2];
					int col4=thisline[x*4+3];
					thisline[x]=(col1<<6)|(col2<<4)|(col3<<2)|(col4<<0);
				}
				break;
				case 4:
				for (unsigned int x=0;x<=width/2;x++)
				{
					int col1=thisline[x*2+0];
					int col2=thisline[x*2+1];
					thisline[x]=(col1<<4)|(col2<<0);
				}
				break;
				case 8: break;
			}
		}
		else
		{
			switch (img->bpp)
			{
			case 15: case 16:
				for (unsigned int x=0;x<width;x++)
				{
					int col=upconv[((uint16_t*)thislineraw)[x]];
					thisline[x*3+0]=(col>>16)&0xFF;
					thisline[x*3+1]=(col>>8 )&0xFF;
					thisline[x*3+2]=(col    )&0xFF;
				}
				break;
			case 24:
				memcpy(thisline, thislineraw, bpl);
				break;
			case 32:
				for (unsigned int x=0;x<width;x++)
				{
					int col=((uint32_t*)thislineraw)[x];
					thisline[x*3+0]=col>>16;
					thisline[x*3+1]=col>>8;
					thisline[x*3+2]=col;
				}
				break;
			}
		}
		
		uint8_t * filteredlines[5];
		for (unsigned int i=0;i<5;i++) filteredlines[i]=(uint8_t*)malloc(bpl);
		unsigned int filterid;
		
		if (palettelen==0)
		{
			//0 - None
			memcpy(filteredlines[0], thisline, bpl);
			
			//1 - Sub
			for (unsigned int x=0;x<bpl;x++)
			{
				filteredlines[1][x]=thisline[x]-thisline[x-bpp];
			}
			
			//2 - Up
			for (unsigned int x=0;x<bpl;x++)
			{
				filteredlines[2][x]=thisline[x]-prevline[x];
			}
			
			//3 - Average
			for (unsigned int x=0;x<bpl;x++)
			{
				filteredlines[3][x]=thisline[x]-(thisline[x-bpp]+prevline[x])/2;
			}
			
			//4 - Paeth
			for (unsigned int x=0;x<bpl;x++)
			{
				int prediction;
				
				int a=thisline[x-bpp];
				int b=prevline[x];
				int c=prevline[x-bpp];
				
				int p=a+b-c;
				int pa=abs(p-a);
				int pb=abs(p-b);
				int pc=abs(p-c);
				
				if (pa<=pb && pa<=pc) prediction=a;
				else if (pb<=pc) prediction=b;
				else prediction=c;
				
				filteredlines[4][x]=thisline[x]-prediction;
			}
			
			//Select the one with fewest distinct values (counting the filter); break ties with lowest absolute differences of predictions
			//This was tested on a large number of images and shown to be the best.
			int numdistinct[5];
			memset(numdistinct, 0, sizeof(numdistinct));
			bool distinct[5][256];
			memset(distinct, 0, sizeof(distinct));
			
			long long int differences[5];
			memset(differences, 0, sizeof(differences));
			
			filterid=0;
			for (unsigned int f=0;f<5;f++)
			{
				distinct[f][f]=true;
				for (unsigned int x=0;x<bpl;x++)
				{
					if (!distinct[f][filteredlines[f][x]]) numdistinct[f]++;
					distinct[f][filteredlines[f][x]]=true;
					differences[f]+=abs((int)((unsigned char)(filteredlines[f][x]+thisline[x]))-(int)thisline[x]);
					//shenanigans! actually, I'm not even sure if they work.
				}
				if (numdistinct[f]<numdistinct[filterid]) filterid=f;
				if (numdistinct[f]==numdistinct[filterid] && differences[f]<differences[filterid]) filterid=f;
				//if (differences[f]<differences[filterid]) filterid=f;
			}
		}
		else
		{
			//paletted images shouldn't be filtered
			filterid=0;
			memcpy(filteredlines[0], thisline, bpl);
		}
		
		tdefl_compress_buffer(&d, &filterid, 1, TDEFL_NO_FLUSH);
		tdefl_compress_buffer(&d, (char*)filteredlines[filterid], bpl, TDEFL_NO_FLUSH);
		
		for (unsigned int i=0;i<5;i++) free(filteredlines[i]);
		
		memcpy(prevline, thisline, bpl);
		thislineraw+=img->pitch;
	}
//puts("");
	free(prevlineplus);
	free(thislineplus);
	tdefl_compress_buffer(&d, NULL, 0, TDEFL_FINISH);
	
	CHUNK("IDAT", buf_data(&chunkbuf), buf_len(&chunkbuf));
	buf_delete(&chunkbuf);
	
	EMPTY_CHUNK("IEND");
	
	*pngdata=buf_data(&buf);
	*pnglen=buf_len(&buf);
	
	return true;
}
