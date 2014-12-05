#if 0
<<COMMENT1
//retrorepeat - a headless libretro frontend, for regression testing
//usage: retroprofile corepath rompath frames tracedata [savestate]
//example: ./retrorepeat roms/gambatte_libretro.so roms/zeldaseasons.gbc 10000 /tmp/retrotrace roms/zeldaseasons.gst
//the given core and rom will be loaded and executed for the given number of frames
//the first time, the output will be stored to the trace file
//the second time, the output will be compared to the trace file, and the tool will report if they're identical
//you can not create two traces and compare them; input is randomized
//the savestate will be loaded if present; if not, the game is started from scratch
COMMENT1

cd tests || true
cd ..
g++ -I. -Wno-unused-result tests/retrorepeat.cpp \
	libretro.cpp -DDYLIB_POSIX dylib.cpp -DWINDOW_MINIMAL window-none.cpp \
	memory.cpp video.cpp miniz.c -ldl -lrt -Os -s -o retrorepeat
mv retrorepeat ~/bin

<<COMMENT2
#endif

#include "minir.h"
#include "io.h"
#include "window.h"
#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct mem {
	char* data;
	size_t pos;
	size_t len;
};

void mem_append(struct mem * buf, const void * data, size_t len)
{
	if (buf->pos + len > buf->len)
	{
		if (!buf->len) buf->len=256;
		while (buf->pos + len > buf->len) buf->len*=2;
		buf->data=realloc(buf->data, buf->len);
	}
	memcpy(buf->data + buf->pos, data, len);
	buf->pos += len;
}

mz_bool mem_append_mz(const void* pBuf, int len, void* pUser)
{
	mem_append((struct mem*)pUser, pBuf, len);
	return MZ_TRUE;
}


struct zmem {
	union {
		struct {
			tinfl_decompressor decomp;
			size_t decomppos;
			uint8_t decompbuf[TINFL_LZ_DICT_SIZE];
		};
		tdefl_compressor comp;
	};
	struct mem child;
};

void zmem_init_read(struct zmem * buf, void * data, size_t len)
{
	tinfl_init(&buf->decomp);
	buf->child.pos=0;
	buf->child.len=len;
	buf->child.data=(char*)data;
}

void zmem_init_write(struct zmem * buf)
{
	tdefl_init(&buf->comp, mem_append_mz, &buf->child, TDEFL_DEFAULT_MAX_PROBES);
	buf->child.pos=0;
	buf->child.len=0;
	buf->child.data=NULL;
}

void zmem_read(struct zmem * buf, void * data, size_t len)
{
memcpy(data, (char*)buf->child.data+buf->child.pos, len);
buf->child.pos+=len;
return;
	
	char * datatmp=(char*)data;
	while (len)
	{
		size_t outsize=sizeof(buf->decompbuf)-buf->decomppos;
		if (len > outsize) outsize=len;
		tinfl_status status=tinfl_decompress(&buf->decomp, (mz_uint8*)buf->child.data + buf->child.len - buf->child.pos, &buf->child.pos,
		                                     buf->decompbuf, buf->decompbuf+buf->decomppos, &outsize, 0);
		if (status!=TINFL_STATUS_HAS_MORE_OUTPUT && status!=TINFL_STATUS_DONE) abort();
		memcpy(datatmp, buf->child.data + buf->child.len - buf->child.pos, outsize);
		datatmp+=outsize;
		len-=outsize;
		buf->decomppos+=len;
		if (buf->decomppos==sizeof(buf->decompbuf)) buf->decomppos=0;
	}
}

void zmem_write(struct zmem * buf, const void * data, size_t len)
{
mem_append(&buf->child, data, len);
return;
	tdefl_compress_buffer(&buf->comp, data, len, TDEFL_NO_FLUSH);
}

void* zmem_finish_write(struct zmem * buf, size_t* len)
{
	tdefl_compress_buffer(&buf->comp, NULL, 0, TDEFL_FINISH);
	*len=buf->child.pos;
	return buf->child.data;
}


enum type {
	ty_video,
	ty_audio,
	ty_input,
	ty_savestate,
	
	ty_final
};

struct zmem tracers[ty_final];

