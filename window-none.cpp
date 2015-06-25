#include "window.h"
#include "file.h"
#include "os.h"
#ifdef WINDOW_MINIMAL
#include <stdio.h>
#include <stdlib.h>

#include <time.h>

void window_init(int * argc, char * * argv[]) {}

uint64_t window_get_time()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec*1000000 + ts.tv_nsec/1000;
}

file* file::create(const char * filename) { return create_fs(filename); }

#ifdef WINDOW_MINIMAL_NO_THREAD
mutex* mutex::create() { return NULL; }
void mutex::lock() {}
bool mutex::try_lock() { return true; }
void mutex::unlock() {}
void mutex::release() {}
#endif
#endif
