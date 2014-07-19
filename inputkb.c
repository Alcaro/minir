#include "minir.h"
#include <string.h>
#include <stdlib.h>
#include "libretro.h"

static unsigned int keyboard_num_keyboards(struct inputraw * this) { return 0; }
static unsigned int keyboard_num_keys(struct inputraw * this) { return 1; }
static bool keyboard_poll(struct inputraw * this, unsigned int kb_id, unsigned char * keys) { keys[0]=0; return true; }
static void keyboard_get_map(struct inputraw * this, const unsigned int ** keycode_to_libretro,
                                                     const unsigned int ** libretro_to_keycode)
{
	static const unsigned int * fakemap=NULL;
	if (!fakemap) fakemap=calloc(RETROK_LAST, sizeof(unsigned int));
	*keycode_to_libretro=fakemap;
	*libretro_to_keycode=fakemap;
}
static void free_(struct inputraw * this) {}

struct inputraw * _inputraw_create_none(uintptr_t windowhandle)
{
	static struct inputraw this={ keyboard_num_keyboards, keyboard_num_keys, keyboard_poll, keyboard_get_map, free_ };
	return &this;
}

struct inputraw * inputraw_create(const char * backend, uintptr_t windowhandle)
{
#ifdef INPUT_X11_XINPUT2
	if (!strcmp(backend, "X11-XInput2")) return _inputraw_create_x11_xinput2(windowhandle);
#endif
#ifdef INPUT_RAWINPUT
	if (!strcmp(backend, "RawInput")) return _inputraw_create_rawinput(windowhandle);
#endif
#ifdef INPUT_X11
	if (!strcmp(backend, "X11")) return _inputraw_create_x11(windowhandle);
#endif
#ifdef INPUT_DIRECTINPUT
	if (!strcmp(backend, "DirectInput")) return _inputraw_create_directinput(windowhandle);
#endif
	if (!strcmp(backend, "None")) return _inputraw_create_none(windowhandle);
	return NULL;
}


//terrible spacing - it's temp code, no point wasting time on making it proper
struct inputkb_compat {
	struct inputkb i;
	struct inputraw * ir;
void (*key_cb)(struct inputkb * subject,
	                                    unsigned int keyboard, int scancode, int libretrocode,
	                                    bool down, bool changed, void* userdata);
	                     void* userdata;
	unsigned char state[32][1024];
};

static void ikbc_set_callback(struct inputkb * this_,
	                     void (*key_cb)(struct inputkb * subject,
	                                    unsigned int keyboard, int scancode, int libretrocode, 
	                                    bool down, bool changed, void* userdata),
	                     void* userdata)
{
	struct inputkb_compat * this=(struct inputkb_compat*)this_;
	this->key_cb=key_cb;
	this->userdata=userdata;
}

static void ikbc_poll(struct inputkb * this_)
{
struct inputkb_compat * this=(struct inputkb_compat*)this_;
const unsigned int * kclr;
this->ir->keyboard_get_map(this->ir, &kclr, NULL);
	
	unsigned char newstate[1024];
	memset(newstate, 0, sizeof(newstate));
	for (int i=0;i<32;i++)
	{
		if (!this->ir->keyboard_poll(this->ir, i, newstate)) memset(newstate, 0, sizeof(newstate));
		for (int j=0;j<1024;j++)
		{
			if (newstate[j]!=this->state[i][j])
			{
				this->state[i][j]=newstate[j];
				this->key_cb((struct inputkb*)this, i, j, kclr[j]?kclr[j]:-1, this->state[i][j], true, this->userdata);
			}
		}
	}
}
static void ikbc_free(struct inputkb * this_)
{
	struct inputkb_compat * this=(struct inputkb_compat*)this_;
	this->ir->free(this->ir);
	free(this);
}

struct inputkb * inputkb_create(const char * backend, uintptr_t windowhandle)
{
	struct inputkb_compat * this=malloc(sizeof(struct inputkb_compat));
	this->i.set_callback=ikbc_set_callback;
	this->i.poll=ikbc_poll;
	this->i.free=ikbc_free;
	memset(this->state, 0, sizeof(this->state));
	this->ir=inputraw_create(backend,windowhandle);
	return (struct inputkb*)this;
}



const char * const * inputkb_supported_backends()
{
	static const char * backends[]={
#ifdef INPUT_X11_XINPUT2
		"X11-XInput2",
#endif
#ifdef INPUT_RAWINPUT
		"RawInput",
#endif
#ifdef INPUT_X11
		"X11",
#endif
#ifdef INPUT_DIRECTINPUT
		"DirectInput",
#endif
		"None",
		NULL
	};
	return backends;
}
