#include "minir.h"
#include "containers.h"
#include <ctype.h>

namespace {
class u32_u16_multimap {
	intmap<uint32_t, multiint<uint16_t> > data;
	
public:
	void add(uint32_t key, uint16_t val)
	{
		data.get(key).add(val);
	}
	
	void remove(uint32_t key, uint16_t val)
	{
		multiint<uint16_t>* core = data.get_ptr(key);
		if (!core) return;
		if (core->count() == 1) data.remove(key);
		else core->remove(val);
	}
	
	//Returns a pointer to all keys. 'len' is the buffer length.
	//The returned pointer is valid until the next add() or remove().
	//The order is unspecified and can change at any moment.
	//If there is nothing at the specified key, the return value may or may not be NULL.
	uint16_t* get(uint32_t key, uint16_t& len)
	{
		multiint<uint16_t>* core = data.get_ptr(key);
		if (core)
		{
			return core->get(len);
		}
		else
		{
			len = 0;
			return NULL;
		}
	}
};

class devmgr_inputmapper_impl : public devmgr::inputmapper {
	//function<void(size_t id, bool down)> callback; // held in the parent class
	
	//each button maps to an uint32, known as 'key descriptor', which uniquely describes it
	//format:
	//ttttxxxx xxxxxxxx xxxxxxxx xxxxxxxx
	//tttt=type
	// 0000=keyboard
	// others=TODO
	//x=meaning varies between different device types
	//
	//for keyboard:
	//0000---- -------- kkkkksnn nnnnnnnn
	//-=unused, always 0
	//k=keyboard ID, 1-31, or 0 for any; if more than 31, loops back to 16
	//s=is scancode flag
	//n=if s is set, scancode, otherwise libretro code
	//
	//00000000 00000000 00000000 00000000 (RETROK_NONE on any keyboard) is impossible and may be used as special case
	//
	//none of those descriptors may leave this file
	
	class keydata {
	public:
		uint32_t trigger; // 0 if the slot is unused
		
		uint16_t nummod;
		//char padding[2];
		
	private:
		static const size_t inlinesize = sizeof(uint32_t*)/sizeof(uint32_t);
		
		union {
			uint32_t mod_inline[inlinesize];
			uint32_t* mod_ptr;
		};
		
	public:
		keydata() { nummod = 0; }
		
		uint32_t* resize(uint16_t count)
		{
			if (nummod > inlinesize) free(mod_ptr);
			
			nummod = count;
			if (nummod > inlinesize)
			{
				mod_ptr = malloc(sizeof(uint32_t)*count);
				return mod_ptr;
			}
			else return mod_inline;
		}
		
		const uint32_t* mods()
		{
			if (nummod <= inlinesize) return mod_inline;
			else return mod_ptr;
		}
		
		~keydata()
		{
			if (nummod > inlinesize) free(mod_ptr);
		}
	};
	
	array<keydata> mappings;//the key is a slot ID
	uint16_t firstempty;//any slot such that all previous slots are used
	u32_u16_multimap keylist;//the key is a descriptor for the trigger key; it returns a slot ID
	
	u32_u16_multimap modsfor;//the key is a descriptor for the trigger key; it returns a slot ID
	
	size_t register_group(size_t len)
	{
		while (true)
		{
			size_t n=0;
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
	
	/*private*/ bool parse_descriptor_kb(keydata& key, const char * desc)
	{
return false;
	}
	
	/*private*/ bool parse_descriptor(keydata& key, const char * desc)
	{
		if (desc[0]=='K' && desc[1]=='B' && !isalpha(desc[2])) return parse_descriptor_kb(key, desc);
		return false;
	}
	
	bool register_button(size_t id, const char * desc)
	{
		keydata& key = mappings[id];
		keylist.remove(key.trigger, id);
		if (!desc) goto fail; // not really fail, but the results are the same.
		
		if (!parse_descriptor(key, desc)) goto fail;
		keylist.add(key.trigger, id);
		return true;
		
	fail:
		key.trigger=0;
		firstempty=id;
		return false;
	}
	
	//enum dev_t {
	//	dev_kb,
	//	dev_mouse,
	//	dev_gamepad,
	//};
	//enum { mb_left, mb_right, mb_middle, mb_x4, mb_x5 };
	//type is an entry in the dev_ enum.
	//device is which item of this type is relevant. If you have two keyboards, pressing A on both
	// would give different values here.
	//button is the 'common' ID for that device.
	// For keyboard, it's a RETROK_*. For mouse, it's the mb_* enum. For gamepads, [TODO]
	//scancode is a hardware-dependent unique ID for that key. If a keyboard has two As, they will
	// have different scancodes. If a key that doesn't map to any RETROK (Mute, for example), the common
	// ID will be some NULL value (RETROK_NONE, for example), and scancode will be something valid.
	//down is the new state of the button. Duplicate events are fine and will be ignored.
	void event(dev_t type, unsigned int device, unsigned int button, unsigned int scancode, bool down)
	{
		
	}
	
	bool query(dev_t type, unsigned int device, unsigned int button, unsigned int scancode)
	{
		
		return false;
	}
	
	void reset(dev_t type)
	{
		
	}
	
	//~devmgr_inputmapper_impl()
	//{
	//	
	//}
};
}

devmgr::inputmapper* devmgr::inputmapper::create()
{
	return new devmgr_inputmapper_impl;
}
