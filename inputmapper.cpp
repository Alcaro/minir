#include "minir.h"
#include "libretro.h"
#include "io.h"
#include "containers.h"
#include <ctype.h>

namespace {
class devmgr_inputmapper_impl : public devmgr::inputmapper {
public:
//function<void(int id, bool down)> callback; // held in the parent class

//each button maps to two uint32, known as 'descriptor', which uniquely describes it
//format:
//trigger: ttttxxxx xxxxxxxx xxxxxxxx xxxxxxxx
//level:   yyyyyyyy yyyyyyyy yyyyyyyy yyyyyyyy
//tttt=type
// 0000=unknown, never happens
// 0001=keyboard
// 0010=mouse
// 0011=joypad
// others=TODO
//x=meaning varies between different device types
//y=meaning varies between different device types
//
//TODO: various input sources are not buttons, the following exist and may require changing this one:
//analog/discrete
//centering/positioned/unpositioned
// button (keyboard)
//  already solved
// self-centering analog (joystick)
//  maybe I should define some of the bits to be some kind of minimum value? The data structures are not prepared for that...
//  how will I solve issues with rounding errors? I don't want the device to fire events if it jitters across the limit.
//  do I use the padding in the keydata? That should be a good solution. There's no padding on 32bit, but 32bit is dying anyways.
//  events must contain the positions, I can't just turn them into booleans
// unpositioned analog (mouse, when grabbed)
//  fire an event each time it moves
//  merge events? probably not
// positioned analog (mouse pointer)
//  fire an event for each change
//  do I send delta or absolute position? probably absolute
// unpositioned discrete (mouse wheel)
//  make it fire press events, but never release and never be held? Fire release instantly?
//  wheels can be analog too; none are in practice, but MS docs say they can be
//
//for keyboard:
//0000---- -------- kkkkksnn nnnnnnnn
//-------- -------- -------- --------
//-=unused, always 0
//k=keyboard ID, 1-31, or 0 for any; if more than 31, loops back to 16
//s=is scancode flag
//n=if s is set, scancode, otherwise libretro code
//
//00000000 00000000 00000000 00000000 (RETROK_NONE on any keyboard) can never fire and may be used as special case
//
//none of those descriptors may leave this file

struct keydata {
	uint32_t trigger;//0 if the slot is unused
	uint32_t level;//used by some event sources
	multiint<uint32_t> mods;//must be held for this to fire
	keydata* next;//if multiple key combos are mapped to the same slot, this is non-NULL
	
