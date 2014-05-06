#include "minir.h"
#include <stdlib.h>

#ifdef DYLIB_POSIX
#include <dlfcn.h>
#endif

#ifdef DYLIB_WIN32
#define _WIN32_WINNT 0x0501
#include <windows.h>

BOOL WINAPI SetDllDirectoryA(LPCSTR lpPathName);
BOOL WINAPI SetDllDirectoryW(LPCWSTR lpPathName);
#ifdef UNICODE
#define SetDllDirectory SetDllDirectoryW
#else
#define SetDllDirectory SetDllDirectoryA
#endif

#endif

struct dylib_impl {
	struct dylib i;
	
#ifdef DYLIB_POSIX
	void* lib;
#endif
#ifdef DYLIB_WIN32
	HMODULE lib;
#endif
	bool owned;
};

static bool void_true(struct dylib * this)
{
	return true;
}

static bool void_false(struct dylib * this)
{
	return false;
}

static void* sym_ptr(struct dylib * this_, const char * name)
{
	struct dylib_impl * this=(struct dylib_impl*)this_;
#ifdef DYLIB_POSIX
	return dlsym(this->lib, name);
#endif
#ifdef DYLIB_WIN32
	return (void*)GetProcAddress(this->lib, name);
#endif
}

static funcptr sym_func(struct dylib * this_, const char * name)
{
	struct dylib_impl * this=(struct dylib_impl*)this_;
#ifdef DYLIB_POSIX
	funcptr ret;
	*(void**)(&ret)=dlsym(this->lib, name);
	return ret;
#endif
#ifdef DYLIB_WIN32
	return (funcptr)GetProcAddress(this->lib, name);
#endif
}

static void free_(struct dylib * this_)
{
	struct dylib_impl * this=(struct dylib_impl*)this_;
#ifdef DYLIB_POSIX
	if (this->lib) dlclose(this->lib);
#endif
#ifdef DYLIB_WIN32
	if (this->lib) FreeLibrary(this->lib);
#endif
	free(this);
}

struct dylib * dylib_create(const char * filename)
{
	struct dylib_impl * this=malloc(sizeof(struct dylib_impl));
	this->i.sym_ptr=sym_ptr;
	this->i.sym_func=sym_func;
	this->i.free=free_;
	
#ifdef DYLIB_POSIX
	this->lib=dlopen(filename, RTLD_LAZY|RTLD_NOLOAD);
	this->i.owned=(this->lib ? void_false : void_true);
	if (!this->lib) this->lib=dlopen(filename, RTLD_LAZY);
#endif
#ifdef DYLIB_WIN32
	if (!GetModuleHandleEx(0, filename, &this->lib)) this->lib=NULL;
	this->i.owned=(this->lib ? void_false : void_true);
	
	if (!this->lib)
	{
		//this is so weird dependencies, for example winpthread-1.dll, can be placed beside the dll where they belong
		char * filename_copy=strdup(filename);
		char * filename_copy_slash=strrchr(filename_copy, '/');
		if (!filename_copy_slash) filename_copy_slash=strrchr(filename_copy, '\0');
		filename_copy_slash[0]='\0';
		SetDllDirectory(filename_copy);
		free(filename_copy);
		
		if (!this->lib) this->lib=LoadLibrary(filename);
		SetDllDirectory(NULL);
	}
#endif
	
	if (!this->lib) goto cancel;
	return (struct dylib*)this;
	
cancel:
	free_((struct dylib*)this);
	return NULL;
}

const char * dylib_ext()
{
#ifdef DYLIB_POSIX
	return ".so";
#endif
#ifdef DYLIB_WIN32
	return ".dll";
#endif
}
