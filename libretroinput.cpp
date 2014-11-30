#include "minir.h"
#include <stdlib.h>
#include <string.h>
#include "libretro.h"

#define this This

struct libretroinput_impl {
	struct libretroinput i;
	
	struct inputmapper * in;
	
	unsigned int joypad_num_ports;
	unsigned int * joypad_index_start;
	unsigned int * joypad_index_len;
	bool joypad_block_opposing;
};

static void set_input(struct libretroinput * this_, struct inputmapper * in)
{
	struct libretroinput_impl * this=(struct libretroinput_impl*)this_;
	this->in=in;
}

static int16_t query(struct libretroinput * this_, unsigned port, unsigned device, unsigned index, unsigned id)
{
	struct libretroinput_impl * this=(struct libretroinput_impl*)this_;
	
	switch (device)
	{
		case RETRO_DEVICE_JOYPAD:
		{
			if (port>=this->joypad_num_ports) return 0;
			if (id>=16) return 0;
			unsigned max=this->joypad_index_len[port];
			if (index*16+id>=max) return 0;
			if (id>=4 && id<=7 && this->joypad_block_opposing)
			{
				if (this->in->button(this->in, this->joypad_index_start[port]+(index*16+(id^1)), false)) return false;
			}
			return (this->in->button(this->in, this->joypad_index_start[port]+(index*16+id), false));
		}
case RETRO_DEVICE_ANALOG:
{
//TODO: Do this properly.
if (port!=0) return 0;
if (index!=RETRO_DEVICE_INDEX_ANALOG_LEFT) return 0;
unsigned int base_id=(id==RETRO_DEVICE_ID_ANALOG_X ? 6 : 4);
int16_t ret=0;
if (this->in->button(this->in, this->joypad_index_start[port]+base_id, false)) ret-=0x7FFF;
if (this->in->button(this->in, this->joypad_index_start[port]+base_id+1, false)) ret+=0x7FFF;
return ret;
}
	}
	
	return 0;
}

static void joypad_set_inputs(struct libretroinput * this_, unsigned port, unsigned int inputstart, unsigned int len)
{
	struct libretroinput_impl * this=(struct libretroinput_impl*)this_;
	
	if (port>=this->joypad_num_ports)
	{
		this->joypad_index_start=realloc(this->joypad_index_start, sizeof(unsigned int)*(port+1));
		this->joypad_index_len=realloc(this->joypad_index_len, sizeof(unsigned int)*(port+1));
		this->joypad_num_ports=port+1;
	}
	
	this->joypad_index_start[port]=inputstart;
	this->joypad_index_len[port]=len;
}

static void joypad_set_block_opposing(struct libretroinput * this_, bool block)
{
	struct libretroinput_impl * this=(struct libretroinput_impl*)this_;
	this->joypad_block_opposing=block;
}

static void free_(struct libretroinput * this_)
{
	struct libretroinput_impl * this=(struct libretroinput_impl*)this_;
	
	free(this->joypad_index_start);
	free(this->joypad_index_len);
	free(this);
}

struct libretroinput * libretroinput_create(struct inputmapper * in)
{
	struct libretroinput_impl * this=malloc(sizeof(struct libretroinput_impl));
	this->i.set_input=set_input;
	this->i.query=query;
	this->i.joypad_set_inputs=joypad_set_inputs;
	this->i.joypad_set_block_opposing=joypad_set_block_opposing;
	this->i.free=free_;
	
	this->joypad_num_ports=0;
	this->joypad_index_len=NULL;
	this->joypad_index_start=NULL;
	this->joypad_block_opposing=true;
	
	this->in=in;
	return (struct libretroinput*)this;
}
