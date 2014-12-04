#include "os.h"
#include <stdlib.h>

#ifdef DYLIB_POSIX
#include <dlfcn.h>
#endif

#ifdef DYLIB_WIN32
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0502//0x0501 excludes SetDllDirectory, so I need to put it at 0x0502
#undef bind
#include <windows.h>
#define bind BIND_CB
#endif

void* dylib::sym_ptr(const char * name)
{
#ifdef DYLIB_POSIX
	return dlsym(this->lib, name);
#endif
#ifdef DYLIB_WIN32
	return (void*)GetProcAddress((HMODULE)this->lib, name);
#endif
}

funcptr dylib::sym_func(const char * name)
{
#ifdef DYLIB_POSIX
	funcptr ret;
	*(void**)(&ret)=dlsym(this->lib, name);
	return ret;
#endif
#ifdef DYLIB_WIN32
	return (funcptr)GetProcAddress((HMODULE)this->lib, name);
#endif
}

dylib::~dylib()
{
#ifdef DYLIB_POSIX
	if (this->lib) dlclose(this->lib);
#endif
#ifdef DYLIB_WIN32
	if (this->lib) FreeLibrary((HMODULE)this->lib);
#endif
}

dylib::dylib(const char * filename)
{
#ifdef DYLIB_POSIX
	this->lib=dlopen(filename, RTLD_LAZY|RTLD_NOLOAD);
	this->owned_=(!this->lib);
	if (!this->lib) this->lib=dlopen(filename, RTLD_LAZY);
#endif
#ifdef DYLIB_WIN32
	if (!GetModuleHandleEx(0, filename, (HMODULE*)&this->lib)) this->lib=NULL;
	this->owned_=(!this->lib);
	
	if (!this->lib)
	{
		//this is so weird dependencies, for example winpthread-1.dll, can be placed beside the dll where they belong
		char * filename_copy=strdup(filename);
		char * filename_copy_slash=strrchr(filename_copy, '/');
		if (!filename_copy_slash) filename_copy_slash=strrchr(filename_copy, '\0');
		filename_copy_slash[0]='\0';
		SetDllDirectory(filename_copy);
		free(filename_copy);
		
		if (!this->lib) this->lib=(HMODULE)LoadLibrary(filename);
		SetDllDirectory(NULL);
	}
#endif
}

dylib* dylib_create(const char * filename)
{
	dylib* ret=new dylib(filename);
	if (!ret->lib)
	{
		delete ret;
		return NULL;
	}
	return ret;
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
