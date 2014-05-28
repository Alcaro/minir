#if 0
rm ../roms/testcore_libretro.so
gcc -std=c99 -I.. testcore.c -Os -s -shared -fPIC -fvisibility=hidden -o ../roms/testcore_libretro.so
exit

windows:
del ..\roms\testcore_libretro.dll & gcc -std=c99 -I.. testcore.c -Os -s -shared -o ../roms/testcore_libretro.dll
#endif

// 1. Video output
// 1a. Tearing test, horizontal: Vertical lines moving horizontally.
// 1b. Tearing test, vertical: Horizontal lines moving vertically.
// 1c. vsync test: Flickers between white and black each frame.
// 1d. Stretching test: A checkerboard of black and white, to test if each square looks smooth.
// 1e. Border test: A white screen, with red and yellow borders.
// 2. Latency and synchronization
// 2a. A/V sync test. Will switch between white and silent, and black and noisy, every two seconds.
// 2b. Latency test. If any of ABXYLRStSe are held, it's black and noisy; if not, it's white and silent.
// 3. Netplay
// 3a. Input sync. All button presses are sent to the screen; a hash of that is used as background color, for easy comparison.
// Up and Down will cycle between the test groups. Left and Right will cycle between the tests in each group.
int numgroups=3;
int groupsizes[]={5,2,1};
#define init_grp 1
#define init_sub 'c'

//Also tests the following Libretro env callbacks:
//RETRO_ENVIRONMENT_SET_PIXEL_FORMAT 10
//RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME 18

//#define PIXFMT 0//0RGB1555
//#define PIXFMT 1//XRGB8888
#define PIXFMT 2//RGB565
//For XRGB8888, 1a will set the Xs to 1, while 1b will set them to 0.
//Note that test 3a will give different colors for each of the pixel formats. Therefore, for any comparison to be meaningful, the pixel format must be the same.

#if PIXFMT==0
#define pixel_t uint16_t
#define p_red 0x001F
#define p_grn 0x03E0
#define p_blu 0x7C00
#define p_dark 0x3DEF
#endif
#if PIXFMT==1
#define pixel_t uint32_t
#define p_red 0x0000FF
#define p_grn 0x00FF00
#define p_blu 0xFF0000
#define p_dark 0x7F7F7F
#endif
#if PIXFMT==2
#define pixel_t uint16_t
#define p_red 0x001F
#define p_grn 0x07E0
#define p_blu 0xF800
#define p_dark 0x7BEF
#endif
#define p_blk 0
#define p_yel (p_red|p_grn)
#define p_pur (p_red|p_blu)
#define p_tel (p_grn|p_blu)
#define p_wht (p_red|p_grn|p_blu)

#include "libretro.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#define PI 3.14159265358979323846

retro_environment_t environ_cb = NULL;
retro_video_refresh_t video_cb = NULL;
retro_audio_sample_t audio_cb = NULL;
retro_audio_sample_batch_t audio_batch_cb = NULL;
retro_input_poll_t poller_cb = NULL;
retro_input_state_t input_state_cb = NULL;



struct {
	int testgroup;
	int testsub;
	
	int frame;
	
	uint16_t test3a[28][3];
} state;

uint16_t inpstate[2];
bool sound_enable;
pixel_t pixels[240][320];

void renderchr(int chr, int x, int y);
void renderstr(const char * str, int x, int y);
unsigned long crc32_calc (unsigned char *ptr, unsigned cnt, unsigned long crc);



void test1a()
{
	for (int x=0;x<320;x++)
	{
		if ((x+state.frame)%40 > 20) pixels[0][x]=p_blk;
		else pixels[0][x]=p_wht;
	}
	for (int y=1;y<240;y++) memcpy(pixels[y], pixels[0], sizeof(*pixels));
}

void test1b()
{
	for (int p=0;p<320*240;p++)
	{
		if ((y+state.frame)%30 > 15) pixels[0][p]=p_blk;
		else pixels[0][p]=p_wht;
	}
}

void test1c()
{
	if (state.frame&1) memset(pixels, 0xFF, sizeof(pixels));
	else memset(pixels, 0x00, sizeof(pixels));
}

void test1d()
{
	for (int y=0;y<240;y++)
	for (int x=0;x<320;x++)
	{
		if ((y^x) & 1) pixels[y][x]=p_wht;
		else pixels[y][x]=p_blk;
	}
}

void test1e()
{
	memset(pixels, 0xFF, sizeof(pixels));
	for (int x=0;x<320;x++)
	{
		pixels[  0][x]=((x&1)?p_red:p_yel);
		pixels[239][x]=((x&1)?p_yel:p_red);
	}
	for (int y=0;y<240;y++)
	{
		pixels[y][  0]=((y&1)?p_red:p_yel);
		pixels[y][319]=((y&1)?p_yel:p_red);
	}
}


