#include "minir.h"
#include <string.h>
#include <stdlib.h>
#include "libretro.h"

static void none_set_callback(struct inputkb * this,
                              void (*key_cb)(struct inputkb * subject,
                                             unsigned int keyboard, int scancode, int libretrocode, 
                                             bool down, bool silent, void* userdata),
                              void* userdata)
{}
static void none_poll(struct inputkb * this) {}
static void none_free(struct inputkb * this) {}

struct inputkb * inputkb_create_none(uintptr_t windowhandle)
{
	static struct inputkb this={ none_set_callback, none_poll, none_free };
	return &this;
}

static struct inputraw * inputraw_create(const char * backend, uintptr_t windowhandle)
{
#ifdef INPUT_XINPUT2
	if (!strcmp(backend, "XInput2")) return _inputraw_create_xinput2(windowhandle);
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
	unsigned char newstate[1024];
	memset(newstate, 0, sizeof(newstate));
	for (int i=0;i<this->ir->keyboard_num_keyboards(this->ir);i++)
	{
		if (!this->ir->keyboard_poll(this->ir, i, newstate)) memset(newstate, 0, sizeof(newstate));
		for (int j=0;j<1024;j++)
		{
			if (newstate[j]!=this->state[i][j])
			{
				this->state[i][j]=newstate[j];
				this->key_cb((struct inputkb*)this, i, j, inputkb_x11_translate_key(j), this->state[i][j], true, this->userdata);
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

static unsigned int return1(struct inputraw * this) { return 1; }

void _inputraw_x11_keyboard_create_shared(struct inputraw * this)
{
	this->keyboard_num_keyboards=return1;
	this->keyboard_num_keys=NULL;
	this->keyboard_get_map=NULL;
}

struct inputkb * inputkb_create_gdk(uintptr_t windowhandle);
struct inputkb * inputkb_create(const char * backend, uintptr_t windowhandle)
{
#ifdef INPUT_GDK
	if (!strcmp(backend, "GDK")) return inputkb_create_gdk(windowhandle);
#endif
	if (!strcmp(backend, "None")) return inputkb_create_none(windowhandle);
	
	struct inputraw * raw=inputraw_create(backend,windowhandle);
	
	if (!raw) return NULL;
	struct inputkb_compat * this=malloc(sizeof(struct inputkb_compat));
	inputkb_x11_translate_init();
	this->i.set_callback=ikbc_set_callback;
	this->i.poll=ikbc_poll;
	this->i.free=ikbc_free;
	this->ir=raw;
	memset(this->state, 0, sizeof(this->state));
	return (struct inputkb*)this;
}



const char * const * inputkb_supported_backends()
{
	static const char * backends[]={
#ifdef INPUT_GDK
		"GDK",
#endif
#ifdef INPUT_XINPUT2
		"XInput2",
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
