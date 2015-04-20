#include "minir.h"
#include "libretro.h"
#include "io.h"
#include "containers.h"
#include <ctype.h>

namespace {
class minir_inputmapper_impl : public minir::inputmapper {
public:
//function<void(int id, bool down)> callback; // held in the parent class

//each button maps to two uint32, known as 'descriptor', which uniquely describes it
//format: ttttnnnn nnnnnnnn nnnnnnll llllllll
//tttt=type
// 0000=unknown, never happens and may be used for special purposes
// 0001=keyboard
// 0010=mouse
// 0011=gamepad
// others=TODO
//n=uint18; must be exactly this to fire
//l=uint10; must be at >= this to fire (masked off to 0 when used as key to the intmaps)
//[TODO: actually do as promised with the Ls]
//none of those descriptors may leave this file
//
//each type has their own rules for how events are mapped to descriptors
//keyboard:
// n = 00kk kkkscccc cccccc
// k=keyboard ID, 1-31, or 0 for any; if more than 31, loops back to 16
// s=is scancode flag
// c=if s is set, scancode, otherwise libretro code
//others: TODO
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
//maybe I should make some of the events bypass this structure entirely?

typedef uint32_t keydesc_t;
typedef uint16_t keylevel_t;
typedef uint16_t keyslot_t;

typedef keydesc_t n_keydesc_t;
typedef keyslot_t n_keyslot_t;

struct keydata {
	multiint<keydesc_t> keys;//must be held for this to fire
	keydesc_t primary;
	keyslot_t level;//used by some event sources; 0 if the slot is unused
	bool active;//whether this one is set (only valid for entries in 'mappings', not child items)
	bool trigger;//whether this one is trigger-only (false: level-based)
	keydata* next;//if multiple key combos are mapped to the same slot, this is non-NULL
	
	keydata() : primary(0), level(0), active(false), next(NULL) {}
};

class kb {
	kb(){} // cannot instantiate this one
	
	private: static keydesc_t compile_key(unsigned int button, unsigned int scancode)
	{
		if (button) return (0<<10 | button<<0);
		else return (1<<10 | scancode<<0);
	}
	
	private: static keydesc_t compile(unsigned int device, unsigned int key)
	{
		//the device is known to be in the correct range already
		return dev_kb<<28 | (device<<11|key)<<10;
	}
	
	public: static keydesc_t compile(unsigned int device, unsigned int button, unsigned int scancode)
	{
		if (device > 31) device = (device&15)|16;
		return compile(device, compile_key(button, scancode));
	}
	
	public: static keydesc_t global(keydesc_t desc)
	{
		return desc&~(31 << 21);
	}
	
	
	private: static unsigned int parse_keyname(const char * name, const char * * nameend)
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
			return compile_key(0, keyid);
		}
		else
		{
			const char * const * keynames = inputkb::keynames();
			for (unsigned int i=0;i<RETROK_LAST;i++)
			{
				if (keynames[i] && !strncmp(name, keynames[i], len) && !keynames[i][len])
				{
					return compile_key(i, 0);
				}
			}
			return 0;
		}
	}
	
	public: static keydata parse(const char * desc)
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
			keydesc_t keyid = parse_keyname(desc, &desc);
			if (keyid==0) return keydata();
			keydesc_t key = compile(kbid, keyid);
			
			if (*desc=='+')
			{
				ret.keys.add(key);
				desc++;
			}
			else if (*desc=='\0' || *desc==',')
			{
				ret.keys.add(key);
				ret.keys.sort();
				ret.primary = key;
				ret.level = 1;
				return ret;
			}
			else return keydata();
		}
	}
};


class bydev {
	bydev(){} // cannot instantiate this one
	
	public: static keydesc_t compile(dev_t type, unsigned int device, unsigned int button, unsigned int scancode)
	{
		switch (type)
		{
			case dev_unknown: return 0;
			case dev_kb: return kb::compile(device, button, scancode);
			case dev_mouse: return 0;
			case dev_gamepad: return 0;
			default: return 0;
		}
	}
	