void test2a()
{
	if (state.frame%240 >= 120)
	{
		memset(pixels, 0x00, sizeof(pixels));
		memset(pixels[(state.frame%120)*2], 0xFF, sizeof(*pixels)*2);
		sound_enable=true;
	}
	else
	{
		memset(pixels, 0xFF, sizeof(pixels));
		memset(pixels[(state.frame%120)*2], 0x00, sizeof(*pixels)*2);
		sound_enable=false;
	}
}

void test2b()
{
	if (inpstate[0]&0x0F0F)
	{
		memset(pixels, 0x00, sizeof(pixels));
		sound_enable=true;
	}
	else
	{
		memset(pixels, 0xFF, sizeof(pixels));
		sound_enable=false;
	}
}


void test3a()
{
	if (inpstate[0]!=state.test3a[27][1] || inpstate[1]!=state.test3a[27][2])
	{
		for (int i=0;i<27*3;i++)
		{
			state.test3a[0][i]=state.test3a[0][i+3];
		}
		state.test3a[27][0]=state.frame;
		state.test3a[27][1]=inpstate[0];
		state.test3a[27][2]=inpstate[1];
	}
	
	uint16_t color=(~crc32_calc((void*)state.test3a, 6*28, ~0U))&p_dark;
	for (int i=0;i<320*240;i++) pixels[0][i]=color;
	
	for (int i=0;i<28;i++)
	{
		if (state.test3a[i][0])
		{
			char line[17];
			sprintf(line, "%i: %.4X %.4X", state.test3a[i][0], state.test3a[i][1], state.test3a[i][2]);
			renderstr(line, 8, 8+i*8);
		}
	}
}



#ifndef EXTERNC
 #ifdef __cplusplus
  #define EXTERNC extern "C"
 #else
  #define EXTERNC
 #endif
#endif

#ifndef EXPORT
 #if defined(CPPCLI)
  #define EXPORT EXTERNC
 #elif defined(_WIN32)
  #define EXPORT EXTERNC __declspec(dllexport)
 #else
  #define EXPORT EXTERNC __attribute__((visibility("default")))
 #endif
#endif

EXPORT void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
EXPORT void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
EXPORT void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
EXPORT void retro_set_input_poll(retro_input_poll_t cb) { poller_cb = cb; }
EXPORT void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

EXPORT void retro_set_environment(retro_environment_t cb)
{
	environ_cb=cb;
	
	bool True=true;
	environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &True);
}

EXPORT void retro_init(void) {}
EXPORT void retro_deinit(void) {}
EXPORT unsigned retro_api_version(void) { return RETRO_API_VERSION; }

EXPORT void retro_get_system_info(struct retro_system_info *info)
{
	const struct retro_system_info myinfo={ "Test core", "v0.01", "c|h", false, false };
	memcpy(info, &myinfo, sizeof(myinfo));
}

EXPORT void retro_get_system_av_info(struct retro_system_av_info *info)
{
	const struct retro_system_av_info myinfo={
		{ 320, 240, 320, 240, 0.0 },
		{ 60.0, 30720.0 }
	};
	memcpy(info, &myinfo, sizeof(myinfo));
}

EXPORT void retro_set_controller_port_device(unsigned port, unsigned device) {}

EXPORT void retro_reset(void)
{
	memset(&state, 0, sizeof(state));
	state.testgroup=init_grp;
	state.testsub=init_sub;
}

