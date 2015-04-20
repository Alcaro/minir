#pragma once
#include "intwrap.h"

//This one defines:
//Macros END_LITTLE, END_BIG and ENDIAN; ENDIAN is equal to one of the other two. The test is borrowed from byuu's nall.
//end_swap() - Byteswaps an integer.
//end_nat_to_le(), end_le_to_nat(), end_nat_to_be(), end_be_to_nat() - Byteswaps an integer or returns it unmodified, depending on the host endianness.
//Class litend<> and bigend<> - Acts like the given integer type, but is stored by the named endianness internally. Safe to memcpy() and fwrite().

#define END_LITTLE 0x04030201
#define END_BIG 0x01020304
#if (defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && __BYTE_ORDER == __LITTLE_ENDIAN) || defined(__LITTLE_ENDIAN__) || \
    defined(__i386__) || defined(__amd64__) || \
    defined(_M_IX86) || defined(_M_AMD64) || \
    defined(__ARM_EABI__) || defined(__arm__)
#define ENDIAN END_LITTLE
#elif (defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && __BYTE_ORDER == __BIG_ENDIAN) || defined(__BIG_ENDIAN__) || \
    defined(__powerpc__) || defined(_M_PPC)
#define ENDIAN END_BIG
#endif
//for crazy endians, just leave it undefined

#if defined(__GNUC__)
//This one is mostly useless (GCC detects the pattern and optimizes it).
//However, MSVC doesn't, so I need the intrinsics. Might as well use both sets.
static uint8_t end_swap(uint8_t n) { return n; }
static uint16_t end_swap(uint16_t n) { return __builtin_bswap16(n); }
static uint32_t end_swap(uint32_t n) { return __builtin_bswap32(n); }
static uint64_t end_swap(uint64_t n) { return __builtin_bswap64(n); }
#elif defined(_MSC_VER)
static uint8_t end_swap(uint8_t n) { return n; }
static uint16_t end_swap(uint16_t n) { return _byteswap_ushort(n); }
static uint32_t end_swap(uint32_t n) { return _byteswap_ulong(n); }
static uint64_t end_swap(uint64_t n) { return _byteswap_uint64(n); }
#else
static uint8_t end_swap(uint8_t n) { return n; }
static uint16_t end_swap(uint16_t n) { return n>>8 | n<<8; }
static uint32_t end_swap(uint32_t n)
{
	n = n>>16 | n<<16;
	n = (n&0x00FF00FF)<<8 | (n&0xFF00FF00)>>8;
	return n;
}
static uint64_t end_swap(uint64_t n)
{
	n = n>>32 | n<<32;
	n = (n&0x0000FFFF0000FFFF)<<16 | (n&0xFFFF0000FFFF0000)>>16;
	n = (n&0x00FF00FF00FF00FF)<<8  | (n&0xFF00FF00FF00FF00)>>8 ;
	return n;
}
#endif

#ifdef ENDIAN
#if ENDIAN == END_LITTLE
template<typename T> static T end_nat_to_le(T val) { return val; }
template<typename T> static T end_nat_to_be(T val) { return end_swap(val); }
template<typename T> static T end_le_to_nat(T val) { return val; }
template<typename T> static T end_be_to_nat(T val) { return end_swap(val); }
#else
template<typename T> static T end_nat_to_le(T val) { return end_swap(val); }
template<typename T> static T end_nat_to_be(T val) { return val; }
template<typename T> static T end_le_to_nat(T val) { return end_swap(val); }
template<typename T> static T end_be_to_nat(T val) { return val; }
#endif

template<typename T, bool little> class endian_core
{
	T val;
	
public:
	operator T()
	{
		if (little == (ENDIAN==END_LITTLE)) return val;
		else return end_swap(val);
	}
	
	void operator=(T newval)
	{
		if (little == (ENDIAN==END_LITTLE)) val = newval;
		else val = end_swap(newval);
	}
};

#else

//This one doesn't optimize properly. While it does get unrolled, it remains as four byte loads, and some shift/or.
template<typename T, bool little> class endian_core
{
	union {
		T align;
		uint8_t bytes[sizeof(T)];
	};
	
public:
	operator T()
	{
		if (little)
		{
			T ret=0;
			for (size_t i=0;i<sizeof(T);i++)
			{
				ret = (ret<<8) | bytes[i];
			}
			return ret;
		}
		else
		{
			T ret=0;
			for (size_t i=0;i<sizeof(T);i++)
			{
				ret = (ret<<8) | bytes[sizeof(T)-1-i];
			}
			return ret;
		}
	}
	
	void operator=(T newval)
	{
		if ((little && ENDIAN==END_LITTLE) || (!little && ENDIAN==END_BIG))
		{
			val = newval;
			return;
		}
		if (!little)
		{
			for (size_t i=0;i<sizeof(T);i++)
			{
				bytes[sizeof(T)-1-i]=(newval&0xFF);
				newval>>=8;
			}
		}
		else
		{
			for (size_t i=0;i<sizeof(T);i++)
			{
				bytes[i]=(newval&0xFF);
				newval>>=8;
			}
		}
	}
};
#endif

template<typename T> class bigend : public intwrap<endian_core<T, false>, T> {
public:
	bigend() {}
	bigend(T i) : intwrap<endian_core<T, false>, T>(i) {} // why does C++ need so much irritating cruft
};

template<typename T> class litend : public intwrap<endian_core<T, true>, T> {
public:
	litend() {}
	litend(T i) : intwrap<endian_core<T, true>, T>(i) {}
};