	public: static keydesc_t global(keydesc_t desc)
	{
		switch (desc>>28)
		{
			case dev_unknown: return 0;
			case dev_kb: return kb::global(desc);
			case dev_mouse: return 0;
			case dev_gamepad: return 0;
			default: return 0;
		}
	}
	
	private: static keydata parse_single(const char * desc)
	{
		if (desc[0]=='K' && desc[1]=='B') return kb::parse(desc+2);
		return keydata();
	}
	
	public: static keydata parse(const char * desc)
	{
		const char * next = strchr(desc, ',');
		if (next)
		{
			keydata first = parse_single(desc);
			next++;
			while (isspace(*next)) next++;
			keydata second = parse(next);
			if (first.level && second.level)
			{
				first.next = malloc(sizeof(keydata));
				new(first.next) keydata(second);
				return first;
			}
			else if (first.level) return first;
			else return second;
		}
		
		return parse_single(desc);
	}
};


array<keydata> mappings;//the key is a slot ID
keyslot_t firstempty;//any slot such that all previous slots are used

intmap<keydesc_t, multiint<keyslot_t> > keylist;//returns which slots are affected by this descriptor

bool keymod_valid;
intmap<keydesc_t, multiint<keydesc_t> > keymod;//returns which descriptors are modifiers for this one

intmap<keydesc_t, uint8_t> keyheld;

/*private*/ void keymod_regen()
{
	if (keymod_valid) return;
	keymod_valid = true;
	
	for (n_keyslot_t i=0;i<mappings.len();i++)
	{
		keydata* key = &mappings[i];
		while (key)
		{
			keydesc_t numkeys;
			keydesc_t* keys = key->keys.get(numkeys);
			for (keydesc_t j=0;j<numkeys;j++)
			{
				keymod.get(key->primary).add(keys[j]);
			}
			key = key->next;
		}
	}
}

/*private*/ template<typename K, typename V, typename K2, typename V2>
void multimap_remove(intmap<K, multiint<V> >& map, K2 key, V2 val)
{
	multiint<V>* items = map.get_ptr(key);
	if (!items) return;
	items->remove(val);
	if (items->count() == 0) map.remove(key);
}

/*private*/ void keydata_add(keydata& key, keyslot_t id)
{
	mappings[id] = key;
	
	keydata* iter = &key;
	while (iter)
	{
		n_keydesc_t numkeys;
		keydesc_t* keys = iter->keys.get(numkeys);
		for (n_keydesc_t i=0;i<numkeys;i++) keylist.get(keys[i]).add(id);
		
		iter = iter->next;
	}
}

/*private*/ void keydata_delete(keyslot_t id)
{
	keydata* key = &mappings[id];
	bool first = true;
	
	while (key)
	{
		keydata* next = key->next;
		
		keydesc_t numkeys;
		keydesc_t* keys = key->keys.get(numkeys);
		for (keydesc_t i=0;i<numkeys;i++) multimap_remove(keylist, keys[i], id);
		
		//keydata_delete_single(key, id);
		if (first)
		{
			key->level = 0;
			first=false;
		}
		else
		{
			key->~keydata();
			free(key);
		}
		key = next;
	}
	
	if (id < firstempty) firstempty = id;
}

bool register_button(unsigned int id, const char * desc, bool trigger)
{
	keymod.reset();
	keymod_valid = false;
	
	keydata_delete(id);
	if (desc)
	{
		keydata newkey = bydev::parse(desc);
		if (!newkey.level) goto fail;
		newkey.trigger = trigger;
		keydata_add(newkey, id);
		return true;
	}
	//fall through to fail - it's not a failure, but the results are the same.
	
fail:
	if (id < firstempty) firstempty = id;
	return false;
}

