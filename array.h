#pragma once
#include "global.h"

//size: two pointers, plus one T per item
template<typename T> class array {
	T * items;
	size_t count;
	
	void clone(const array<T>& other)
	{
		this->count=other.count;
		this->items=malloc(sizeof(T)*bitround(this->count));
		for (size_t i=0;i<this->count;i++) new(&this->items[i]) T(other.items[i]);
	}
	
	void swap(array<T>& other)
	{
		T* newitems = other.items;
		size_t newcount = other.count;
		other.items = newitems;
		other.count = count;
		items = newitems;
		count = newcount;
	}
	
	void resize_grow(size_t count)
	{
		if (this->count >= count) return;
		size_t bufsize_pre=bitround(this->count);
		size_t bufsize_post=bitround(count);
		if (bufsize_pre != bufsize_post) this->items=realloc(this->items, sizeof(T)*bufsize_post);
		for (size_t i=this->count;i<count;i++)
		{
			new(&this->items[i]) T();
		}
		this->count=count;
	}
	
	void resize_shrink(size_t count)
	{
		if (this->count < count) return;
		for (size_t i=count;i<this->count;i++)
		{
			this->items[i]->~T();
		}
		size_t bufsize_pre=bitround(this->count);
		size_t bufsize_post=bitround(count);
		if (bufsize_pre != bufsize_post) this->items=realloc(this->items, sizeof(T)*bufsize_post);
		this->count=count;
	}
	
	void resize_to(size_t count)
	{
		if (count > this->count) resize_grow(count);
		else resize_shrink(count);
	}
	
public:
	T& operator[](size_t n) { resize_grow(n+1); return items[n]; }
	const T& operator[](size_t n) const { return items[n]; }
	size_t len() const { return count; }
	
	T* ptr() { return items; }
	void resize(size_t len) { resize_to(len); }
	
	T join() const
	{
		T out=items[0];
		for (size_t n=1;n<count;n++)
		{
			out+=items[n];
		}
		return out;
	}
	
	T join(T between) const
	{
		T out=items[0];
		for (size_t n=1;n<count;n++)
		{
			out+=between;
			out+=items[n];
		}
		return out;
	}
	
	T join(char between) const
	{
		T out=items[0];
		for (size_t n=1;n<count;n++)
		{
			out+=between;
			out+=items[n];
		}
		return out;
	}
	
	void append(const T& item) { size_t pos=this->count; resize_grow(pos+1); items[pos]=item; }
	void reset() { resize_shrink(0); }
	
	array()
	{
		this->items=NULL;
		this->count=0;
	}
	
	array(const array<T>& other)
	{
		clone(other);
	}
	
	array<T> operator=(array<T> other)
	{
		swap(other);
		return *this;
	}
	
	~array()
	{
		for (size_t i=0;i<this->count;i++) this->items[i].~T();
		free(this->items);
	}
};
