//TODO: use inputkb::refresh()
#include "io.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "libretro.h"
//static void dump_buttons(const unsigned int * buttons){for(int i=0;buttons[i];i++)printf("%.8X,",buttons[i]);puts("00000000");}

#define this This

namespace {

static const char * const * keynames;

static unsigned int str_to_id(const char * str, int str_len)
{
	for (int i=0;i<RETROK_LAST;i++)
	{
		if (keynames[i] && !strncmp(str, keynames[i], str_len) && !keynames[i][str_len]) return i;
	}
	return 0;
}

static const char * id_to_str(unsigned int id)
{
	return keynames[id];
}

//Button descriptor specification:
//ssssxxxx xxxxxxxx xxxxxxxx xxxxxxxx
//s: Selector, tells what device type the button is on.
//x: The meaning varies depending on device. Not all bits are used by all device types.
//
//Keyboard:
//ssss0000 00000000 dddddlkk kkkkkkkk
//s: The selector is 0000 for keyboards.
//0: The 0s are not used.
//d: Device ID. If there are multiple keyboards, this tells which one is relevant.
//n: Native flag. If set, the key index is a native scancode; if not, it's a RETROK_x.
//k: Key index. See above for details.
//Special case: 00000000 00000000 00000000 00000000 is never held, can never trigger, and is used as terminator.

static uint32_t * parse_descriptor(const char * descriptor, const char ** descriptor_end)
{
	uint32_t * rules;
	
	if (descriptor[0]=='K' && descriptor[1]=='B' && isdigit(descriptor[2]))
	{
		unsigned int keyboardid=strtoul(descriptor+2, (char**)&descriptor, 10);
		if (descriptor[0]!=':' || descriptor[1]!=':') return NULL;
		descriptor+=2;
		
		if (keyboardid<0 || keyboardid>31)  return NULL;
		
		rules=malloc(sizeof(uint32_t));
		int rulelen=1;
		
		const char * next;
		do {
			next=strchr(descriptor, '+');
			const char * next2=strchr(descriptor, ',');
			if (!next) next=next2;
			if (next && next2 && next2<next) next=next2;
			if (!next) next=strchr(descriptor, '\0');
			
			unsigned int keyid=str_to_id(descriptor, next-descriptor);
			if (!keyid)
			{
				if (*descriptor!='x') goto bad;
				keyid=strtoul(descriptor, (char**)&descriptor, 16);
				if (keyid&0xFFFFFC00) goto bad;
				if (descriptor!=next) goto bad;
				keyid|=0x00000400;
			}
			
			keyid|=keyboardid<<11;
			
			if (*next && *next!=',')
			{
				rulelen++;
				rules=realloc(rules, sizeof(uint32_t)*rulelen);
				rules[rulelen-1]=keyid;
			}
			else
			{
				rules[0]=keyid;
			}
			descriptor=next+1;
		} while (*next && *next!=',');
		
		if (descriptor_end)
		{
			if (*next==',') *descriptor_end=next+1;
			else *descriptor_end=NULL;
		}
		
		rulelen++;
		rules=realloc(rules, sizeof(uint32_t)*rulelen);
		rules[rulelen-1]=0;
		
		return rules;
	}
	
	return NULL;
	
bad:
	free(rules);
	return NULL;
}

static uint32_t ** parse_chain_descriptor(const char * descriptor)
{
	int chainlen=0;
	uint32_t ** ret=NULL;
	while (descriptor)
	{
		while (*descriptor==' ') descriptor++;
		uint32_t * next=parse_descriptor(descriptor, &descriptor);
		if (!next)
		{
			descriptor=strchr(descriptor, ',');
			if (descriptor) descriptor++;
			continue;
		}
		ret=realloc(ret, sizeof(uint32_t*)*(chainlen+1));
		ret[chainlen]=next;
		chainlen++;
	}
	if (ret)
	{
		ret=realloc(ret, sizeof(uint32_t*)*(chainlen+1));
		ret[chainlen]=NULL;
	}
	return ret;
}

static void delete_chain_rule(uint32_t ** rules)
{
	if (!rules) return;
	uint32_t ** tmp=rules;
	while (*tmp)
	{
		free(*tmp);
		tmp++;
	}
	free(rules);
}

//Note that this one will sort the buttons and delete duplicates.
static char * create_descriptor(uint32_t * buttons)
{
	uint32_t base=buttons[0];
	
	unsigned int nummod=0;
	if (buttons[1])
	{
		uint32_t * sorted=buttons+1;
		uint32_t * sortend=buttons+1;
		uint32_t * unsorted=buttons+1;
		while (*unsorted)
		{
			uint32_t thisone=*unsorted;
			*unsorted=0xFFFFFFFF;
			uint32_t * tmp=sorted;
			while (thisone>*tmp) tmp++;
			if (thisone!=*tmp && thisone!=base)
			{
				memmove(tmp+1, tmp, (sortend-tmp)*sizeof(uint32_t));
				*tmp=thisone;
				sortend++;
			}
			unsorted++;
		}
		*sortend=0x00000000;
		nummod=sortend-sorted;
	}
	
	int type=base>>28;
	switch (type)
	{
		case 0:
		{
			//longest possible: KB31::KP_Multiply+KP_Multiply+KP_Multiply+KP_Multiply(nul)
			//we won't hit it since there is only one KP_Multiply, but we won't overshoot too badly.
			//overshooting by a few hundred bytes won't hurt anyone, anyways
			char * out=malloc(6+11+nummod*12+1);
			
			int keyboard=(base>>11)&31;
			char * outat=out+sprintf(out, "KB%i:", keyboard);
			char * setcolon=outat;
			
			uint32_t * terminator=buttons+nummod+1;
			*terminator=base;
			buttons++;
			while (buttons<=terminator)
			{
				int key=(*buttons&0x7FF);
				if (key&0x400) outat+=sprintf(outat, "+x%.2X", key&0x3FF);
				else outat+=sprintf(outat, "+%s", id_to_str(key&0x3FF));
				buttons++;
			}
			*terminator=0x00000000;
			*setcolon=':';
			return out;
		}
	}
	return NULL;
}

static char * create_chain_descriptor(uint32_t ** buttons)
{
	if (!buttons || !*buttons) return NULL;
	char * out=create_descriptor(*buttons);
	int len=strlen(out);
	buttons++;
	while (*buttons)
	{
		char * newpart=create_descriptor(*buttons);
		int newlen=strlen(newpart);
		out=realloc(out, len+2+newlen+1);
		out[len+0]=',';
		out[len+1]=' ';
		strcpy(out+len+2, newpart);
		len+=2+newlen;
		buttons++;
	}
	return out;
}



struct inputmapper_impl {
	struct inputmapper i;
	
