#include "minir.h"

#ifdef DYLIB_POSIX
#undef malloc
#undef realloc
#undef free
#include <dlfcn.h>
#include <string.h>
static void* (*malloc_)(size_t);
static void* (*realloc_)(void*, size_t);
static void (*free_)(void*);
static void* (*dlopen_)(const char *, int);

struct memdebug {
	void (*malloc)(void* ptr, size_t size);
	void (*realloc)(void* prev, void* ptr, size_t size);
	void (*free)(void* prev);
};
static struct memdebug cb;
static unsigned int ignore;

void memdebug_init(struct memdebug * i)
{
	ignore=0;
	malloc_ = dlsym(RTLD_NEXT, "malloc");
	realloc_ = dlsym(RTLD_NEXT, "realloc");
	free_ = dlsym(RTLD_NEXT, "free");
	dlopen_ = dlsym(RTLD_NEXT, "dlopen");
	memcpy(&cb, i, sizeof(struct memdebug));
}

void* malloc(size_t size)
{
	void* ret=malloc_(size);
	if (ignore==0)
	{
		ignore++;
		cb.malloc(ret, size);
		ignore--;
	}
	return ret;
}

void* calloc(size_t n, size_t size)
{
	//just implement this one as malloc...
	void* ret=malloc(n*size);
	memset(ret, 0, n*size);
	return ret;
}

void* realloc(void* ptr, size_t size)
{
	void* ret=realloc_(ptr, size);
	if (ignore==0)
	{
		ignore++;
		cb.realloc(ptr, ret, size);
		ignore--;
	}
	return ret;
}

void free(void* ptr)
{
	if (ignore==0)
	{
		ignore++;
		cb.free(ptr);
		ignore--;
	}
	free_(ptr);
}

//void* dlopen(const char * filename, int flag)
//{
//	ignore++;
//	void* ret=dlopen_(filename, flag);
//	ignore--;
//	return ret;
//}
#endif
