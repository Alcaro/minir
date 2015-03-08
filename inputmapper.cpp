#include "minir.h"
#include "containers.h"
#include <ctype.h>

namespace {
template<typename T> class multinum {
	enum { numinline = sizeof(T*) / sizeof(T) };
	
	void assertions() {
		static_assert(numinline > 1);//there must be sufficient space for at least two ints in a pointer
		static_assert(numinline * sizeof(T) == sizeof(T*));//the size of a pointer must be a multiple of the size of an int
		static_assert((numinline & (numinline-1)) == 0);//this multiple must be a power of two
		static_assert(numinline<<1 < (T)-1);//the int must be large enough to store how many ints fit in a pointer, plus the tag bit
	};
	
	//the value is either:
	//if the lowest bit of ptr_raw is set:
	// the number of items is (ptr>>1) & numinline
	// the others are in inlines_raw[], at position inlines()
	//else:
	// the number of items is in ptr[0]
	// the items are in ptr[1..count]
	union {
		T* ptr_raw;
		T inlines_raw[numinline];
	};
	
	static int tag_offset()
	{
		union {
			T* ptr;
			T uint[numinline];
		} u;
		u.ptr = (T*)(uintptr_t)0xFFFF;
		
		if (u.uint[0]==0xFFFF) return 0; // little endian
		else if (u.uint[numinline-1]==0xFF) return numinline-1; // big endian
		else return -1; // middle endian - let's blow up (middle endian is dead, anyways)
	}
	
	T& tag()
	{
		return inlines_raw[tag_offset()];
	}
	
	bool is_inline()
	{
		return tag()&1;
	}
	
	T* inlines()
	{
		if (tag_offset()==0) return inlines_raw+1;
		else return inlines_raw;
	}
	
public:
	T* ptr()
	{
		if (is_inline()) return inlines();
		else return ptr_raw+1;
	}
	
	T count()
	{
		if (is_inline()) return tag()>>1;
		else return ptr_raw[0];
	}
	
private:
	//If increased, does not initialize the new entries. If decreased, drops the top.
	void set_count(T newcount)
	{
		T oldcount=count();
		T* oldptr=ptr();
		
		if (oldcount < numinline && newcount < numinline)
		{
			tag() = newcount<<1 | 1;
		}
		if (oldcount >= numinline && newcount < numinline)
		{
			T* freethis = ptr_raw;
			memcpy(inlines(), oldptr, sizeof(T)*newcount);
			tag() = newcount<<1 | 1;
			free(freethis);
		}
		if (oldcount < numinline && newcount >= numinline)
		{
			T* newptr = malloc(sizeof(T)*(1+newcount));
			newptr[0] = newcount;
			memcpy(newptr+1, oldptr, sizeof(T)*oldcount);
			ptr_raw = newptr;
		}
		if (oldcount >= numinline && newcount >= numinline)
		{
			ptr_raw = realloc(ptr_raw, sizeof(T)*(1+newcount));
			ptr_raw[0] = newcount;
		}
	}
	
public:
	multinum()
	{
		tag() = 0<<1 | 1;
	}
	
	void add(T val)
	{
		T* entries = ptr();
		T num = count();
		
		for (T i=0;i<num;i++)
		{
			if (entries[i]==val) return;
		}
		
		add_uniq(val);
	}
	
	//Use this if the value is known to not exist in the set already.
	void add_uniq(T val)
	{
		T num = count();
		set_count(num+1);
		ptr()[num] = val;
	}
	
	void remove(T val)
	{
		T* entries = ptr();
		T num = count();
		
		for (T i=0;i<num;i++)
		{
			if (entries[i]==val)
			{
				entries[i] = entries[num-1];
				set_count(num-1);
				break;
			}
		}
	}
	
	T* get(T& len)
	{
		len=count();
		return ptr();
	}
	
	~multinum()
	{
		if (!is_inline()) free(ptr_raw);
	}
};

class u32_u16_multimap {
	intmap<uint32_t, multinum<uint16_t> > data;
	
public:
	void add(uint32_t key, uint16_t val)
	{
		data.get(key).add(val);
	}
	
	void remove(uint32_t key, uint16_t val)
	{
		multinum<uint16_t>* core = data.get_ptr(key);
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
		multinum<uint16_t>* core = data.get_ptr(key);
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
