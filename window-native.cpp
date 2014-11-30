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

char * _window_native_get_absolute_path(const char * basepath, const char * path, bool allow_up)
{
	if (!path) return NULL;
	window_cwd_enter(NULL);
	char* freethis=NULL;
	const char * matchdir=window_cwd_get_default();
	if (basepath)
	{
		const char * filepart=strrchr(basepath, '/');
		if (filepart) // if no path is present, use current
		{
			char * basedir=strndup(basepath, filepart+1-basepath);
			freethis=basedir;
			chdir(basedir);
			matchdir=basedir;
		}
	}
	
	char * ret=realpath(path, NULL);
	
	window_cwd_leave();
	
	if (!allow_up)
	{
		if (strncasecmp(matchdir, ret, strlen(matchdir))!=0)
		{
			free(ret);
			free(freethis);
			return NULL;
		}
	}
	free(freethis);
	
	return ret;
}

static mutex* cwd_lock;
static char * cwd_init;
static char * cwd_bogus;

void window_cwd_enter(const char * dir)
{
	cwd_lock->lock();
	char * cwd_bogus_check=getcwd(NULL, 0);
	if (strcmp(cwd_bogus, cwd_bogus_check)!=0) abort();//if this fires, someone changed the directory without us knowing - not allowed. cwd belongs to the frontend.
	if (dir) chdir(dir);
	else chdir(cwd_init);
}

void window_cwd_leave()
{
	chdir(cwd_bogus);
	cwd_lock->unlock();
}

const char * window_cwd_get_default()
{
	return cwd_init;
}

void _window_init_shared()
{
	cwd_lock=new mutex();
	
	cwd_init=getcwd(NULL, 0);
	//try a couple of useless directories and hope one of them works
	chdir("/");
	chdir("/tmp");
	chdir("/home");
	chdir("/dev");
	chdir("/dev/cpu");
	cwd_bogus=getcwd(NULL, 0);
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

char * _window_native_get_absolute_path(const char * basepath, const char * path, bool allow_up)
{
	if (!path) return NULL;
	window_cwd_enter(NULL);
	char* freethis=NULL;
	const char * matchdir=window_cwd_get_default();
	if (basepath)
	{
		DWORD len=GetFullPathName(basepath, 0, NULL, NULL);
		char * dir=malloc(len);
		freethis=dir;
		char * filepart;
		GetFullPathName(basepath, len, dir, &filepart);
		if (filepart) *filepart='\0';
		SetCurrentDirectory(dir);
		for (unsigned int i=0;dir[i];i++)
		{
			if (dir[i]=='\\') dir[i]='/';
		}
		matchdir=dir;
	}
	
	DWORD len=GetFullPathName(path, 0, NULL, NULL);
	char * ret=malloc(len);
	GetFullPathName(path, len, ret, NULL);
	
	window_cwd_leave();
	
	for (unsigned int i=0;i<len;i++)
	{
		if (ret[i]=='\\') ret[i]='/';
	}
	
	if (!allow_up)
	{
		if (strncasecmp(matchdir, ret, strlen(matchdir))!=0)
		{
			free(freethis);
			free(ret);
			return NULL;
		}
	}
	free(freethis);
	
	return ret;
}

static CRITICAL_SECTION cwd_lock;
static char * cwd_init;
static char * cwd_bogus;
static char * cwd_bogus_check;
static DWORD cwd_bogus_check_len;

void window_cwd_enter(const char * dir)
{
	EnterCriticalSection(&cwd_lock);
	GetCurrentDirectory(cwd_bogus_check_len, cwd_bogus_check);
	if (strcmp(cwd_bogus, cwd_bogus_check)!=0) abort();//if this fires, someone changed the directory without us knowing - not allowed. cwd belongs to the frontend.
	if (dir) SetCurrentDirectory(dir);
	else SetCurrentDirectory(cwd_init);
}

void window_cwd_leave()
{
	SetCurrentDirectory(cwd_bogus);
	LeaveCriticalSection(&cwd_lock);
}

const char * window_cwd_get_default()
{
	return cwd_init;
}

void _window_init_shared()
{
	InitializeCriticalSection(&cwd_lock);
	
	DWORD len=GetCurrentDirectory(0, NULL);
	cwd_init=malloc(len+1);
	GetCurrentDirectory(len, cwd_init);
	len=strlen(cwd_init);
	for (unsigned int i=0;i<len;i++)
	{
		if (cwd_init[i]=='\\') cwd_init[i]='/';
	}
	if (cwd_init[len-1]!='/')
	{
		cwd_init[len+0]='/';
		cwd_init[len+1]='\0';
	}
	
	//try a couple of useless directories and hope one of them works
	//(this code is downright Perl-like, but the alternative is a pile of ugly nesting)
	SetCurrentDirectory("\\Users") ||
	SetCurrentDirectory("\\Documents and Settings") ||
	SetCurrentDirectory("\\Windows") ||
	(SetCurrentDirectory("C:\\") && false) ||
	SetCurrentDirectory("\\Users") ||
	SetCurrentDirectory("\\Documents and Settings") ||
	SetCurrentDirectory("\\Windows") ||
	SetCurrentDirectory("\\");
	
	len=GetCurrentDirectory(0, NULL);
	cwd_bogus=malloc(len);
	cwd_bogus_check=malloc(len);
	cwd_bogus_check_len=len;
	GetCurrentDirectory(len, cwd_bogus);
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
