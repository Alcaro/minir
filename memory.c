#include "minir.h"
#undef malloc
#undef realloc
#include <stdlib.h>
//#include<stdio.h>

//static int q=0;
//void ex()
//{
//printf("%i\n",q);
//}
void* malloc_check(size_t size)
{
//if(!q)atexit(ex);q++;
//if(q==17)
//{
//size+=128;
//}
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
//if(!q)atexit(ex);q++;
//if(q==406)
//{
//size+=128;
//}
//if(q>=600)size+=128;
	void* ret=realloc(ptr, size);
	if (!ret) abort();
	return ret;
}

void* try_realloc(void* ptr, size_t size)
{
	return realloc(ptr, size);
}