bool trace_writing;
void trace_data(type ty, const void * data, size_t size)
{
	if (trace_writing)
	{
		zmem_write(&tracers[ty], data, size);
	}
	else
	{
		char compare[size];
		zmem_read(&tracers[ty], compare, size);
		if (memcmp(compare, data, size))
		{
			printf("error in type %i\n", ty);
			exit(1);
		}
	}
}

void trace_get_data(type ty, void* data, size_t size)
{
	if (trace_writing)
	{
		zmem_write(&tracers[ty], data, size);
	}
	else
	{
		zmem_read(&tracers[ty], data, size);
	}
}

class video_trace : public video {
public:
	uint8_t bpp;
	uint32_t features() { return 0; }
	void set_source(unsigned int max_width, unsigned int max_height, videoformat depth) { bpp=(depth==fmt_xrgb8888 ? 4 : 2); }
	void draw_2d(unsigned int width, unsigned int height, const void * data, unsigned int pitch)
	{
		for (unsigned int i=0;i<height;i++) trace_data(ty_video, (char*)data + pitch*i, width*bpp);
	}
	void set_dest_size(unsigned int width, unsigned int height) {}
	~video_trace() {}
};

void trace_audio(struct audio * This, unsigned int numframes, const int16_t * samples)
{
	trace_data(ty_audio, samples, sizeof(int16_t)*2*numframes);
}

int16_t trace_input(struct libretroinput * This, unsigned port, unsigned device, unsigned index, unsigned id)
{
	int16_t ret=rand();
	trace_get_data(ty_input, &ret, sizeof(ret));
	return ret;
}

int main(int argc, char * argv[])
{
	//window_init(&argc, &argv);
	srand(window_get_time());
	FILE * f=fopen(argv[4], "rb");
	if (f)
	{
		trace_writing=false;
		for (unsigned int i=0;i<ty_final;i++)
		{
			size_t len;
			fread(&len, 1,sizeof(size_t), f);
			void* data=malloc(len);
			fread(data, 1,len, f);
			zmem_init_read(&tracers[i], data, len);
		}
	}
	else
	{
		f=fopen(argv[4], "wb");
		trace_writing=true;
		for (unsigned int i=0;i<ty_final;i++)
		{
			zmem_init_write(&tracers[i]);
			tracers[i].child.pos=0;
			tracers[i].child.len=0;
			tracers[i].child.data=NULL;
		}
	}
	
	unsigned int frames=atoi(argv[3]);
	
	struct libretro * core=libretro_create(argv[1], NULL, NULL);
	if (!core)
	{
		puts("Couldn't load core.");
		return 1;
	}
	
	video* v=new video_trace();
	
	static struct audio traudio;
	traudio.render=trace_audio;
	
	static struct libretroinput trinput;
	trinput.query=trace_input;
	
	core->attach_interfaces(core, v, &traudio, &trinput);
	
	if (!core->load_rom(core, NULL,0, argv[2]))
	{
		puts("Couldn't load ROM.");
		return 1;
	}
	
	videoformat fmt;
	core->get_video_settings(core, NULL, NULL, &fmt, NULL);
	v->set_source(0, 0, fmt);
	
	size_t statesize=core->state_size(core);
	void* state=malloc(statesize);
	
	if (argv[5])
	{
		FILE * f=fopen(argv[5], "rb");
		fread(state, 1,statesize, f);
		core->state_load(core, state, statesize);
		fclose(f);
	}
	
	unsigned int fps=0;
	uint64_t print=window_get_time();
	for (unsigned int i=0;i<frames;i++)
	{
		core->run(core);
		uint64_t now=window_get_time();
		if (now > print+200000) print=now,printf("%i/%i\r", i, frames),fflush(stdout);
		
		if (i%83==29)//savestate at random intervals and ensure nothing changed
		{
			core->state_save(core, state, statesize);
			trace_data(ty_savestate, state, statesize);
		}
	}
	
	core->free(core);
	
	if (trace_writing)
	{
		for (unsigned int i=0;i<ty_final;i++)
		{
			size_t len;
			void* data=zmem_finish_write(&tracers[i], &len);
			fwrite(&len, 1,sizeof(size_t), f);
			fwrite(data, 1,len, f);
		}
		puts("Trace recorded");
	}
	else
	{
		puts("All data matches");
	}
}
#if 0
COMMENT2
#endif