	keydata() : trigger(0), level(0), next(NULL) {}
};

array<keydata> mappings;//the key is a slot ID
uint16_t firstempty;//any slot such that all previous slots are used

intmap<uint32_t, multiint<uint16_t> > keylist;//returns which slots are triggered by this descriptor

bool keymod_valid;
intmap<uint32_t, multiint<uint32_t> > keymod;//returns which descriptors are modifiers for this one

//intset<uint32_t> keyheld;

const char * const * keynames;

/*private*/ void keymod_regen()
{
	if (keymod_valid) return;
	keymod_valid = true;
	
	for (uint16_t i=0;i<mappings.len();i++)
	{
		keydata& key = mappings[i];
		uint32_t nummods;
		uint32_t* mods = key.mods.get(nummods);
		for (uint32_t j=0;j<nummods;j++)
		{
			keymod.get(key.trigger).add(mods[j]);
		}
	}
}

/*private*/ unsigned int parse_keyname(const char * name, const char * * nameend)
{
	size_t len;
	for (len=0;isalnum(name[len]);len++) {}
	if (nameend) *nameend = name+len;
	
	if (name[0]=='x')
	{
		//validate the input - we require only uppercase hex, with 0x banned
		for (size_t test=1;test<len;test++)
		{
			if (!isxdigit(name[test]) || islower(name[test])) return 0;
		}
		unsigned int keyid = strtoul(name+1, NULL, 16);
		if (keyid >= 0x400) return 0;
		return 0x400 | keyid;
	}
	else
	{
		for (unsigned int i=0;i<RETROK_LAST;i++)
		{
			if (keynames[i] && !strncmp(name, keynames[i], len) && !keynames[i][len]) return i;
		}
		return 0;
	}
}

/*private*/ keydata parse_descriptor_kb(const char * desc)
{
	unsigned int kbid;
	if (isdigit(*desc))
	{
		kbid = strtoul(desc, (char**)&desc, 10);
		if (kbid<=0 || kbid>=32) return keydata();
	}
	else kbid = 0;
	
	if (desc[0]!=':' || desc[1]!=':') return keydata();
	desc += 2;
	
	keydata ret;
	while (true)
	{
		uint32_t key = parse_keyname(desc, &desc);
		if (key==0) return keydata();
		key |= dev_kb<<28 | kbid<<11;
		if (*desc=='+')
		{
			ret.mods.add(key);
			desc++;
		}
		else if (*desc=='\0' || *desc==',')
		{
			ret.mods.remove(key);
			ret.trigger = key;
			break;
		}
		else return keydata();
	}
	return ret;
}

/*private*/ keydata parse_descriptor_single(const char * desc)
{
	if (desc[0]=='K' && desc[1]=='B' && !isalpha(desc[2])) return parse_descriptor_kb(desc+2);
	return keydata();
}

/*private*/ keydata parse_descriptor(const char * desc)
{
	const char * next = strchr(desc, ',');
	if (next)
	{
		keydata first = parse_descriptor_single(desc);
		next++;
		while (isspace(*next)) next++;
		keydata second = parse_descriptor_single(next);
		if (first.trigger && second.trigger)
		{
			first.next = malloc(sizeof(keydata));
			*first.next = second;
			return first;
		}
		else if (first.trigger) return first;
		else return second;
	}
	
	return parse_descriptor_single(desc);
}

/*private*/ template<typename K, typename V, typename K2, typename V2>
void multimap_remove(intmap<K, multiint<V> >& map, K2 key, V2 val)
{
	multiint<V>* items = map.get_ptr(key);
	if (!items) return;
	items->remove(val);
	if (items->count() == 0) map.remove(key);
}

/*private*/ void keydata_delete(uint16_t id)
{
	keydata* key = &mappings[id];
	multimap_remove(keylist, key->trigger, id);
	
	if (key->next)
	{
		key = key->next;
		while (key)
		{
			keydata* next = key->next;
			multimap_remove(keylist, key->trigger, id);
			key->~keydata();
			free(key);
			key = next;
		}
	}
}

/*private*/ void keydata_add(const keydata& key, uint16_t id)
{
	mappings[id] = key;
	
	const keydata* iter = &key;
	while (iter)
	{
		keylist.get(iter->trigger).add(id);
		iter = iter->next;
	}
}

bool register_button(unsigned int id, const char * desc)
{
	keymod.reset();
	keymod_valid = false;
	
	keydata_delete(id);
	if (desc)
	{
		keydata newkey = parse_descriptor(desc);
		if (!newkey.trigger) goto fail;
		keydata_add(newkey, id);
		return true;
	}
	//fall through to fail - it's not a failure, but the results are the same.
	
fail:
	firstempty = id;
	return false;
}

unsigned int register_group(unsigned int len)
{
	while (true)
	{
		unsigned int n=0;
		while (true)
		{
			if (mappings[firstempty+n].trigger != 0)
			{
				firstempty += n+1;
				break;
			}
			n++;
			if (n == len) return firstempty;
		}
	}
}

//enum dev_t {
//	dev_unknown,
//	dev_kb,
//	dev_mouse,
//	dev_gamepad,
//};
//enum { mb_left, mb_right, mb_middle, mb_x4, mb_x5 };
//type is an entry in the dev_ enum.
//device is which item of this type is relevant. If you have two keyboards, pressing A on both
// should give different values here. If they're distinguishable, use 1 and higher; if not, use 0.
//button is the 'common' ID for that device.
// For keyboard, it's a RETROK_* (not present here, include libretro.h). For mouse, it's the mb_* enum. For gamepads, [TODO]
//scancode is a hardware-dependent unique ID for that key. If a keyboard has two As, they will
// have different scancodes. If a key that doesn't map to any RETROK (Mute, for example), the
// common ID will be 0, and scancode will be something valid. Scancodes are still present for non-keyboards.
//down is the new state of the button. Duplicate events are fine and will be ignored.

/*private*/ uint32_t compile_trigger(dev_t type, unsigned int device, unsigned int button, unsigned int scancode)
{
	switch (type)
	{
		case dev_unknown: return 0;
		case dev_kb:
			if (device > 31) device = (device&15)|16;
			if (button) return dev_kb<<28 | device<<11 | 0<<10 | button<<0;
			else return dev_kb<<28 | device<<11 | 1<<10 | scancode<<0;
		case dev_mouse:
			return 0;
		case dev_gamepad:
			return 0;
	}
	return 0;
}

/*private*/ uint32_t trigger_to_global(uint32_t trigger)
{
	switch (trigger>>28)
	{
		case dev_unknown: return 0;
		case dev_kb:
			return (trigger&~0x0000F800);
		case dev_mouse:
			return 0;
		case dev_gamepad:
			return 0;
	}
	return 0;
}

/*private*/ void send_event(uint32_t trigger, bool down)
{
	uint16_t numslots;
	uint16_t* slots = keylist.get(trigger).get(numslots);
	for (uint16_t i=0;i<numslots;i++)
	{
		//TODO: check mods
		//TODO: block events that repeat already-known state
		callback(slots[i], down);
	}
}

void event(dev_t type, unsigned int device, unsigned int button, unsigned int scancode, bool down)
{
	uint32_t trigger = compile_trigger(type, device, button, scancode);
	send_event(trigger, down);
	send_event(trigger_to_global(trigger), down);
}

bool query(dev_t type, unsigned int device, unsigned int button, unsigned int scancode)
{
	
	return false;
}

void reset(dev_t type)
{
	
}

devmgr_inputmapper_impl()
{
	keynames = inputkb::keynames();
	keymod_valid = false;
}

};
}

devmgr::inputmapper* devmgr::inputmapper::create()
{
	return new devmgr_inputmapper_impl;
}
