#include "minir.h"
#undef malloc
#undef realloc
#include <stdlib.h>

void* malloc_check(size_t size)
{
	void* ret=malloc(size);
	if (!ret) abort();
	return ret;
}

void* try_malloc(size_t size)
{
	return malloc(size);
}

void* realloc_check(void* ptr, size_t size)
{
	void* ret=realloc(ptr, size);
	if (size && !ret) abort();
	return ret;
}

void* try_realloc(void* ptr, size_t size)
{
	return realloc(ptr, size);
}
