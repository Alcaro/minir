#include "minir.h"
#include "containers.h"
#include <ctype.h>

namespace {
class multiu16 {
	enum { numu16 = sizeof(uint16_t*) / sizeof(uint16_t) };
	
	void assertions() {
		static_assert(numu16 > 1);//there must be sufficient space for at least two ints in a pointer
		static_assert(numu16 * sizeof(uint16_t) == sizeof(uint16_t*));//the size of a pointer must be a multiple of the size of an int
		static_assert((numu16 & (numu16-1)) == 0);//this multiple must be a power of two
		static_assert(numu16<<1 < (uint16_t)-1);//the int must be large enough to store how many ints fit in a pointer, plus the tag bit
	};
	
	//the value is either:
	//if the lowest bit of ptr_raw is set:
	// the number of items is (ptr>>1) & numu16
	// the others are in inlines_raw[], at position inlines()
	//else:
	// the number of items is in ptr[0]
	// the items are in ptr[1..count]
	union {
		uint16_t* ptr_raw;
		uint16_t inlines_raw[numu16];
	};
	
	static int tag_offset()
	{
		union {
			uint16_t* ptr;
			uint16_t uint[numu16];
		} u;
		u.ptr = (uint16_t*)(uintptr_t)0xFFFF;
		
		if (u.uint[0]==0xFFFF) return 0; // little endian
		else if (u.uint[numu16-1]==0xFF) return numu16-1; // big endian
		else return -1; // middle endian - let's blow up (middle endian is dead, anyways)
	}
	
	uint16_t& tag()
	{
		return inlines_raw[tag_offset()];
	}
	
	bool is_inline()
	{
		return tag()&1;
	}
	
	uint16_t* inlines()
	{
		if (tag_offset()==0) return inlines_raw+1;
		else return inlines_raw;
	}
	
public:
	uint16_t* ptr()
	{
		if (is_inline()) return inlines();
		else return ptr_raw+1;
	}
	
	uint16_t count()
	{
		if (is_inline()) return tag()>>1;
		else return ptr_raw[0];
	}
	
private:
	//If increased, does not initialize the new entries. If decreased, drops the top.
	void set_count(uint16_t newcount)
	{
		uint16_t oldcount=count();
		uint16_t* oldptr=ptr();
		
		if (oldcount < numu16 && newcount < numu16)
		{
			tag() = newcount<<1 | 1;
		}
		if (oldcount >= numu16 && newcount < numu16)
		{
			uint16_t* freethis = ptr_raw;
			memcpy(inlines(), oldptr, sizeof(uint16_t)*newcount);
			tag() = newcount<<1 | 1;
			free(freethis);
		}
		if (oldcount < numu16 && newcount >= numu16)
		{
			uint16_t* newptr = malloc(sizeof(uint16_t)*(1+newcount));
			newptr[0] = newcount;
			memcpy(newptr+1, oldptr, sizeof(uint16_t)*oldcount);
			ptr_raw = newptr;
		}
		if (oldcount >= numu16 && newcount >= numu16)
		{
			ptr_raw = realloc(ptr_raw, sizeof(uint16_t)*(1+newcount));
			ptr_raw[0] = newcount;
		}
	}
	
public:
	multiu16()
	{
		tag() = 0<<1 | 1;
	}
	
	void add(uint16_t val)
	{
		uint16_t* entries = ptr();
		uint16_t num = count();
		
		for (uint16_t i=0;i<num;i++)
		{
			if (entries[i]==val) return;
		}
		
		add_uniq(val);
	}
	
	//Use this if the value is known to not exist in the set already.
	void add_uniq(uint16_t val)
	{
		uint16_t num = count();
		set_count(num+1);
		ptr()[num] = val;
	}
	
	void remove(uint16_t val)
	{
		uint16_t* entries = ptr();
		uint16_t num = count();
		
		for (uint16_t i=0;i<num;i++)
		{
			if (entries[i]==val)
			{
				entries[i] = entries[num-1];
				set_count(num-1);
				break;
			}
		}
	}
	
	uint16_t* get(uint16_t& len)
	{
		len=count();
		return ptr();
	}
	
