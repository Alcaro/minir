#include "file.h"
#include "os.h"

#define MMAP_THRESHOLD 128*1024

#ifdef FILEPATH_WINDOWS
#undef bind
#include <windows.h>
#define bind bind_func
#include <string.h>

static void window_cwd_enter(const char * dir);
static void window_cwd_leave();

const char * window_get_proc_path()
{
	static char path[MAX_PATH];
	GetModuleFileName(NULL, path, MAX_PATH);
	for (int i=0;path[i];i++)
	{
		if (path[i]=='\\') path[i]='/';
	}
	char * end=strrchr(path, '/');
	if (end) end[1]='\0';
	return path;
}

char * _window_native_get_absolute_path(const char * basepath, const char * path, bool allow_up)
{
	if (!path || !basepath) return NULL;
	
	DWORD len=GetFullPathName(basepath, 0, NULL, NULL);
	char * matchdir=malloc(len);
	char * filepart;
	GetFullPathName(basepath, len, matchdir, &filepart);
	if (filepart) *filepart='\0';
	window_cwd_enter(matchdir);
	for (unsigned int i=0;matchdir[i];i++)
	{
		if (matchdir[i]=='\\') matchdir[i]='/';
	}
	
	len=GetFullPathName(path, 0, NULL, NULL);
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
			free(matchdir);
			free(ret);
			return NULL;
		}
	}
	free(matchdir);
	
	return ret;
}

static char * cwd_init;
static char * cwd_bogus;
static char * cwd_bogus_check;
static DWORD cwd_bogus_check_len;

static void window_cwd_enter(const char * dir)
{
	_int_mutex_lock(_imutex_cwd);
	GetCurrentDirectory(cwd_bogus_check_len, cwd_bogus_check);
	if (strcmp(cwd_bogus, cwd_bogus_check)!=0) abort();//if this fires, someone changed the directory without us knowing - not allowed. cwd belongs to the frontend.
	SetCurrentDirectory(dir);
}

static void window_cwd_leave()
{
	SetCurrentDirectory(cwd_bogus);
	_int_mutex_unlock(_imutex_cwd);
}

const char * window_get_cwd()
{
	return cwd_init;
}

void _window_init_native()
{
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



//static void* file_alloc(int fd, size_t len, bool writable)
//{

//}
//static size_t pagesize()
//{
//	SYSTEM_INFO inf;
//	GetSystemInfo(&inf);
//	return inf.dwPageSize;//dwAllocationGranularity?
//}
//static size_t allocgran()
//{
//	SYSTEM_INFO inf;
//	GetSystemInfo(&inf);
//	return inf.dwAllocationGranularity;
//}

namespace {
	class file_fs_rd : public file::impl {
	public:
		HANDLE handle;
		file_fs_rd(HANDLE handle, size_t len) : file::impl(len), handle(handle) {}
		
		void read(size_t start, void* target, size_t len)
		{
			LARGE_INTEGER pos;
			pos.QuadPart=start;
			SetFilePointerEx(this->handle, &pos, NULL, FILE_BEGIN);
			DWORD ignore;
		more:
			ReadFile(this->handle, target, len, &ignore, NULL);
			if (len>0xFFFFFFFF)//you shouldn't be reading multiple gigabytes at all...
			{
				len-=0xFFFFFFFF;
				target+=0xFFFFFFFF;
				goto more;
			}
		}
		
		void* map(size_t start, size_t len)
		{
			HANDLE mem=CreateFileMapping(handle, NULL, PAGE_READONLY, 0, 0, NULL);
			void* ptr=MapViewOfFile(mem, FILE_MAP_READ, (SIZE_MAX>0xFFFFFFFF ? start>>32 : 0), start&0xFFFFFFFF, len);
			CloseHandle(mem);
			return ptr;
		}
		
		void unmap(const void* data, size_t len) { UnmapViewOfFile((void*)data); }
		~file_fs_rd() { CloseHandle(handle); }
	};
}

file::impl* file::create_fs(const char * filename)
{
	HANDLE file=CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,  FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) return NULL;
	LARGE_INTEGER size;
	GetFileSizeEx(file, &size);
	return new file_fs_rd(file, size.QuadPart);
}

//namespace {
//	class file_fs_wr : public filewrite {
//	public:
//		int fd;
//		file_fs_wr(int fd) : fd(fd) {}
		
//		/*private*/ void alloc(size_t size)
//		{

//		}
		
//		/*private*/ void dealloc()
//		{

//		}
		
//		bool resize(size_t newsize)
//		{

//		}
		
//		void sync()
//		{

//		}
		
//		~file_fs_wr()
//		{
//			sync();
//			dealloc();
//			close(this->fd);
//		}
//	};
//};

//filewrite* filewrite::create_fs(const char * filename, bool truncate)
//{

//}
#endif
