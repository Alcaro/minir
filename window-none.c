#include "minir.h"
#ifdef WINDOW_MINIMAL
#include <stdio.h>
#include <stdlib.h>

#include <time.h>

uint64_t window_get_time()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec*1000000 + ts.tv_nsec/1000;
}

bool file_read(const char * filename, char* * data, size_t * len)
{
	FILE * file=fopen(filename, "rb");
	if (!file) return false;
	fseek(file, 0, SEEK_END);
	size_t len_=ftell(file);
	fseek(file, 0, SEEK_SET);
	*data=malloc(len_);
	size_t truelen=fread(*data, 1,len_, file);
	fclose(file);
	
	if (len_!=truelen)
	{
		free(*data);
		return false;
	}
	if (len) *len=len_;
	return true;
}

//bool file_write(const char * filename, const char * data, size_t len)
//{
	//
//}
//
//bool file_read_to(const char * filename, char * data, size_t len)
//{
	//
//}
#endif
