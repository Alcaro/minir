#include "window.h"
#include "file.h"
#include "os.h"
#ifdef WINDOW_MINIMAL
#include <stdio.h>
#include <stdlib.h>

#include <time.h>

static void init_mutex_stuff();
void window_init(int * argc, char * * argv[])
{
	_window_init_native();
	init_mutex_stuff();
}

uint64_t window_get_time()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec*1000000 + ts.tv_nsec/1000;
}

bool file_read(const char * filename, void* * data, size_t * len)
{
	FILE * file=fopen(filename, "rb");
	if (!file) return false;
	fseek(file, 0, SEEK_END);
	size_t len_=ftell(file);
	fseek(file, 0, SEEK_SET);
	char* data_=malloc(len_+1);
	size_t truelen=fread(data_, 1,len_, file);
	fclose(file);
	
	if (len_!=truelen)
	{
		free(data_);
		return false;
	}
	data_[len_]='\0';
	*data=data_;
	if (len) *len=len_;
	return true;
}

bool file_write(const char * filename, const char * data, size_t len)
{
	FILE * file=fopen(filename, "wb");
	if (!file) return false;
	size_t truelen=fwrite(data, 1,len, file);
	return (len==truelen);
}

//bool file_read_to(const char * filename, char * data, size_t len)
//{
	//
//}

char * window_get_absolute_path(const char * basepath, const char * path, bool allow_up)
{
	return _window_native_get_absolute_path(basepath, path, allow_up);
}

#if defined(WINDOW_MINIMAL_IMUTEX)
static mutex* imutex[_imutex_count];
static void init_mutex_stuff()
{
	for (unsigned int i=0;i<_imutex_count;i++) imutex[i]=new mutex;
}

void _int_mutex_lock(enum _int_mutex id) { imutex[id]->lock(); }
bool _int_mutex_try_lock(enum _int_mutex id) { return imutex[id]->try_lock(); }
void _int_mutex_unlock(enum _int_mutex id) { imutex[id]->unlock(); }
#else
static void init_mutex_stuff() {}
void _int_mutex_lock(enum _int_mutex id) {}
bool _int_mutex_try_lock(enum _int_mutex id) {}
void _int_mutex_unlock(enum _int_mutex id) {}
#endif
#endif
