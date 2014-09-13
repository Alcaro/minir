#include "minir.h"
#undef malloc
#undef realloc
#undef calloc
#include <stdlib.h>

anyptr malloc_check(size_t size)
{
	void* ret=malloc(size);
	if (!ret) abort();
	return ret;
}

anyptr try_malloc(size_t size)
{
	return malloc(size);
}

anyptr realloc_check(anyptr ptr, size_t size)
{
	void* ret=realloc(ptr, size);
	if (size && !ret) abort();
	return ret;
}

anyptr try_realloc(anyptr ptr, size_t size)
{
	return realloc(ptr, size);
}

anyptr calloc_check(size_t size, size_t count)
{
	void* ret=calloc(size, count);
	if (!ret) abort();
	return ret;
}

anyptr try_calloc(size_t size, size_t count)
{
	return calloc(size, count);
}
