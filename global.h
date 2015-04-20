#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE //strdup, realpath, asprintf
#endif
#define _strdup strdup //and windows is being windows as usual
#define __STDC_LIMIT_MACROS //how many of these stupid things exist
#define __STDC_FORMAT_MACROS//if I include a header, it's because I want to use its contents
#define __STDC_CONSTANT_MACROS
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "host.h"
#include "function.h"

typedef void(*funcptr)();

//Note to anyone interested in reusing these objects:
//Many, if not most, of them will likely change their interface, likely quite fundamentally, in the future.
//No attempt is made to keep any kind of backwards or forwards compatibility.

//#include "function.h"

//#ifndef NO_ANON_UNION_STRUCT//For crappy compilers.
//Mandatory because there's a pile of unions in struct config.
#define UNION_BEGIN union {
#define UNION_END };
#define STRUCT_BEGIN struct {
#define STRUCT_END };
//#else
//#define UNION_BEGIN
//#define UNION_END
//#define STRUCT_BEGIN
//#define STRUCT_END
//#endif

#define JOIN_(x, y) x ## y
#define JOIN(x, y) JOIN_(x, y)

#define UNPACK_PAREN(...) __VA_ARGS__

//some magic stolen from http://blogs.msdn.com/b/the1/archive/2004/05/07/128242.aspx
//C++ can be so messy sometimes...
template<typename T, size_t N> char(&ARRAY_SIZE_CORE(T(&x)[N]))[N];
#define ARRAY_SIZE(x) (sizeof(ARRAY_SIZE_CORE(x)))

//requirements:
//- static_assert(false) throws something at compile time
//- multiple static_assert(true) works
//- does not require unique names for each assertion
//- zero traces left in the object files, except if debug info is enabled
//- zero warnings under any compiler
//- static_assert(2+2 < 5); works at the global scope
//- static_assert(2+2 < 5); works as a class member
//- static_assert(2+2 < 5); works inside a function
//- static_assert(2+2 < 5); works in all of the above when templates are involved
//- works on all compilers
//optional:
//- (FAILED) works if compiled as C
//- can name assertions, if desired
#ifdef __GNUC__
#define MAYBE_UNUSED __attribute__((__unused__)) // shut up, stupid warnings
#define TYPENAME_IF_GCC typename
#else
#define MAYBE_UNUSED
#define TYPENAME_IF_GCC
#endif
template<bool x> struct static_assert_t;
template<> struct static_assert_t<true> { struct STATIC_ASSERTION_FAILED {}; };
template<> struct static_assert_t<false> {};
//#define static_assert(expr)
//	typedef TYPENAME_IF_NEEDED static_assert_t<(bool)(expr)>::STATIC_ASSERTION_FAILED
//	JOIN(static_assertion_, __COUNTER__) MAYBE_UNUSED;
#define static_assert(expr) \
	enum { \
		JOIN(static_assertion_, __COUNTER__) = \
		sizeof(TYPENAME_IF_GCC static_assert_t<(bool)(expr)>::STATIC_ASSERTION_FAILED) \
	} MAYBE_UNUSED

#ifdef __GNUC__
#define ALIGN(n) __attribute__((aligned(n)))
#endif
#ifdef _MSC_VER
#define ALIGN(n) __declspec(align(n))
#endif




#ifdef __cplusplus
class anyptr {
void* data;
public:
template<typename T> anyptr(T* data_) { data=(void*)data_; }
template<typename T> operator T*() { return (T*)data; }
template<typename T> operator const T*() const { return (const T*)data; }
};
#else
typedef void* anyptr;
#endif


#include <stdlib.h> // needed because otherwise I get errors from malloc_check being redeclared.
anyptr malloc_check(size_t size);
anyptr try_malloc(size_t size);
#define malloc malloc_check
anyptr realloc_check(anyptr ptr, size_t size);
anyptr try_realloc(anyptr ptr, size_t size);
#define realloc realloc_check
anyptr calloc_check(size_t size, size_t count);
anyptr try_calloc(size_t size, size_t count);
#define calloc calloc_check


//if I cast it to void, that means I do not care, so shut the hell up about warn_unused_result.
template<typename T> static inline void ignore(T t) {}



//too reliant on non-ancient compilers
////some SFINAE shenanigans to call T::create if it exists, otherwise new() - took an eternity to google up
////don't use this template yourself, use generic_create/destroy instead
//template<typename T> class generic_create_core {
//	template<int G> class int_eater {};
//public:
//	template<typename T2> static T* create(T2*, int_eater<sizeof(&T2::create)>*) { return T::create(); }
//	static T* create(T*, ...) { return new T(); }
//	
//	template<typename T2> static void destroy(T* obj, T2*, int_eater<sizeof(&T2::release)>*) { obj->release(); }
//	static void destroy(T* obj, T*, ...) { delete obj; }
//};
//template<typename T> T* generic_create() { return generic_create_core<T>::create((T*)NULL, NULL); }
//template<typename T> void generic_delete(T* obj) { generic_create_core<T>::destroy(obj, (T*)NULL, NULL); }

template<typename T> T* generic_create() { return T::create(); }
template<typename T> T* generic_new() { return new T; }
template<typename T> void generic_delete(T* obj) { delete obj; }
template<typename T> void generic_release(T* obj) { obj->release(); }

