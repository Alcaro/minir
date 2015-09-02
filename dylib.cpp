#include "os.h"
#include <stdlib.h>

#ifdef DYLIB_POSIX
#include <dlfcn.h>
#endif

#ifdef DYLIB_WIN32
#undef bind
#include <windows.h>
#define bind bind_func
#endif

static mutex dylib_lock;

dylib* dylib::create(const char * filename, bool * owned)
{
	dylib_lock.lock();
	dylib* ret=NULL;
#ifdef DYLIB_POSIX
	if (owned)
	{
		ret=(dylib*)dlopen(filename, RTLD_LAZY|RTLD_NOLOAD);
		*owned=(!ret);
		if (ret) return ret;
	}
	if (!ret) ret=(dylib*)dlopen(filename, RTLD_LAZY);
#endif
#ifdef DYLIB_WIN32
	if (owned)
	{
		if (!GetModuleHandleEx(0, filename, (HMODULE*)&ret)) ret=NULL;
		*owned=(!ret);
	}
	
	if (!ret)
	{
		//this is so weird dependencies, for example winpthread-1.dll, can be placed beside the dll where they belong
		char * filename_copy=strdup(filename);
		char * filename_copy_slash=strrchr(filename_copy, '/');
		if (!filename_copy_slash) filename_copy_slash=strrchr(filename_copy, '\0');
		filename_copy_slash[0]='\0';
		SetDllDirectory(filename_copy);
		free(filename_copy);
		
		ret=(dylib*)LoadLibrary(filename);
		SetDllDirectory(NULL);
	}
#endif
	dylib_lock.unlock();
	return ret;
}

void* dylib::sym_ptr(const char * name)
{
#ifdef DYLIB_POSIX
	return dlsym((void*)this, name);
#endif
#ifdef DYLIB_WIN32
	return (void*)GetProcAddress((HMODULE)this, name);
#endif
}

funcptr dylib::sym_func(const char * name)
{
#ifdef DYLIB_POSIX
	funcptr ret;
	*(void**)(&ret)=dlsym((void*)this, name);
	return ret;
#endif
#ifdef DYLIB_WIN32
	return (funcptr)GetProcAddress((HMODULE)this, name);
#endif
}

void dylib::release()
{
#ifdef DYLIB_POSIX
	dlclose((void*)this);
#endif
#ifdef DYLIB_WIN32
	FreeLibrary((HMODULE)this);
#endif
}
