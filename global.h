#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE //strdup, realpath, asprintf
#endif
#define _strdup strdup //and windows is being windows as usual
#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "host.h"
#include "function.h"

typedef void(*funcptr)();

//Note to anyone interested in reusing these objects:
//Many, if not most, of them will likely change their interface, likely quite fundamentally, in the future.

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

#ifdef __cplusplus
 //template<bool cond> class static_assertion { private: enum { val=0 }; };
 //template<> class static_assertion<true> { public: enum { val=1 }; };
 //#define STATIC_ASSERT(cond, name) (void)(static_assertion<(cond)>::val)
 //#define STATIC_ASSERT_GSCOPE(cond, name) extern static_assertion<static_assertion<(cond)>::val> name
 #define STATIC_ASSERT(cond, name) extern char name[(cond)?1:-1]; (void)name
 #define STATIC_ASSERT_GSCOPE(cond, name) extern char name[(cond)?1:-1]
#else
 #define STATIC_ASSERT(cond, name) (void)(sizeof(struct { int:-!(cond); }))
 #define STATIC_ASSERT_GSCOPE(cond, name) extern char name[(cond)?1:-1]
#endif
#define STATIC_ASSERT_CAN_EVALUATE(cond, name) STATIC_ASSERT(sizeof(cond), name)
#define STATIC_ASSERT_GSCOPE_CAN_EVALUATE(cond, name) STATIC_ASSERT_GSCOPE(sizeof(cond), name)


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


class nocopy {
protected:
	nocopy() {}
	~nocopy() {}
private:
	nocopy(const nocopy&);
	const nocopy& operator=(const nocopy&);
};



#ifndef HAVE_ASPRINTF
void asprintf(char * * ptr, const char * fmt, ...);
#else
//if I cast it to void, that means I do not care, so shut the hell up about warn_unused_result.
static inline void shutupgcc(int x){}
#define asprintf(...) shutupgcc(asprintf(__VA_ARGS__))
#endif

//#include "window.h"
//#include "image.h"

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

class video;
enum videoformat {
	//these three are same as in libretro
	fmt_0rgb1555,
	fmt_xrgb8888,
	fmt_rgb565,
	
	//these are used only in minir
	fmt_none,//this should be 0, but libretro compatibility means I can't do that
	
	fmt_rgb888,
	fmt_argb1555,
	fmt_argb8888,
};
static inline uint8_t videofmt_byte_per_pixel(videoformat fmt)
{
	static const uint8_t table[]={2, 4, 2, 0, 3, 2, 4};
	return table[fmt];
}
