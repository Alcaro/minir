#include "minir.h"
#ifdef WINDOW_WIN32
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#define _WIN32_IE 0x0600
#include <windows.h>

//Number of ugly hacks: 4
//If a status bar item is right-aligned, a space is appended.
//The status bar is created with WS_DISABLED.
//WM_SYSCOMMAND is sometimes ignored.
//I have to keep track of the mouse position so I can ignore various bogus instances of WM_MOUSEMOVE.

//XP incompatibility status:
//Level 1 - a feature is usable, but behaves weirdly
//Level 2 - attempting to use a feature throws an error box, or reports failure in a way the program can and does handle
//Level 3 - attempting to use a feature reports success internally, but nothing happens
//Level 4 - attempting to use a feature crashes the program
//Level 5 - program won't start
//Maximum allowed incompatibility level: 2 [will be increased later, and eventually this list removed]
//List:
//Level 1: LVCFMT_FIXED_WIDTH on the listbox requires Vista+
//Danger list (likely to hit):
//Level 4: printf dislikes z (size_t) size specifiers; they must be behind #ifdef DEBUG, or turned into "I" via #define

//static LARGE_INTEGER timer_freq;

void window_init(int * argc, char * * argv[])
{
	for (int i=0;(*argv)[0][i];i++)
	{
		if ((*argv)[0][i]=='\\') (*argv)[0][i]='/';
	}
	
	_window_init_shell();
	_window_init_inner();
	
	//QueryPerformanceFrequency(&timer_freq);
}

bool window_message_box(const char * text, const char * title, enum mbox_sev severity, enum mbox_btns buttons)
{
	UINT sev[3]={ 0, MB_ICONWARNING, MB_ICONERROR };
	UINT btns[3]={ 0, MB_OKCANCEL, MB_YESNO };
	int ret=MessageBox(NULL, text, title, sev[severity]|btns[buttons]|MB_TASKMODAL);
	return (ret==IDOK || ret==IDYES);
}

const char * const * window_file_picker(struct window * parent,
                                        const char * title,
                                        const char * const * extensions,
                                        const char * extdescription,
                                        bool dylib,
                                        bool multiple)
{
	//there is no reasonable way to use the dylib flag; windows has nothing gvfs-like (okay, maybe IShellItem, but I can't get that from GetOpenFileName).
	static char * * ret=NULL;
	if (ret)
	{
		char * * delete=ret;
		while (*delete)
		{
			free(*delete);
			delete++;
		}
		free(ret);
		ret=NULL;
	}
	
	unsigned int filterlen=strlen(extdescription)+1+0+1-1+strlen("All files")+1+strlen("*.*")+1+1;
	for (unsigned int i=0;extensions[i];i++) filterlen+=2+strlen(extensions[i])+1;
	char * filter=malloc(filterlen);
	
	char * filterat=filter;
	strcpy(filterat, extdescription);
	filterat+=strlen(extdescription)+1;
	for (unsigned int i=0;extensions[i];i++)
	{
		unsigned int thislen=strlen(extensions[i]);
		filterat[0]='*';
		filterat[1]='.';
		if (*extensions[i]=='.') filterat--;
		memcpy(filterat+2, extensions[i], thislen);
		filterat[2+thislen]=';';
		filterat+=2+thislen+1;
	}
	memcpy(filterat-1, "\0All files\0*.*\0", 1+strlen("All files")+1+strlen("*.*")+1+1);
	
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize=sizeof(ofn);
	ofn.hwndOwner=(parent?(HWND)parent->_get_handle(parent):NULL);
	ofn.lpstrFilter=(extensions[0] ? filter : "All files (*.*)\0*.*\0");
	char * filenames=malloc(65536);
	*filenames='\0';
	ofn.lpstrFile=filenames;
	ofn.nMaxFile=65536;
	ofn.lpstrTitle=title;
	ofn.Flags=OFN_HIDEREADONLY|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER|(multiple?OFN_ALLOWMULTISELECT:0);
	ofn.lpstrDefExt=NULL;
	
	DWORD cwd_len=GetCurrentDirectoryW(0, NULL);
	WCHAR cwd[cwd_len+1];
	GetCurrentDirectoryW(cwd_len+1, cwd);
	BOOL ok=GetOpenFileName(&ofn);
	SetCurrentDirectoryW(cwd);
	
	free(filter);
	if (!ok)
	{
		free(filenames);
		return NULL;
	}
	
	bool ismultiple=(ofn.nFileOffset && filenames[ofn.nFileOffset-1]=='\0');
	if (!ismultiple)
	{
		ret=malloc(sizeof(char*)*2);
		ret[0]=strdup(filenames);
		ret[1]=NULL;
		return (const char * const *)ret;
	}
	filenames[ofn.nFileOffset-1]='\\';
	
	unsigned int numfiles=0;
	char * filename=filenames+ofn.nFileOffset;
	while (*filename)
	{
		numfiles++;
		filename+=strlen(filename)+1;
	}
	
	ret=malloc(sizeof(char*)*(numfiles+1));
	filename=filenames+ofn.nFileOffset;
	char * * retout=ret;
	while (*filename)
	{
		unsigned int thislen=strlen(filename);
		memcpy(filenames+ofn.nFileOffset, filename, thislen+1);
		*retout=window_get_absolute_path(filenames);
		retout++;
		filename+=thislen+1;
	}
	free(filenames);
	
	return (const char * const *)ret;
}

