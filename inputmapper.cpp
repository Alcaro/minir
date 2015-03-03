#include "minir.h"
#include "containers.h"

namespace {
class u32_u16_multimap {
	//the value is either:
	//- if (uintptr_t)val&1: this key contains exactly one slot: (uintptr_t)val >> 16
	//- else: pointer to a list of slots for this one; length is in val[-1]
	intmap<uint32_t, uint16_t*> data;
	
	static uint16_t* ptr_skip_low_u16(uint16_t* ptr)
	{
		const size_t numu16 = sizeof(uint16_t*) / sizeof(uint16_t);
		static_assert(numu16 > 1);
		
		union {
			uint16_t uint[numu16];
			void* ptr;
		} u;
		u.ptr = (void*)(uintptr_t)0xFFFF;
		
		if (u.uint[0]==0xFFFF) return ptr+1; // little endian
		else if (u.uint[numu16-1]==0xFF) return ptr; // big endian
		else return NULL; // middle endian - let's blow up (middle endian is dead, anyways)
	}
	
	uint16_t* encode(uint16_t val)
	{
		return (uint16_t*)(uintptr_t)(val<<16 | 0xFFFF);
	}
	
public:
	void add(uint32_t key, uint16_t val)
	{
		uint16_t*& valptr=data.get(key);
		if (!valptr)
		{
			valptr=encode(val);
			return;
		}
		if ((uintptr_t)valptr & 1)
		{
			if (valptr == encode(val)) return;
			
			//it's possible to shove multiple u16s into the extra bits of a 64bit
			// pointer, but that adds so much complexity there's no real point
			
			uint16_t prevval=((uintptr_t)valptr >> 16);
			valptr = (uint16_t*)malloc(sizeof(uint16_t)*3) + 1;
			valptr[-1]=2;
			valptr[0]=prevval;
			valptr[1]=val;
		}
		else
		{
			uint16_t numprev=valptr[-1];
			for (uint16_t i=0;i<numprev;i++)
			{
				if (valptr[i]==val) return;
			}
			valptr = (uint16_t*)realloc(valptr-1, sizeof(uint16_t)*(1+numprev+1)) + 1;
			valptr[-1]++;
			valptr[numprev]=val;
			//TODO: allocate more agressively? Probably not, nobody will map more than three-or-so things to the same key.
		}
	}
	
	void remove(uint32_t key, uint16_t val)
	{
		uint16_t** valptr=data.get_ptr(key);
		if (!valptr) return;
		if ((uintptr_t)*valptr & 1)
		{
			if (*valptr == encode(val)) data.remove(key);
			return;
		}
		else
		{
			uint16_t* vals=*valptr;
			uint16_t count=vals[-1];
			uint16_t i=0;
			while (true)
			{
				if (i==count) return;//not present
				if (vals[i]==val) break;
				i++;
			}
			
			vals[i]=vals[count-1];
			vals[-1]--;
			if (vals[-1]==1)
			{
				*valptr = encode(vals[1]);
				free(vals);
			}
			//TODO: reallocate? Probably not worth it, those objects are <16 in all sane contexts.
		}
	}
	
	//Returns a pointer to all keys. 'len' is the buffer length.
	//The returned pointer is valid until the next add() or remove().
	//The order is unspecified and can change at any moment.
	//If there is nothing at the specified key, the return value may or may not be NULL.
	uint16_t* get(uint32_t key, uint16_t& len)
	{
		uint16_t** valptr=data.get_ptr(key);
		if (!valptr)
		{
			len=0;
			return NULL;
		}
		
		uint16_t* val = *valptr;
		if ((uintptr_t)val & 1)
		{
			len=1;
			return ptr_skip_low_u16((uint16_t*)valptr);
		}
		else
		{
			len=val[-1];
			return val;
		}
	}
};

#ifdef SELFTEST
//cls & g++ -DSELFTEST -DNOMAIN nomain.cpp inputmapper.cpp memory.cpp -g & gdb a.exe
//this does not test that intmap works properly, only that this multimap does
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
	obj.remove(2, 1);
	obj.remove(2, 12);
	obj.remove(2, 123);
	obj.remove(2, 1234);
	obj.remove(2, 12345);
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
	
	struct keydata {
		static const size_t inlinesize = sizeof(uint32_t*)/sizeof(uint32_t);
		
		uint32_t trigger; // 0 if the slot is unused
		
		uint32_t nummod;
		union {
			uint32_t mod_inline[inlinesize];
			uint32_t* mod;
		};
	};
	
	array<keydata> mappings;//the key is a slot ID
	u32_u16_multimap keylist;//the key is a descriptor for the trigger key
	uint16_t firstempty;
	
	size_t register_group(size_t len)
	{
		while (true)
		{
			size_t n=0;
			while (mappings[firstempty+n].trigger == 0)
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
	
	bool register_button(size_t id, const char * desc)
	{
		keydata& key = mappings[id];
		if (!desc) goto fail;
goto fail;
		
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
