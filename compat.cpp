#include "global.h"

#ifndef HAVE_ASPRINTF
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void asprintf(char * * ptr, const char * fmt, ...)
{
	va_list args;
	
	char tmpdata[64];
	
	va_start(args, fmt);
	int neededlen=vsnprintf(tmpdata, 64, fmt, args);
	va_end(args);
	
	if (neededlen>=64)
	{
		*ptr=malloc(neededlen+1);
		va_start(args, fmt);
		vsnprintf(*ptr, neededlen+1, fmt, args);
		va_end(args);
	}
	else
	{
		*ptr=strdup(tmpdata);
	}
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