EXPORT void retro_run(void)
{
	poller_cb();
	bool canchange=((inpstate[0]&0x00F0)==0);
	inpstate[0]=0x0000;
	inpstate[1]=0x0000;
	for (int i=0;i<16;i++)//it only goes to 12, but a pile of zeroes is harmless.
	{
		inpstate[0]|=(input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i))<<i;
		inpstate[1]|=(input_state_cb(1, RETRO_DEVICE_JOYPAD, 0, i))<<i;
	}
	sound_enable=false;
	
	if (canchange)
	{
		bool changed=false;
		if (inpstate[0]&(1<<RETRO_DEVICE_ID_JOYPAD_UP))
		{
			state.testgroup--;
			if (state.testgroup==0) state.testgroup=numgroups;
			state.testsub='a';
			changed=true;
		}
		if (inpstate[0]&(1<<RETRO_DEVICE_ID_JOYPAD_DOWN))
		{
			state.testgroup++;
			if (state.testgroup-1==numgroups) state.testgroup=1;
			state.testsub='a';
			changed=true;
		}
		if (inpstate[0]&(1<<RETRO_DEVICE_ID_JOYPAD_LEFT))
		{
			state.testsub--;
			if (state.testsub=='a'-1) state.testsub=groupsizes[state.testgroup-1]+'a'-1;
			changed=true;
		}
		if (inpstate[0]&(1<<RETRO_DEVICE_ID_JOYPAD_RIGHT))
		{
			state.testsub++;
			if (state.testsub-1==groupsizes[state.testgroup-1]+'a'-1) state.testsub='a';
			changed=true;
		}
		if (changed)
		{
			state.frame=0;
		}
	}
	
	if (state.testgroup==1)
	{
		if (state.testsub=='a') test1a();
		if (state.testsub=='b') test1b();
		if (state.testsub=='c') test1c();
		if (state.testsub=='d') test1d();
		if (state.testsub=='e') test1e();
	}
	if (state.testgroup==2)
	{
		if (state.testsub=='a') test2a();
		if (state.testsub=='b') test2b();
	}
	if (state.testgroup==3)
	{
		if (state.testsub=='a') test3a();
	}
	
	state.frame++;
	
	if (sound_enable)
	{
		int16_t data[8*64*2];
		for (int i=0;i<8*64;i++)
		{
			data[i*2]=sin(((double)(state.frame*8*64 + i))/30720*2*PI * 440)*4096;
			data[i*2+1]=data[i*2];
		}
		for (int i=0;i<8;i++)
		{
			audio_batch_cb(data+(i*128), 64);
		}
	}
	else
	{
		int16_t data[64*2];
		memset(data, 0, sizeof(data));
		for (int i=0;i<8;i++) audio_batch_cb(data, 64);
	}
	
	video_cb(pixels, 320, 240, sizeof(*pixels));
}

EXPORT size_t retro_serialize_size(void) { return sizeof(state); }
EXPORT bool retro_serialize(void *data, size_t size)
{
	if (size<sizeof(state)) return false;
	memcpy(data, &state, sizeof(state));
	return true;
}
EXPORT bool retro_unserialize(const void *data, size_t size)
{
	if (size<sizeof(state)) return false;
	memcpy(&state, data, sizeof(state));
	return true;
}

EXPORT void retro_cheat_reset(void) {}
EXPORT void retro_cheat_set(unsigned index, bool enabled, const char *code) {}
EXPORT bool retro_load_game(const struct retro_game_info *game)
{
	retro_reset();
	enum retro_pixel_format rgb565=PIXFMT;
	if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb565)) return false;
	return true;
}
EXPORT bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info) { return false; }
EXPORT void retro_unload_game(void) {}
EXPORT unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
EXPORT void *retro_get_memory_data(unsigned id) { return NULL; }
EXPORT size_t retro_get_memory_size(unsigned id) { return 0; }



