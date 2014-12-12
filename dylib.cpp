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

ndylib* dylib_open(const char * filename, bool * owned)
{
	_int_mutex_lock(_imutex_dylib);
	ndylib* ret=NULL;
#ifdef DYLIB_POSIX
	if (owned)
	{
		ret=(ndylib*)dlopen(filename, RTLD_LAZY|RTLD_NOLOAD);
		*owned=(!ret);
		if (ret) return ret;
	}
	if (!ret) ret=(ndylib*)dlopen(filename, RTLD_LAZY);
#endif
#ifdef DYLIB_WIN32
	if (owned)
	{
		if (!GetModuleHandleEx(0, filename, &ret)) ret=NULL;
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
		
		HMODULE ret=LoadLibrary(filename);
		SetDllDirectory(NULL);
	}
#endif
	_int_mutex_unlock(_imutex_dylib);
	return ret;
}

void* dylib_sym_ptr(ndylib* lib, const char * name)
{
#ifdef DYLIB_POSIX
	return dlsym((void*)lib, name);
#endif
#ifdef DYLIB_WIN32
	return (void*)GetProcAddress((HMODULE)lib, name);
#endif
}

funcptr dylib_sym_func(ndylib* lib, const char * name)
{
#ifdef DYLIB_POSIX
	funcptr ret;
	*(void**)(&ret)=dlsym((void*)lib, name);
	return ret;
#endif
#ifdef DYLIB_WIN32
	return (funcptr)GetProcAddress((void*)lib, name);
#endif
}

void dylib_free(ndylib* lib)
{
#ifdef DYLIB_POSIX
	dlclose((void*)lib);
#endif
#ifdef DYLIB_WIN32
	FreeLibrary((HMODULE)lib);
#endif
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
