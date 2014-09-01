#include "minir.h"

//Not in the other window-* because GTK does not demand Linux, and vice versa.
#if 0
#error
#elif defined(FILEPATH_POSIX)
#include <unistd.h>
//#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

const char * window_get_proc_path()
{
	//we could lstat it, but apparently that just returns zero on /proc on Linux.
	
	ssize_t bufsize=64;
	static char * linkname=NULL;
	if (linkname) return linkname;
	
	while (true)
	{
		linkname=malloc(bufsize);
		ssize_t r=readlink("/proc/self/exe", linkname, bufsize);
		if (r<0 || r>=bufsize)
		{
			free(linkname);
			if (r<0) return NULL;
			
			bufsize*=2;
			continue;
		}
		linkname[r]='\0';
		char * end=strrchr(linkname, '/');
		if (end) *end='\0';
		
		return linkname;
	}
}

//const char * window_get_cur_path()
//{
//	static char * path=NULL;
//	if (path) return path;
//	path=getcwd(NULL, 0);
//	if (path) return path;
//	
//	size_t bufsize=32;
//	while (true)
//	{
//		bufsize*=2;
//		free(path);
//		path=malloc(bufsize);
//		if (getcwd(path, bufsize)) return path;
//	}
//}

char * _window_native_get_absolute_path(const char * path)
{
	return realpath(path, NULL);
}

#elif defined(FILEPATH_WINDOWS)
#undef bind
#include <windows.h>
#define bind BIND_CB
#include <string.h>

const char * window_get_proc_path()
{
	static char path[MAX_PATH];
	GetModuleFileName(NULL, path, MAX_PATH);
	for (int i=0;path[i];i++)
	{
		if (path[i]=='\\') path[i]='/';
	}
	char * end=strrchr(path, '/');
	if (end) *end='\0';
	return path;
}

char * _window_native_get_absolute_path(const char * path)
{
	if (!path) return NULL;
	DWORD len=GetFullPathName(path, 0, NULL, NULL);
	char * ret=malloc(len);
	GetFullPathName(path, len, ret, NULL);
	for (int i=0;ret[i];i++)
	{
		if (ret[i]=='\\') ret[i]='/';
	}
	return ret;
}
#else
	//Mac OS X: _NSGetExecutablePath() http://stackoverflow.com/questions/799679/#1024933
	//Mac OS X, some BSDs, Solaris: dlinfo https://gist.github.com/kode54/8534201
	//Linux: readlink /proc/self/exe
	//Solaris: getexecname() http://download.oracle.com/docs/cd/E19253-01/816-5168/6mbb3hrb1/index.html
	//FreeBSD: sysctl CTL_KERN KERN_PROC KERN_PROC_PATHNAME -1 http://stackoverflow.com/questions/799679
	//FreeBSD if it has procfs: readlink /proc/curproc/file (FreeBSD doesn't have procfs by default)
	//NetBSD: readlink /proc/curproc/exe
	//DragonFly BSD: readlink /proc/curproc/file
	//Windows: GetModuleFileName() with hModule = NULL http://msdn.microsoft.com/en-us/library/ms683197.aspx
#error Unsupported OS
#endif
