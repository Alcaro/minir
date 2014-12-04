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
