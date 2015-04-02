#pragma once
#include "global.h"
#include <string.h> // strdup
#include <new>

template<typename T> void sort(T* items, size_t count)
{
	for (size_t a=0;a<count;a++)
	{
		size_t b;
		for (b=0;b<a;b++)
		{
			if (items[a] < items[b]) break;
		}
		if (a == b) continue;
		
		char tmp[sizeof(T)];
		memcpy(tmp, items+a, sizeof(T));
		memmove(items+b+1, items+b, sizeof(T)*(a-b));
		memcpy(items+b, tmp, sizeof(T));
	}
}

#include "array.h"
#include "fifo.h"
#include "hashmap.h"
#include "multiint.h"