	~multiu16()
	{
		if (!is_inline()) free(ptr_raw);
	}
};

class u32_u16_multimap {
	intmap<uint32_t, multiu16> data;
	
public:
	void add(uint32_t key, uint16_t val)
	{
		data.get(key).add(val);
	}
	
	void remove(uint32_t key, uint16_t val)
	{
		multiu16* core = data.get_ptr(key);
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
		multiu16* core = data.get_ptr(key);
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

#ifdef SELFTEST
//#ifdef OS_POSIX
//#include <valgrind/memcheck.h>
//#else
//#define VALGRIND_DO_LEAK_CHECK
//#endif
//cls & g++ -DSELFTEST -DNOMAIN nomain.cpp inputmapper.cpp memory.cpp -g & gdb a.exe
//g++ -DSELFTEST -DNOMAIN nomain.cpp inputmapper.cpp memory.cpp -g && gdb ./a.out
//this will test only this multimap; it assumes that the intmap is already functional
static void assert(bool cond)
{
	if (!cond) abort();
}
static void test() __attribute__((constructor));
static void test()
{
	u32_u16_multimap obj;
	uint16_t* ptr;
	uint16_t num;
	
	ptr = obj.get(0x12345678, num);
	assert(num == 0);
	
	obj.add(1, 0x123);
	
	ptr = obj.get(0, num);
	assert(num==0);
	ptr = obj.get(1, num);
	assert(num == 1);
	assert(ptr[0] == 0x123);
	
	obj.add(1, 0x123);
	ptr = obj.get(1, num);
	assert(num == 1);
	assert(ptr[0] == 0x123);
	
	obj.add(1, 123);
	ptr = obj.get(1, num);
	assert(num == 2);
	assert(ptr[0]+ptr[1] == 0x123+123);
	
	obj.add(1, 1234);
	ptr = obj.get(1, num);
	assert(num == 3);
	assert(ptr[0]+ptr[1]+ptr[2] == 0x123+123+1234);
	
	obj.add(0, 123);
	ptr = obj.get(0, num);
	assert(num == 1);
	assert(ptr[0] == 123);
	
	ptr = obj.get(1, num);
	assert(num == 3);
	assert(ptr[0]+ptr[1]+ptr[2] == 0x123+123+1234);
	
	obj.remove(1, 123);
	ptr = obj.get(1, num);
	assert(num == 2);
	assert(ptr[0]+ptr[1] == 0x123+1234);
	
	obj.remove(1, 123);
	ptr = obj.get(1, num);
	assert(num == 2);
	assert(ptr[0]+ptr[1] == 0x123+1234);
	
	obj.remove(1, 0x123);
	ptr = obj.get(1, num);
	assert(num == 1);
	assert(ptr[0] == 1234);
	
	obj.remove(1, 1234);
	ptr = obj.get(1, num);
	assert(num == 0);
	
	obj.add(1, 123);
	obj.add(1, 1234);
	obj.remove(1, 123);
	ptr = obj.get(1, num);
	assert(num == 1);
	assert(ptr[0]==1234);
	obj.remove(1, 1234);
	
	obj.add(1, 123);
	obj.add(1, 1234);
	obj.remove(1, 1234);
	ptr = obj.get(1, num);
	assert(num == 1);
	assert(ptr[0]==123);
	obj.remove(1, 123);
	
	obj.add(2, 1);
	obj.add(2, 12);
	obj.add(2, 123);
	obj.add(2, 1234);
	obj.add(2, 12345);
	obj.remove(2, 123);
	ptr = obj.get(2, num);
	assert(num == 4);
	assert(ptr[0]+ptr[1]+ptr[2]+ptr[3] == 1+12+1234+12345);
	obj.remove(2, 1);
	obj.remove(2, 12);
	obj.remove(2, 123);
	obj.remove(2, 1234);
	obj.remove(2, 12345);
	
	obj.add(2, 1);
	obj.add(2, 12);
	obj.add(2, 123);
	obj.add(2, 1234);
	obj.add(2, 12345);
	obj.remove(2, 12345);
	ptr = obj.get(2, num);
	assert(num == 4);
	assert(ptr[0]+ptr[1]+ptr[2]+ptr[3] == 1+12+123+1234);
	
	//leave it populated - leak check
	//VALGRIND_DO_LEAK_CHECK;
}
#endif

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