unsigned int register_group(unsigned int len)
{
	while (true)
	{
		unsigned int n=0;
		while (true)
		{
			if (mappings[firstempty+n].level != 0)
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

/*private*/ n_keydesc_t num_held_in_set(multiint<keydesc_t>* set)
{
	if (!set) return 0;
	
	n_keydesc_t count = 0;
	
	n_keydesc_t len;
	keydesc_t* items = set->get(len);
	for (n_keydesc_t i=0;i<len;i++)
	{
		if (keyheld.get_or(items[i], false)) count++; // do not just add, some elements have values outside [0,1]
	}
	return count;
}

/*private*/ bool slot_active_single(keydata* data, keydesc_t primary)
{
	keymod_regen();
	
	multiint<keydesc_t>* modsforthis = &data->keys;
	multiint<keydesc_t>* modsfor = keymod.get_ptr(data->primary);
	
	//we want to know if every element in modsforthis is held, but no other in modsfor
	//since every element in modsforthis is in modsfor, these checks are sufficient
	//they don't tell which keys have wrong state, but we're not interested either.
	
	if (modsforthis->count() == num_held_in_set(modsforthis) &&
	    modsforthis->count() == num_held_in_set(modsfor) && 
	    (!primary || data->primary == primary))
	{
		return true;
	}
	else return false;
}

/*private*/ bool slot_active(keyslot_t slot, keydesc_t primary)
{
	keydata* data = &mappings[slot];
	while (data)
	{
		if (slot_active_single(data, primary)) return true;
		data = data->next;
	}
	return false;
}

/*private*/ void send_event(keydesc_t trigger, bool down)
{
	n_keyslot_t numslots;
	keyslot_t* slots = keylist.get(trigger).get(numslots);
printf("%.8X try %i\n",trigger,numslots);
	
	for (n_keyslot_t i=0;i<numslots;i++)
	{
		keydata& keydat = mappings[slots[i]];
		bool down = slot_active(slots[i], keydat.trigger ? trigger : 0);
		
		if (keydat.trigger)
		{
			if (down) callback(slots[i], true);
		}
		else
		{
			if (down != keydat.active) callback(slots[i], down);
			keydat.active = down;
		}
	}
}

void event(dev_t type, unsigned int device, unsigned int button, unsigned int scancode, bool down)
{
	keydesc_t trigger = bydev::compile(type, device, button, scancode);
	keydesc_t gtrigger = bydev::global(trigger);
	uint8_t& state = keyheld.get(trigger);
	if ((bool)state == down) return;
	
	state = down;
	if (trigger != gtrigger)
		keyheld.get(gtrigger) += (down ? 1 : -1);
	
	send_event(trigger, down);
	if (trigger != gtrigger)
		send_event(gtrigger, down);
}

bool query(dev_t type, unsigned int device, unsigned int button, unsigned int scancode)
{
	keydesc_t trigger = bydev::compile(type, device, button, scancode);
	return keyheld.get_or(trigger, false);
}

bool query_slot(unsigned int slot)
{
	return slot_active(slot, 0);
}

void reset(dev_t type)
{
	//TODO: this makes a bunch of keydata.active states inconsistent with keyheld
	if (type == dev_unknown)
	{
		keyheld.reset();
	}
	else
	{
		keyheld.remove_if(bind_ptr(&minir_inputmapper_impl::reset_cond, (void*)(uintptr_t)type));
	}
}

/*private*/ static bool reset_cond(void* type, keydesc_t key, uint8_t& value)
{
	return (key>>28 == (uintptr_t)type);
}

minir_inputmapper_impl()
{
	keymod_valid = false;
	firstempty = 0;
}

};
}

minir::inputmapper* minir::inputmapper::create()
{
	return new minir_inputmapper_impl;
}


//typedef unsigned int triggertype;
//enum {
//	tr_press = 1,   // The slot is now held, and wasn't before.
//	tr_release = 2, // The slot is now released, and wasn't before.
//	tr_primary = 4, // The primary key for this slot was pressed.
//	//Only a few of the combinations (1, 2, 4, 5) are possible.
//};
