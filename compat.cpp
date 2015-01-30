#include "global.h"

#ifndef HAVE_ASPRINTF
#include <stdarg.h>
#include <stdio.h>
void asprintf(char * * ptr, const char * fmt, ...)
{
	va_list args;
	
	char * data=malloc(64);
	
	va_start(args, fmt);
	int neededlen=vsnprintf(data, 64, fmt, args);
	va_end(args);
	
	if (neededlen>=64)
	{
		free(data);
		data=malloc(neededlen+1);
		va_start(args, fmt);
		vsnprintf(data, neededlen+1, fmt, args);
		va_end(args);
	}
	
	*ptr=data;
}
#endif

/*
//not supported by gcc 4.8
#if __i386__ || __x86_64__
#include <x86intrin.h>
void unalign_lock()   { __writeeflags(__readeflags()| (1<<18)); }
void unalign_unlock() { __writeeflags(__readeflags()&~(1<<18)); }
#else
void unalign_lock()   {}
void unalign_unlock() {}
#endif
*/
//extern void unalign_lock();
//extern void unalign_unlock();
//#if __x86_64__
//__asm__(
//"unalign_lock:\n"
//"pushf\n"
//"movl $(1<<18),%eax\n"
//"orl %eax,(%rsp)\n"
//"popf\n"
//"ret\n"
//"unalign_unlock:\n"
//"pushf\n"
//"movl $(~(1<<18)),%eax\n"
//"andl %eax,(%rsp)\n"
//"popf\n"
//"ret\n"
//);
//#elif __i386__
//__asm__(
//"_unalign_lock:\n"
//"pushf\n"
//"movl $(1<<18),%eax\n"
//"orl %eax,(%esp)\n"
//"popf\n"
//"ret\n"
//"_unalign_unlock:\n"
//"pushf\n"
//"movl $(~(1<<18)),%eax\n"
//"andl %eax,(%esp)\n"
//"popf\n"
//"ret\n"
//);
//#else
//void unalign_lock() {}
//void unalign_unlock() {}
//#endif
