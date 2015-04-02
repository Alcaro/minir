#pragma once
#include "global.h"

template<typename T> class fifo {
static const int init_capacity = 8;

T* items;
size_t buflen;
size_t head;
size_t tail;

T* push_ref()
{
	if ((head+1)%buflen == tail)
	{
		T* newitems = malloc(sizeof(T)*buflen*2);
		memcpy(newitems, items+tail, sizeof(T)*(buflen-tail));
		if (tail) memcpy(newitems+buflen-tail, items, sizeof(T)*(head));
		free(items);
		items = newitems;
		tail = 0;
		head = buflen-1;
		buflen *= 2;
	}
	
	T* ret=&items[head];
	head = (head+1)%buflen;
	return ret;
}

T* pop_ref()
{
	if (head==tail) return NULL;
	T* ret = &items[tail];
	tail = (tail+1)%buflen;
	return ret;
}

public:

void push(const T& obj)
{
	T* ref = push_ref();
	new(ref) T(obj);
}

void push_from(fifo<T>& other)
{
	T* src = other.pop_ref();
	if (!src) return;
	T* dst = this->push_ref();
	memcpy(dst, src, sizeof(T));
}

//Returns a default object if the queue is empty.
T pop()
{
	T* ref = pop_ref();
	if (!ref) return T();
	T ret = *ref;
	ref->~T();
	return ret;
}

//Returns a clone of the given object if the queue is empty.
T pop_or(const T& null)
{
	T* ref = pop_ref();
	if (!ref) return null;
	T ret = *ref;
	ref->~T();
	return ret;
}

//Crashes if the queue is empty.
T pop_checked()
{
	T* ref = pop_ref();
	T ret = *ref;
	ref->~T();
	return ret;
}

size_t count() const
{
	return (head-tail + buflen)%buflen;
}

bool empty() const
{
	return (head==tail);
}

fifo()
{
	buflen = init_capacity;
	items = malloc(sizeof(T)*buflen);
	head = 0;
	tail = 0;
}

~fifo()
{
	if (head >= tail)
	{
		for (size_t i=head;i<tail;i++) items[i].~T();
	}
	else
	{
		for (size_t i=head;i<buflen;i++) items[i].~T();
		for (size_t i=0;i<tail;i++) items[i].~T();
	}
}

};