char * window_get_absolute_path(const char * path)
{
	return _window_native_get_absolute_path(path);
}

uint64_t window_get_time()
{
	//this one has an accuracy of 10ms by default
	ULARGE_INTEGER time;
	GetSystemTimeAsFileTime((LPFILETIME)&time);
	return time.QuadPart/10;//this one is in intervals of 100 nanoseconds, for some insane reason. We want microseconds.
	
	//this one is slow - ~800fps -> ~500fps if called each frame
	//LARGE_INTEGER timer_now;
	//QueryPerformanceCounter(&timer_now);
	//return timer_now.QuadPart/timer_freq.QuadPart;
}



bool file_read(const char * filename, char* * data, size_t * len)
{
	if (!filename) return false;
	HANDLE file=CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file==INVALID_HANDLE_VALUE) return false;
	DWORD readlen=GetFileSize(file, NULL);
	DWORD truelen;
	*data=malloc(readlen+1);
	(*data)[readlen]='\0';
	ReadFile(file, *data, readlen, &truelen, NULL);
	CloseHandle(file);
	if (truelen!=readlen)
	{
		free(data);
		return false;
	}
	if (len) *len=truelen;
	return true;
}

bool file_write(const char * filename, const char * data, size_t len)
{
	if (!filename) return false;
	if (!len) return true;
	HANDLE file=CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file==INVALID_HANDLE_VALUE) return false;
	DWORD truelen;
	WriteFile(file, data, len, &truelen, NULL);
	CloseHandle(file);
	return (truelen==len);
}

bool file_read_to(const char * filename, char * data, size_t len)
{
	if (!filename) return false;
	if (!len) return true;
	HANDLE file=CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file==INVALID_HANDLE_VALUE) return false;
	DWORD readlen=GetFileSize(file, NULL);
	if (readlen!=len)
	{
		CloseHandle(file);
		return false;
	}
	DWORD truelen;
	ReadFile(file, data, len, &truelen, NULL);
	CloseHandle(file);
	return (len==truelen);
}


//this could be made far cleaner if . or .. was guaranteed to be first.
struct finddata {
	HANDLE h;
	WIN32_FIND_DATA file;
	bool first;
};

void* file_find_create(const char * path)
{
	if (!path) return NULL;
	int pathlen=strlen(path);
	char * pathcopy=malloc(pathlen+3);
	memcpy(pathcopy, path, pathlen);
	pathcopy[pathlen+0]='\\';
	pathcopy[pathlen+1]='*';
	pathcopy[pathlen+2]='\0';
	struct finddata * find=malloc(sizeof(struct finddata));
	find->h=FindFirstFile(pathcopy, &find->file);
	free(pathcopy);
	find->first=true;
	if (find->h==INVALID_HANDLE_VALUE)
	{
		free(find);
		return NULL;
	}
	return find;
}

bool file_find_next(void* find_, char * * path, bool * isdir)
{
	if (!find_) return false;
	struct finddata * find=find_;
nextfile:;
	bool ok=true;
	if (find->first) find->first=false;
	else ok=FindNextFile(find->h, &find->file);
	if (!ok) return false;
	if (!strcmp(find->file.cFileName, ".")) goto nextfile;
	if (!strcmp(find->file.cFileName, "..")) goto nextfile;
	*path=strdup(find->file.cFileName);
	*isdir=(find->file.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY);
	return true;
}

void file_find_close(void* find_)
{
	if (!find_) return;
	struct finddata * find=find_;
	FindClose(find->h);
	free(find);
}
#endif