//blatant zsnes copypasta
unsigned char zfont[]={
0,0,0,0,0,
0x70,0x98,0xA8,0xC8,0x70, 0x20,0x60,0x20,0x20,0x70, 0x70,0x88,0x30,0x40,0xF8, 0x70,0x88,0x30,0x88,0x70,
0x50,0x90,0xF8,0x10,0x10, 0xF8,0x80,0xF0,0x08,0xF0, 0x70,0x80,0xF0,0x88,0x70, 0xF8,0x08,0x10,0x10,0x10,
0x70,0x88,0x70,0x88,0x70, 0x70,0x88,0x78,0x08,0x70, 0x70,0x88,0xF8,0x88,0x88, 0xF0,0x88,0xF0,0x88,0xF0,
0x70,0x88,0x80,0x88,0x70, 0xF0,0x88,0x88,0x88,0xF0, 0xF8,0x80,0xF0,0x80,0xF8, 0xF8,0x80,0xF0,0x80,0x80,
0x78,0x80,0x98,0x88,0x70, 0x88,0x88,0xF8,0x88,0x88, 0xF8,0x20,0x20,0x20,0xF8, 0x78,0x10,0x10,0x90,0x60,
0x90,0xA0,0xE0,0x90,0x88, 0x80,0x80,0x80,0x80,0xF8, 0xD8,0xA8,0xA8,0xA8,0x88, 0xC8,0xA8,0xA8,0xA8,0x98,
0x70,0x88,0x88,0x88,0x70, 0xF0,0x88,0xF0,0x80,0x80, 0x70,0x88,0xA8,0x90,0x68, 0xF0,0x88,0xF0,0x90,0x88,
0x78,0x80,0x70,0x08,0xF0, 0xF8,0x20,0x20,0x20,0x20, 0x88,0x88,0x88,0x88,0x70, 0x88,0x88,0x50,0x50,0x20,
0x88,0xA8,0xA8,0xA8,0x50, 0x88,0x50,0x20,0x50,0x88, 0x88,0x50,0x20,0x20,0x20, 0xF8,0x10,0x20,0x40,0xF8,
0x00,0x00,0xF8,0x00,0x00, 0x00,0x00,0x00,0x00,0xF8, 0x68,0x90,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x20,
0x08,0x10,0x20,0x40,0x80, 0x10,0x20,0x40,0x20,0x10, 0x40,0x20,0x10,0x20,0x40, 0x70,0x40,0x40,0x40,0x70,
0x70,0x10,0x10,0x10,0x70, 0x00,0x20,0x00,0x20,0x00, 0x60,0x98,0x70,0x98,0x68, 0x20,0x20,0xA8,0x70,0x20,
0x50,0xF8,0x50,0xF8,0x50, 0x00,0xF8,0x00,0xF8,0x00, 0x48,0x90,0x00,0x00,0x00, 0x80,0x40,0x20,0x10,0x08,
0xA8,0x70,0xF8,0x70,0xA8, 0x70,0x88,0x30,0x00,0x20, 0x88,0x10,0x20,0x40,0x88, 0x20,0x20,0xF8,0x20,0x20,
0x00,0x00,0x00,0x20,0x40, 0x30,0x40,0x40,0x40,0x30, 0x60,0x10,0x10,0x10,0x60, 0x70,0x98,0xB8,0x80,0x70,
0x20,0x40,0x00,0x00,0x00, 0x20,0x20,0x20,0x00,0x20, 0x78,0xA0,0x70,0x28,0xF0, 0x00,0x20,0x00,0x20,0x40,
0x40,0x20,0x00,0x00,0x00, 0x20,0x50,0x00,0x00,0x00, 0x30,0x40,0xC0,0x40,0x30, 0x60,0x10,0x18,0x10,0x60,
0x20,0x20,0x70,0x70,0xF8, 0xF8,0x70,0x70,0x20,0x20, 0x08,0x38,0xF8,0x38,0x08, 0x80,0xE0,0xF8,0xE0,0x80,
0x20,0x60,0xF8,0x60,0x20, 0x38,0x20,0x30,0x08,0xB0, 0xFC,0x84,0xFC,0x00,0x00, 0x00,0xFC,0x00,0x00,0x00,
0xF8,0x88,0x88,0x88,0xF8,
};

unsigned char convtable[256]={
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x3E,0x33,0x31,0x3F,0x37,0x2F,0x3D,0x3A,0x3B,0x35,0x38,0x39,0x25,0x28,0x29,
0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x2E,0x40,0x2A,0x32,0x2B,0x36,
0x3C,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,
0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x2C,0x34,0x2D,0x42,0x26,
0x41,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,
0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x43,0x00,0x44,0x27,0x00,
0x0D,0x1F,0x0F,0x0B,0x0B,0x0B,0x0B,0x0D,0x0F,0x0F,0x0F,0x13,0x13,0x13,0x0B,0x0B,
0x0F,0x0B,0x0B,0x19,0x19,0x19,0x1F,0x1F,0x23,0x19,0x1F,0x0D,0x10,0x23,0x1A,0x10,
0x0B,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x5B,0x5C,
0x5D,0x5E,0x5F,0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,
0x6D,0x6E,0x6F,0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B,0x7C,
0x7D,0x7E,0x7F,0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4D,0x4C,0x4B,0x4A,0x45,0x46,0x47,0x48,0x49,
};

void renderchr(int chr, int x, int y)
{
	int ix;
	int iy;
	
	for (iy=0;iy<5;iy++)
	for (ix=0;ix<8;ix++)
	{
		if ((zfont[convtable[chr]*5 + iy]>>ix)&1)
		{
			pixels[y+iy][x+(ix^7)]=p_wht;
		}
	}
}

void renderstr(const char * str, int x, int y)
{
	int i;
	for (i=0;str[i];i++)
	{
		renderchr(str[i], x+i*8, y);
	}
}


// Karl Malbrain's compact CRC-32.
// See "A compact CCITT crc16 and crc32 C implementation that balances processor cache usage against speed":
// http://www.geocities.ws/malbrain/

unsigned long Crc32[] = {
0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

unsigned long crc32_calc (unsigned char *ptr, unsigned cnt, unsigned long crc)
{
    while( cnt-- ) {
        crc = ( crc >> 4 ) ^ Crc32[(crc & 0xf) ^ (*ptr & 0xf)];
        crc = ( crc >> 4 ) ^ Crc32[(crc & 0xf) ^ (*ptr++ >> 4)];
    }
 
    return crc;
}
