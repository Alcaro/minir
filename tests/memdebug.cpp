#include "minir.h"

#ifdef DYLIB_POSIX
#undef malloc
#undef realloc
#undef free
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static void* (*malloc_)(size_t);
static void (*free_)(void*);
static void* (*realloc_)(void*, size_t);
static void* (*dlopen_)(const char *, int);
static int (*dlclose_)(void*);

struct memdebug {
	void (*malloc)(void* ptr, size_t size);
	void (*free)(void* prev);
	void (*realloc)(void* prev, void* ptr, size_t size);
	
	void* (*s_malloc)(size_t size);
	void (*s_free)(void* prev);
	void* (*s_realloc)(void* prev, size_t size);
};
static struct memdebug cb;
static unsigned int ignore;

//cb is chosen as a dummy address for zero-size allocations; we don't want them in the debugger main, could get nasty.
#define ZERO_SIZE_POINTER ((void*)&cb)

static void ignore_all()
{
	ignore++;
}

void memdebug_init(struct memdebug * i)
{
	ignore=0;
	malloc_ = (void*(*)(size_t))dlsym(RTLD_NEXT, "malloc");
	free_ = (void(*)(void*))dlsym(RTLD_NEXT, "free");
	realloc_ = (void*(*)(void*,size_t))dlsym(RTLD_NEXT, "realloc");
	dlopen_ = (void*(*)(const char*,int))dlsym(RTLD_NEXT, "dlopen");
	dlclose_ = (int(*)(void*))dlsym(RTLD_NEXT, "dlclose");
	cb.malloc=i->malloc;
	cb.free=i->free;
	cb.realloc=i->realloc;
	i->s_malloc=malloc_;
	i->s_free=free_;
	i->s_realloc=realloc_;
	
	atexit(ignore_all);
}

void* malloc(size_t size)
{
	if (!size) return ZERO_SIZE_POINTER;
	void* ret=malloc_(size);
	if (!ret) abort();
	if (ignore==0)
	{
		ignore++;
		cb.malloc(ret, size);
		ignore--;
	}
	return ret;
}

void free(void* ptr)
{
	if (!ptr) return;
	if (ptr==ZERO_SIZE_POINTER) return;
	if (ignore==0)
	{
		ignore++;
		cb.free(ptr);
		ignore--;
	}
	free_(ptr);
}

void* realloc(void* ptr, size_t size)
{
	if (!ptr) return malloc(size);
	if (!size)
	{
		free(ptr);
		return NULL;
	}
	
	void* ret=realloc_(ptr, size);
	if (!ret) abort();
	if (ignore==0)
	{
		ignore++;
		cb.realloc(ptr, ret, size);
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

struct dl_map {
	struct dl_map * next;
	void * ptr;
};

struct dl_list {
	struct dl_list * next;
	void * handle;
	struct dl_map * maps;
};

struct dl_list dl_head;
struct dl_list * dl_tail=&dl_head;

void* dlopen(const char * filename, int flag)
{
	ignore++;
	void* ret=dlopen_(filename, flag);
	if (ret)
	{
		dl_tail->next=(dl_list*)malloc(sizeof(struct dl_list));
		dl_tail=dl_tail->next;
		dl_tail->handle=ret;
		dl_tail->maps=(dl_map*)malloc(sizeof(struct dl_map));
		struct dl_map * map=dl_tail->maps;
		map->next=NULL;
		
//printf("dl=%s\n",filename);
		FILE * f=fopen("/proc/self/maps", "rt");
		char* lastend=NULL;
		while (!feof(f))
		{
			char data[1024];
			void* shutupgcc=fgets(data, 1024, f); (void)shutupgcc;
			bool thislib=false;
			if (strstr(data, filename)) thislib=true;//assume that no libraries with the same name can be loaded
			
			char* start;
			char* end;
			char mode[5];//rwxp
			sscanf(data, "%p-%p %s", &start, &end, mode);
			if (mode[1]=='w' && (thislib || (start==lastend && strstr(data, "00000000 00:00 0"))))//the ugly condition catches BSS sections
			{
				cb.malloc(start, end-start);
				
				map->next=(dl_map*)malloc(sizeof(struct dl_map));
				map=map->next;
				map->next=NULL;
				map->ptr=start;
			}
			if (thislib) lastend=end;
			else lastend=NULL;
		}
		fclose(f);
	}
	ignore--;
	return ret;
}

int dlclose(void* handle)
{
	ignore++;
	
	struct dl_list * list=&dl_head;
	while (list->next->handle != handle) list=list->next;
	struct dl_list * listnext=list->next;
	list->next=list->next->next;
	if (dl_tail==listnext) dl_tail=list;
	list=listnext;
	
	struct dl_map * map=list->maps->next;
	while (map)
	{
		struct dl_map * next=map->next;
		cb.free(map->ptr);
		free(map);
		map=next;
	}
	free(list->maps);
	free(list);
	
	dlclose_(handle);
	ignore--;
}
#endif

#ifdef DYLIB_WIN32
//I don't even think it's possible to override malloc under win32.
#endif