template<typename T> void* generic_create_void() { return (void*)generic_create<T>(); }
template<typename T> void* generic_new_void() { return (void*)generic_new<T>(); }
template<typename T> void generic_delete_void(void* obj) { generic_delete((T*)obj); }
template<typename T> void generic_release_void(void* obj) { generic_release((T*)obj); }



class nocopy {
protected:
	nocopy() {}
	~nocopy() {}
//#ifdef HAVE_MOVE
//	nocopy(nocopy&&) = default;
//	const nocopy& operator=(nocopy&&) = default;
//#endif
private:
	nocopy(const nocopy&);
	const nocopy& operator=(const nocopy&);
};


/*
template<typename T> class autoptr : nocopy {
	T* obj;
#ifdef HAVE_MOVE
public:
	autoptr(T* obj) : obj(obj) {}
	autoptr(map&& other) : obj(other.obj) { other.obj=NULL; }
	~map() { delete obj; }
#else
	unsigned int* refcount;
public:
	autoptr(T* obj) : obj(obj)
	{
		this->refcount=new unsigned int;
		this->refcount[0]=1;
	}
	autoptr(const autoptr& other) : obj(other.obj)
	{
		this->refcount=other.refcount;
		this->refcount[0]++;
	}
	~autoptr()
	{
		this->refcount[0]--;
		if (this->refcount[0]==0)
		{
			delete this->refcount;
			delete this->obj;
		}
	}
#endif
	
	T& operator*() { return *obj; }
	T* operator->() { return obj; }
};
*/
#ifdef HAVE_MOVE
#define autoref nocopy
#else
template<typename T> class autoref {
	unsigned int* refcount;
public:
	autoref()
	{
		this->refcount=new unsigned int;
		this->refcount[0]=1;
	}
	autoref(const autoref& other)
	{
		this->refcount=other.refcount;
		this->refcount[0]++;
	}
	~autoref()
	{
		this->refcount[0]--;
		if (this->refcount[0]==0)
		{
			((T*)this) -> release();
		}
	}
};
#endif
template<typename T> class autoptr : autoref<T> {
	T* obj;
public:
	autoptr(T* obj) : obj(obj) {}
	void release() { delete obj; }
	
	T& operator*() { return *obj; }
	T* operator->() { return obj; }
};



#ifndef HAVE_ASPRINTF
void asprintf(char * * ptr, const char * fmt, ...);
#else
#define asprintf(...) ignore(asprintf(__VA_ARGS__))
#endif


//msvc:
//typedef unsigned long uint32_t;
//typedef unsigned __int64 uint64_t;
//typedef unsigned int size_t;
template<typename T> static inline T bitround(T in)
{
	in--;
	in|=in>>1;
	in|=in>>2;
	in|=in>>4;
	in|=in>>16;
	if (sizeof(T)>4) in|=in>>16>>16;
	in++;
	return in;
}


//If any interface defines a callback, free() is banned while inside this callback; other functions
// are allowed, unless otherwise specified. Other instances of the same interface may be used and
// freed, and other interfaces may be called.
//If an interface defines a function to set some state, and a callback for when this state changes,
// calling that function will not trigger the state callback.
//Unless otherwise specified, an interface may only be used from its owner thread (the creator).
// However, it is safe for any thread to create an interface, including having different threads
// use multiple instances of the same interface.
//Don't depend on any pointer being unique; for example, the None interfaces are static. However,
// even if they are (potentially) non-unique, following the instructed method to free them is safe;
// either they're owned by the one one giving them to you, or their free() handlers are empty, or
// they could even be refcounted.
//If a pointer is valid until anything from a set of functions is called (including if the set
// contains only one listed function), free() will also invalidate that pointer.
//"Implementation" means the implementation of the interfaces; the blocks of code that are called
// when a function is called on an interface.
//"User" means the one using the interfaces. Some interface implementations are users of other
// interfaces; for example, an implementation of libretro is also the user of a dylib.
//An implementation may, at its sole discretion, choose to define any implementation of undefined
// behaviour. After all, any result, including something well defined, is a valid interpretation of
// undefined behaviour. The user may, of course, not rely on that.
//Any function that starts with an underscore may only be called by the module that implements that
// function. ("Module" is defined as "anything whose compilation is controlled by the same #ifdef,
// or the file implementing an interface, whichever makes sense"; for example, window-win32-* is the
// same module.) The arguments and return values of these private functions may change meaning
// between modules, and the functions are not guaranteed to exist at all, or closely correspond to
// their name. For example, _window_init_misc on GTK+ instead initializes a component needed by the
// listboxes.

//This file, and many other parts of minir, uses a weird mix between Windows- and Linux-style
// filenames and paths. This is intentional; the author prefers Linux-style paths and directory
// structures, but Windows file extensions. .exe is less ambigous than no extension, and Windows'
// insistence on overloading the escape character is irritating. Since this excludes following
// any single OS, the rest is personal preference.

//This one doesn't really belong here, but it's used by both image.h and minir.h (and io.h, but image.h uses that).
//If minir.h grows a dependency on image.h or io.h, move this to image.h.
enum videoformat {
	//these three are same values and order as in libretro - do not change
	fmt_xrgb1555,
	fmt_xrgb8888,
	fmt_rgb565,
	
	//these are used only in minir
	fmt_none,//this should be 0, but libretro compatibility means I can't do that
	
	fmt_rgb888,
	fmt_argb1555,
	fmt_argb8888,
};