	inputkb* kb;
	
	uint8_t * kb_state;
	//0=released
	//1=held
	//2=released this frame
	//3=pressed this frame
	//length is 0x800 * highest observed keyboard ID
	uint8_t kb_nkb;
	bool kb_anylastframe;
	
	//char padding[2];
	
	uint32_t lastreleased;
	
	unsigned int numbuttons;
	uint32_t * ** buttonrules;
	uint32_t * * shiftstates_for;
	
	//data structure docs:
	//buttonrules:
	// an array of uint32*, one for each possible combination of inputs to trigger this one
	// each of the arrays is called 'chain'
	// if one of them is active, it's active
	// terminated with NULL
	//*buttonrules:
	// an array of uint32
	// the first one is the base key; it alone decides if oneshots fire
	// the other ones are which other keys must be held for this array to be true
	// terminated with 0x00000000
	//
	//shiftstates_for:
	// an array of uint32*, one for each keycode (each keyboard counts separately)
	// not terminated, since it's random access; length is numkeyboards*keyboardlen
	//*shiftstates_for:
	// an array of uint32, telling which keys must be in their requested state (as defined by *buttonrules) for anything to be active
};

static bool button_shiftstates(struct inputmapper * this_, unsigned int id, bool oneshot);
static bool button(struct inputmapper * this_, unsigned int id, bool oneshot);

static void reset_shiftstates(struct inputmapper_impl * this)
{
	if (this->shiftstates_for)
	{
		for (int i=0;i<0x800*this->kb_nkb;i++) free(this->shiftstates_for[i]);
		free(this->shiftstates_for);
		this->shiftstates_for=NULL;
	}
	this->i.button=button_shiftstates;
}

static bool map_key(struct inputmapper * this_, const char * descriptor, unsigned int id)
{
	struct inputmapper_impl * this=(struct inputmapper_impl*)this_;
	
	if (id<this->numbuttons)
	{
		free(this->buttonrules[id]);
		this->buttonrules[id]=NULL;
	}
	
	if (!descriptor || !*descriptor) return true;
	
	uint32_t ** rules=parse_chain_descriptor(descriptor);
	if (!rules) return false;
	
	if (id>=this->numbuttons)
	{
		this->buttonrules=realloc(this->buttonrules, sizeof(uint32_t**)*(id+1));
		for (unsigned int i=this->numbuttons;i<id;i++)//not initializing the last one; we'll set it to non-NULL soon enough
		{
			this->buttonrules[i]=NULL;
		}
		this->numbuttons=id+1;
	}
	this->buttonrules[id]=rules;
	
	//we can't keep them updated incrementally; we'd need to check all others to check whether a
	// removed one is used by another mapping. They're supposed to be remapped in large bunches, anyways.
	reset_shiftstates(this);
	return true;
}

static char * last(struct inputmapper * this_)
{
	struct inputmapper_impl * this=(struct inputmapper_impl*)this_;
	if (!this->lastreleased) return NULL;
	uint32_t lastreleased=this->lastreleased;
	this->lastreleased=0x00000000;
	
	unsigned int type=lastreleased>>28;
	switch (type)
	{
		case 0:
		{
			unsigned int kb=(lastreleased>>11)&31;
			
			int numkeys=1;
			int keybuflen=2;
			uint32_t * keys=malloc(sizeof(uint32_t)*keybuflen);
			uint32_t keyboardflags=kb<<11;
			
			keys[0]=lastreleased;
			
			for (int key=0;key<0x800;key++)
			{
				if (this->kb_state[kb*0x800 + key]==1)
				{
					keys[numkeys]=keyboardflags | key;
					numkeys++;
					
					if (numkeys==keybuflen)
					{
						keybuflen*=2;
						keys=realloc(keys, sizeof(uint32_t)*keybuflen);
					}
				}
			}
			keys[numkeys]=0x00000000;
			char * ret=create_descriptor(keys);
			free(keys);
			return ret;
		}
	}
	return NULL;
}

static bool button_shiftstates(struct inputmapper * this_, unsigned int id, bool oneshot)
{
	struct inputmapper_impl * this=(struct inputmapper_impl*)this_;
	
	this->shiftstates_for=malloc(sizeof(uint32_t*)*0x800*this->kb_nkb);
	memset(this->shiftstates_for, 0, sizeof(uint32_t*)*0x800*this->kb_nkb);
	for (unsigned int i=0;i<this->numbuttons;i++)
	{
		uint32_t ** buttonrulechain=this->buttonrules[i];
		if (!buttonrulechain) continue;
		while (*buttonrulechain)
		{
			uint32_t * buttonrules=*(buttonrulechain++);
			uint32_t baserule=*buttonrules;
			
			unsigned int basekb=(baserule>>11)&31;
			unsigned int basekey=(baserule&0x7FF);
			unsigned int baseid=basekb*0x800 + basekey;
			if (basekb >= this->kb_nkb) continue;
			
			buttonrules++;
			while (*buttonrules)
			{
				uint32_t rule=*buttonrules;
				
				if (this->shiftstates_for[baseid])
				{
					//check if this one is already listed
					unsigned int pos=0;
					while (this->shiftstates_for[baseid][pos])
					{
						//sort it in (looks like a funky insertion sort)
						uint32_t a=rule;
						uint32_t b=this->shiftstates_for[baseid][pos];
						if (a==b) goto skipthis;
						if (a>b)
						{
							rule=b;
							this->shiftstates_for[baseid][pos]=a;
						}
						pos++;
					}
					this->shiftstates_for[baseid]=realloc(this->shiftstates_for[baseid], sizeof(uint32_t)*(pos+2));
					this->shiftstates_for[baseid][pos]=rule;
					this->shiftstates_for[baseid][pos+1]=0x00000000;
				}
				else
				{
					this->shiftstates_for[baseid]=malloc(sizeof(uint32_t)*2);
					this->shiftstates_for[baseid][0]=rule;
					this->shiftstates_for[baseid][1]=0x00000000;
				}
				
			skipthis:;
				buttonrules++;
			}
		}
	}
	
	this->i.button=button;
	return button(this_, id, oneshot);
}

static bool button_i(struct inputmapper_impl * this, uint32_t * buttons, bool oneshot)
{
	uint32_t type=buttons[0]>>28;
	switch (type)
	{
		case 0:
		{
			int kb=(buttons[0]>>11)&31;
			int key=(buttons[0]&0x7FF);
			
			if (kb >= this->kb_nkb) return false;
			if (!(this->kb_state[kb*0x800 + key]&1)) return false;
			if (oneshot && this->kb_state[kb*0x800 + key]!=3) return false;
			
			unsigned int numenabledshiftstates=0;
			buttons++;
			while (*buttons)
			{
				if (!(this->kb_state[kb*0x800 + (*buttons&0x7FF)]&1)) return false;
				buttons++;
				numenabledshiftstates++;
			}
			
			uint32_t * shiftstates=this->shiftstates_for[kb*0x800 + key];
			if (shiftstates)
			{
				while (*shiftstates)
				{
					if (this->kb_state[kb*0x800 + (*shiftstates&0x7FF)]&1) numenabledshiftstates--;
					shiftstates++;
				}
			}
			return (numenabledshiftstates==0);
		}
		break;
	}
	return false;
}

static bool button(struct inputmapper * this_, unsigned int id, bool oneshot)
{
	struct inputmapper_impl * this=(struct inputmapper_impl*)this_;
	if (id>=this->numbuttons || !this->buttonrules[id]) return false;
	uint32_t ** buttons=this->buttonrules[id];
	
	while (*buttons)
	{
		if (button_i(this, *buttons, oneshot)) return true;
		buttons++;
	}
	return false;
}

static void poll(struct inputmapper * this_)
{
	struct inputmapper_impl * this=(struct inputmapper_impl*)this_;
	if (this->kb) this->kb->poll();
	if (this->kb_anylastframe)
	{
		this->kb_anylastframe=false;
		for (unsigned int i=0;i<0x800*this->kb_nkb;i++)
		{
			if (this->kb_state[i]>=2) this->kb_state[i]-=2;
			if (this->kb_state[i]>=2) this->kb_anylastframe=true;
		}
	}
}

void inputmapper_kb_cb(struct inputmapper_impl * this, unsigned int keyboard, unsigned int scancode, unsigned int libretrocode, bool down)
{
	if (keyboard >= this->kb_nkb)
	{
		reset_shiftstates(this);
		this->kb_state=realloc(this->kb_state, 0x800*(keyboard+1));
		memset(this->kb_state + 0x800*this->kb_nkb, 0, 0x800*(keyboard+1-this->kb_nkb));
		this->kb_nkb=keyboard+1;
	}
	
	unsigned int key;
	if(0);
	else if (libretrocode>0) key=libretrocode;
	else                     key=scancode|0x400;
	
	key|=keyboard*0x800;
	if ((this->kb_state[key]&1) != down)
	{
		//this->kb_state[key]=(down?1:0) + (changed?4:0);
		//this->kb_anylastframe |= changed;
		//if (changed && !down) this->lastreleased = key;
		this->kb_state[key]=(down?1:0) + 4;
		this->kb_anylastframe = true;
		if (!down) this->lastreleased = key;
	}
}

static void reset(struct inputmapper_impl * this)
{
	reset_shiftstates(this);
	
	if (this->kb) delete this->kb;
	this->kb=NULL;
	
	this->kb_nkb=0;
	free(this->kb_state);
	this->kb_state=NULL;
	this->kb_anylastframe=false;
	
	if (this->buttonrules)
	{
		for (unsigned int i=0;i<this->numbuttons;i++) delete_chain_rule(this->buttonrules[i]);
		free(this->buttonrules);
		this->buttonrules=NULL;
		this->numbuttons=0;
	}
}

static void set_inputkb(struct inputmapper * this_, struct inputkb * kb)
{
	struct inputmapper_impl * this=(struct inputmapper_impl*)this_;
	
	reset(this);
	
	this->kb=kb;
	if (this->kb) this->kb->set_kb_cb(bind_this(inputmapper_kb_cb));
}

static void free_(struct inputmapper * this_)
{
	struct inputmapper_impl * this=(struct inputmapper_impl*)this_;
	reset(this);
	free(this);
}

}

char * inputmapper_normalize(const char * descriptor)
{
	uint32_t ** keys=parse_chain_descriptor(descriptor);
	if (!keys) return NULL;
	char * newdesc=create_chain_descriptor(keys);
	delete_chain_rule(keys);
	return newdesc;
}

struct inputmapper inputmapper_iface = {
	button, last, set_inputkb, map_key, poll, free_
};
struct inputmapper * inputmapper_create()
{
	struct inputmapper_impl * this=malloc(sizeof(struct inputmapper_impl));
	memcpy(&this->i, &inputmapper_iface, sizeof(struct inputmapper));
	
	this->kb=NULL;
	this->kb_state=NULL;
	this->kb_nkb=0;
	this->kb_anylastframe=false;
	
	this->lastreleased=0;
	
	this->numbuttons=0;
	this->buttonrules=NULL;
	this->shiftstates_for=NULL;
	
	keynames=inputkb::keynames();
	
	return (struct inputmapper*)this;
}
