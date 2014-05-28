#include "minir.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "libretro.h"
//static void dump_buttons(const unsigned int * buttons){for(int i=0;buttons[i];i++)printf("%.8X,",buttons[i]);puts("00000000");}

		//"Space",      "!",           "\"",         "#",           "$",          NULL,          "&",          "'",
		//"(",          ")",           "*",          "+",           ",",          "-",           ".",          "/",
		//"0",          "1",           "2",          "3",           "4",          "5",           "6",          "7",
		//"8",          "9",           ":",          ";",           "<",          "=",           ">",          "?",
		//"At",         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		//NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		//NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		//NULL,         NULL,          NULL,         "[",           "\\",         "]",           "^",          "_",
		//"`",          "a",           "b",          "c",           "d",          "e",           "f",          "g",
		//"h",          "i",           "j",          "k",           "l",          "m",           "n",          "o",
		//"p",          "q",           "r",          "s",           "t",          "u",           "v",          "w",
		//"x",          "y",           "z",          NULL,          NULL,         NULL,          NULL,         "Delete",

static const char * const keynames[]={
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		"Backspace",   "Tab",        NULL,         NULL,          "Clear",      "Return",      NULL,         NULL,
		NULL,         NULL,          NULL,         "Pause",       NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         "Escape",      NULL,         NULL,          NULL,         NULL,
		"Space",      "Exclaim",     "QuoteD",     "Hash",        "Dollar",     NULL,          "Ampersand",  "QuoteS",
		"ParenL",     "ParenR",      "Asterisk",   "Plus",        "Comma",      "Minus",       "Period",     "Slash",
		"0",          "1",           "2",          "3",           "4",          "5",           "6",          "7",
		"8",          "9",           "Colon",      "Semicolon",   "Less",       "Equals",      "Greater",    "Question",
		"At",         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         "BracketL",    "Backslash",  "BracketR",    "Caret",      "Underscore",
		"Backtick",   "A",           "B",          "C",           "D",          "E",           "F",          "G",
		"H",          "I",           "J",          "K",           "L",          "M",           "N",          "O",
		"P",          "Q",           "R",          "S",           "T",          "U",           "V",          "W",
		"X",          "Y",           "Z",          NULL,          NULL,         NULL,          NULL,         "Delete",
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
		"KP0",        "KP1",         "KP2",        "KP3",         "KP4",        "KP5",         "KP6",        "KP7",
		"KP8",        "KP9",         "KP_Period",  "KP_Divide",   "KP_Multiply", "KP_Minus",   "KP_Plus",    "KP_Enter",
		"KP_Equals",  "Up",          "Down",       "Right",       "Left",       "Insert",      "Home",       "End",
		"PageUp",     "PageDown",    "F1",         "F2",          "F3",         "F4",          "F5",         "F6",
		"F7",         "F8",          "F9",         "F10",         "F11",        "F12",         "F13",        "F14",
		"F15",        NULL,          NULL,         NULL,          "NumLock",    "CapsLock",    "ScrollLock", "ShiftR",
		"ShiftL",     "CtrlR",       "CtrlL",      "AltR",        "AltL",       "MetaR",       "MetaL",      "SuperL",
		"SuperR",     "Mode",        "Compose",    "Help",        "Print",      "SysRq",       "Break",      "Menu",
		"Power",      "Euro",        "Undo",
};

static unsigned int str_to_id(const char * str, int str_len)
{
	for (int i=0;i<RETROK_LAST;i++)
	{
		if (keynames[i] && !strncmp(str, keynames[i], str_len) && !keynames[i][str_len]) return i;
	}
	return 0;
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
//l: Libretro keyboard flag. Only set when sanitizing a descriptor without a translation table.
//k: Key index. It's a native keycode; an index to the array from inputraw::keyboard_poll.
//Special case: 00000000 00000000 00000000 00000000 is never held, can never trigger, and is used as terminator.

static unsigned int * parse_descriptor(const char * descriptor, const char ** descriptor_end, const unsigned int * libretro_to_keycode)
{
	unsigned int * rules=rules;
	
	if (descriptor[0]=='K' && descriptor[1]=='B' && isdigit(descriptor[2]))
	{
		unsigned int keyboardid=strtoul(descriptor+2, (char**)&descriptor, 0);
		if (descriptor[0]!=':' || descriptor[1]!=':') return NULL;
		descriptor+=2;
		
		keyboardid--;
		if (keyboardid<0 || keyboardid>31)  return NULL;
		
		rules=malloc(sizeof(unsigned int));
		int rulelen=1;
		
		const char * next;
		do {
			next=strchr(descriptor, '+');
			const char * next2=strchr(descriptor, ',');
			if (!next) next=next2;
			if (next && next2 && next2<next) next=next2;
			if (!next) next=strchr(descriptor, '\0');
			
			unsigned int keyid=str_to_id(descriptor, next-descriptor);
			if (keyid)
			{
				if (libretro_to_keycode) keyid=libretro_to_keycode[keyid];
				else keyid|=0x00000400;
			}
			else
			{
				if (*descriptor!='x') goto bad;
				keyid=strtoul(descriptor, (char**)&descriptor, 16);
				if (keyid&0xFFFFFC00) goto bad;
				if (descriptor!=next) goto bad;
			}
			
			keyid|=keyboardid<<11;
			
			if (*next && *next!=',')
			{
				rulelen++;
				rules=realloc(rules, sizeof(unsigned int)*rulelen);
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
		rules=realloc(rules, sizeof(unsigned int)*rulelen);
		rules[rulelen-1]=0;
		
		return rules;
	}
	
	return NULL;
	
bad:
	free(rules);
	return NULL;
}

static unsigned int ** parse_chain_descriptor(const char * descriptor, const unsigned int * libretro_to_keycode)
{
	int chainlen=0;
	unsigned int ** ret=NULL;
	while (descriptor)
	{
		while (*descriptor==' ') descriptor++;
		unsigned int * next=parse_descriptor(descriptor, &descriptor, libretro_to_keycode);
		if (!next)
		{
			descriptor=strchr(descriptor, ',');
			if (descriptor) descriptor++;
			continue;
		}
		ret=realloc(ret, sizeof(unsigned int*)*(chainlen+1));
		ret[chainlen]=next;
		chainlen++;
	}
	if (ret)
	{
		ret=realloc(ret, sizeof(unsigned int*)*(chainlen+1));
		ret[chainlen]=NULL;
	}
	return ret;
}

static void delete_chain_rule(unsigned int ** rules)
{
	if (!rules) return;
	unsigned int ** tmp=rules;
	while (*tmp)
	{
		free(*tmp);
		tmp++;
	}
	free(rules);
}

//Note that this one will sort the buttons and delete duplicates.
static char * create_descriptor(unsigned int * buttons, const unsigned int * keycode_to_libretro, unsigned int keyboardlen)
{
	unsigned int base=buttons[0];
	
	unsigned int nummod=0;
	if (buttons[1])
	{
		unsigned int * sorted=buttons+1;
		unsigned int * sortend=buttons+1;
		unsigned int * unsorted=buttons+1;
		while (*unsorted)
		{
			unsigned int this=*unsorted;
			*unsorted=0xFFFFFFFF;
			unsigned int * tmp=sorted;
			while (this>*tmp) tmp++;
			if (this!=*tmp && this!=base)
			{
				memmove(tmp+1, tmp, (sortend-tmp)*sizeof(unsigned int));
				*tmp=this;
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
			char * outat=out+sprintf(out, "KB%i:", keyboard+1);
			char * setcolon=outat;
			
			unsigned int * terminator=buttons+nummod+1;
			*terminator=base;
			buttons++;
			while (buttons<=terminator)
			{
				int key=(*buttons&0x7FF);
				if (key&0x400) outat+=sprintf(outat, "+%s", keynames[key&0x3FF]);
				else if (key<keyboardlen && keycode_to_libretro[key]) outat+=sprintf(outat, "+%s", keynames[keycode_to_libretro[key]]);
				else outat+=sprintf(outat, "+x%X", key);
				buttons++;
			}
			*terminator=0x00000000;
			*setcolon=':';
			return out;
		}
	}
	return NULL;
}

static char * create_chain_descriptor(unsigned int ** buttons, const unsigned int * keycode_to_libretro, unsigned int keyboardlen)
{
	if (!buttons || !*buttons) return NULL;
	char * out=create_descriptor(*buttons, keycode_to_libretro, keyboardlen);
	int len=strlen(out);
	buttons++;
	while (*buttons)
	{
		char * newpart=create_descriptor(*buttons, keycode_to_libretro, keyboardlen);
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

char * inputmapper_normalize(const char * descriptor)
{
	unsigned int ** keys=parse_chain_descriptor(descriptor, NULL);
	if (!keys) return NULL;
	char * newdesc=create_chain_descriptor(keys, NULL, 0);
	delete_chain_rule(keys);
	return newdesc;
}



struct inputmapper_impl {
	struct inputmapper i;
	
	struct inputraw * in;
	
	const unsigned int * keycode_to_libretro;
	const unsigned int * libretro_to_keycode;
	
	unsigned int numkeyboards;
	unsigned int keyboardlen;
	unsigned char * keyboardstate;
	//0=released
	//1=held
	//2=released this frame
	//3=pressed this frame
	
	unsigned int numbuttons;
	unsigned int * ** buttonrules;
	unsigned int * * shiftstates_for;
	
	//data structure docs:
	//buttonrules:
	// an array of uint*, one for each possible combination of inputs to trigger this one
	// each of the arrays is called 'chain'
	// if one of them is active, it's active
	// terminated with NULL
	//*buttonrules:
	// an array of uint
	// the first one is the base key; it alone decides if oneshots fire
	// the other ones are which other keys must be held for this array to be true
	// terminated with 0x00000000
	//
	//shiftstates_for:
	// an array of uint*, one for each keycode (each keyboard counts separately)
	// not terminated, since it's random access; length is numkeyboards*keyboardlen
	//*shiftstates_for:
	// an array of uint, telling which keys must be in their requested state (as defined by *buttonrules) for anything to be active
};

static bool button_shiftstates(struct inputmapper * this_, unsigned int id, bool oneshot);
static bool button(struct inputmapper * this_, unsigned int id, bool oneshot);

static void reset_shiftstates(struct inputmapper_impl * this)
{
	if (this->shiftstates_for)
	{
		for (int i=0;i<this->keyboardlen*this->numkeyboards;i++) free(this->shiftstates_for[i]);
		free(this->shiftstates_for);
		this->shiftstates_for=NULL;
	}
	this->i.button=button_shiftstates;
}

static bool map_key(struct inputmapper * this_, const char * descriptor, unsigned int id)
{
	struct inputmapper_impl * this=(struct inputmapper_impl*)this_;
	
	if (id<this->numbuttons && this->buttonrules[id])
	{
		free(this->buttonrules[id]);
		this->buttonrules[id]=NULL;
	}
	
	if (!descriptor || !*descriptor) return true;
	
	unsigned int ** rules=parse_chain_descriptor(descriptor, this->libretro_to_keycode);
	if (!rules) return false;
	
	if (id>=this->numbuttons)
	{
		this->buttonrules=realloc(this->buttonrules, sizeof(unsigned int**)*(id+1));
		for (int i=this->numbuttons;i<id;i++)//not initializing the last one; we'll set it to non-NULL soon enough
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
	for (int kb=0;kb<this->numkeyboards;kb++)
	for (int key=0;key<this->keyboardlen;key++)
	{
		if (this->keyboardstate[kb*this->keyboardlen + key]==2)
		{
			int numkeys=1;
			int keybuflen=2;
			unsigned int * keys=malloc(sizeof(unsigned int)*keybuflen);
			unsigned int keyboardflags=kb<<11;
			
			keys[0]=keyboardflags | key;
			
			for (int key=0;key<this->keyboardlen;key++)
			{
				if (this->keyboardstate[kb*this->keyboardlen + key]==1)
				{
					keys[numkeys]=keyboardflags | key;
					numkeys++;
					
					if (numkeys==keybuflen)
					{
						keybuflen*=2;
						keys=realloc(keys, sizeof(unsigned int)*keybuflen);
					}
				}
			}
			keys[numkeys]=0x00000000;
			char * ret=create_descriptor(keys, this->keycode_to_libretro, this->keyboardlen);
			free(keys);
			return ret;
		}
	}
	return NULL;
}

static bool button_shiftstates(struct inputmapper * this_, unsigned int id, bool oneshot)
{
	struct inputmapper_impl * this=(struct inputmapper_impl*)this_;
	
	this->shiftstates_for=malloc(sizeof(unsigned int*)*this->keyboardlen*this->numkeyboards);
	memset(this->shiftstates_for, 0, sizeof(unsigned int*)*this->keyboardlen*this->numkeyboards);
	for (int i=0;i<this->numbuttons;i++)
	{
		unsigned int ** buttonrulechain=this->buttonrules[i];
		if (!buttonrulechain) continue;
		while (*buttonrulechain)
		{
			unsigned int * buttonrules=*buttonrulechain;
			unsigned int baserule=*buttonrules;
			
			unsigned int basekb=(baserule>>11)&31;
			unsigned int basekey=(baserule&0x3FF);
			unsigned int baseid=basekb*this->keyboardlen + basekey;
			
			buttonrules++;
			while (*buttonrules)
			{
				unsigned int rule=*buttonrules;
				
				if (this->shiftstates_for[baseid])
				{
					//check if this one is already listed
					unsigned int pos=0;
					while (this->shiftstates_for[baseid][pos])
					{
						//sort it in (looks like a funky insertion sort)
						unsigned int a=rule;
						unsigned int b=this->shiftstates_for[baseid][pos];
						if (a==b) goto skipthis;
						if (a>b)
						{
							rule=b;
							this->shiftstates_for[baseid][pos]=a;
						}
						pos++;
					}
					this->shiftstates_for[baseid]=realloc(this->shiftstates_for[baseid], sizeof(unsigned int)*(pos+2));
					this->shiftstates_for[baseid][pos]=rule;
					this->shiftstates_for[baseid][pos+1]=0x00000000;
				}
				else
				{
					this->shiftstates_for[baseid]=malloc(sizeof(unsigned int)*2);
					this->shiftstates_for[baseid][0]=rule;
					this->shiftstates_for[baseid][1]=0x00000000;
				}
				
			skipthis:;
				buttonrules++;
			}
			buttonrulechain++;
		}
	}
	
	this->i.button=button;
	return button(this_, id, oneshot);
}

static bool button_i(struct inputmapper_impl * this, unsigned int * buttons, bool oneshot)
{
	int type=buttons[0]>>28;
	switch (type)
	{
		case 0:
		{
			int kb=(buttons[0]>>11)&31;
			int key=(buttons[0]&0x3FF);
			
			if (kb>this->numkeyboards) return false;
			if (!(this->keyboardstate[kb*this->keyboardlen + key]&1)) return false;
			if (oneshot && this->keyboardstate[kb*this->keyboardlen + key]!=3) return false;
			
			unsigned int numenabledshiftstates=0;
			buttons++;
			while (*buttons)
			{
				if (!(this->keyboardstate[kb*this->keyboardlen + (*buttons&0x3FF)]&1)) return false;
				buttons++;
				numenabledshiftstates++;
			}
			
			unsigned int * shiftstates=this->shiftstates_for[kb*this->keyboardlen + key];
			if (shiftstates)
			{
				while (*shiftstates)
				{
					if (this->keyboardstate[kb*this->keyboardlen + (*shiftstates&0x3FF)]&1) numenabledshiftstates--;
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
	unsigned int ** buttons=this->buttonrules[id];
	
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
	
	unsigned int oldnkb=this->numkeyboards;
	unsigned int newnkb=this->in->keyboard_num_keyboards(this->in);
	if (newnkb==0) newnkb=1;
	
	if (oldnkb!=newnkb)
	{
		this->keyboardstate=realloc(this->keyboardstate, newnkb*this->keyboardlen);
		if (newnkb>oldnkb)
		{
			memset(this->keyboardstate + oldnkb*this->keyboardlen, 0, (newnkb-oldnkb)*this->keyboardlen);
		}
		reset_shiftstates(this);//this won't change them, but we need to resize the buffer
		this->numkeyboards=newnkb;
	}
	
	unsigned char newstate[this->keyboardlen];
	for (int i=0;i<this->numkeyboards;i++)
	{
		if (!this->in->keyboard_poll(this->in, i, newstate)) memset(newstate, 0, this->keyboardlen);
		
		for (int j=0;j<this->keyboardlen;j++)
		{
			bool old=(this->keyboardstate[this->keyboardlen*i + j]&1);
			bool new=newstate[j];
			this->keyboardstate[this->keyboardlen*i + j]=((old^new)<<1 | new);
		}
	}
}

static void clear(struct inputmapper * this_)
{
	struct inputmapper_impl * this=(struct inputmapper_impl*)this_;
	
	memset(this->keyboardstate, 0, this->numkeyboards*this->keyboardlen);
}

static void reset(struct inputmapper_impl * this)
{
	reset_shiftstates(this);
	
	free(this->keyboardstate);
	this->keyboardstate=NULL;
	
	if (this->buttonrules)
	{
		for (int i=0;i<this->numbuttons;i++) delete_chain_rule(this->buttonrules[i]);
		free(this->buttonrules);
		this->buttonrules=NULL;
		this->numbuttons=0;
	}
}

static void set_input(struct inputmapper * this_, struct inputraw * in)
{
	struct inputmapper_impl * this=(struct inputmapper_impl*)this_;
	
	reset(this);
	
	this->in=in;
	
	if (in)
	{
		this->numkeyboards=in->keyboard_num_keyboards(in) ?: 1;
		this->keyboardlen=in->keyboard_num_keys(in);
		this->keyboardstate=malloc(this->keyboardlen*this->numkeyboards);
		memset(this->keyboardstate, 0, this->keyboardlen*this->numkeyboards);
		
		in->keyboard_get_map(in, &this->keycode_to_libretro, &this->libretro_to_keycode);
	}
}

static void free_(struct inputmapper * this_)
{
	struct inputmapper_impl * this=(struct inputmapper_impl*)this_;
	reset(this);
	free(this);
}

struct inputmapper inputmapper_iface = {
	button, last, set_input, map_key, poll, clear, free_
};
struct inputmapper * inputmapper_create(struct inputraw * in)
{
	struct inputmapper_impl * this=malloc(sizeof(struct inputmapper_impl));
	memcpy(&this->i, &inputmapper_iface, sizeof(struct inputmapper));
	
	this->keyboardstate=NULL;
	
	this->numbuttons=0;
	this->buttonrules=NULL;
	this->shiftstates_for=NULL;
	
	set_input((struct inputmapper*)this, in);
	return (struct inputmapper*)this;
}
