#pragma once
#include "global.h"

template<typename T> class multiint_inline {
	enum { numinline = sizeof(T*) / sizeof(T) };
	
	static_assert(numinline > 1);//there must be sufficient space for at least two ints in a pointer
	static_assert(numinline * sizeof(T) == sizeof(T*));//the size of a pointer must be a multiple of the size of an int
	static_assert((numinline & (numinline-1)) == 0);//this multiple must be a power of two
	static_assert(numinline<<1 < (T)-1);//the int must be large enough to store how many ints fit in a pointer, plus the tag bit
	
	//the value is either:
	//if the lowest bit of ptr_raw is set:
	// the number of items is (ptr>>1) & numinline
	// the others are in inlines_raw[], at position inlines()
	//else:
	// the number of items is in ptr[0]
	// the items are in ptr[1..count]
	union {
		T* ptr_raw;
		T inlines_raw[numinline];
	};
	
	static int tag_offset()
	{
		union {
			T* ptr;
			T uint[numinline];
		} u;
		u.ptr = (T*)(uintptr_t)0xFFFF;
		
		if (u.uint[0]==0xFFFF) return 0; // little endian
		else if (u.uint[numinline-1]==0xFFFF) return numinline-1; // big endian
		else return -1; // middle endian - let's blow up (middle endian is dead, anyways)
	}
	
	T& tag()
	{
		return inlines_raw[tag_offset()];
	}
	
	T tag() const
	{
		return inlines_raw[tag_offset()];
	}
	
	bool is_inline() const
	{
		return tag()&1;
	}
	
	T* inlines()
	{
		if (tag_offset()==0) return inlines_raw+1;
		else return inlines_raw;
	}
	
	
	void clone(const multiint_inline<T>& other)
	{
		memcpy(this, &other, sizeof(*this));
		if (!is_inline())
		{
			ptr_raw = malloc(sizeof(T)*(1+other.count()));
			memcpy(ptr_raw, other.ptr_raw, sizeof(T)*(1+other.count()));
		}
	}
	
	void swap(multiint_inline<T>& other)
	{
		T* newptr = other.ptr_raw;
		other.ptr_raw = ptr_raw;
		ptr_raw = newptr;
	}
	
	
	//If increased, does not initialize the new entries. If decreased, drops the top.
	void set_count(T newcount)
	{
		T oldcount=count();
		T* oldptr=ptr();
		
		if (oldcount < numinline && newcount < numinline)
		{
			tag() = newcount<<1 | 1;
		}
		if (oldcount >= numinline && newcount < numinline)
		{
			T* freethis = ptr_raw;
			memcpy(inlines(), oldptr, sizeof(T)*newcount);
			tag() = newcount<<1 | 1;
			free(freethis);
		}
		if (oldcount < numinline && newcount >= numinline)
		{
			T* newptr = malloc(sizeof(T)*(1+newcount));
			newptr[0] = newcount;
			memcpy(newptr+1, oldptr, sizeof(T)*oldcount);
			ptr_raw = newptr;
		}
		if (oldcount >= numinline && newcount >= numinline)
		{
			ptr_raw = realloc(ptr_raw, sizeof(T)*(1+newcount));
			ptr_raw[0] = newcount;
		}
	}
	
public:
	multiint_inline()
	{
		tag() = 0<<1 | 1;
	}
	
	multiint_inline(const multiint_inline<T>& prev)
	{
		clone(prev);
	}
	
	multiint_inline<T>& operator=(multiint_inline<T> other)
	{
		swap(other);
		return *this;
	}
	
	~multiint_inline()
	{
		if (!is_inline()) free(ptr_raw);
	}
	
	T* ptr()
	{
		if (is_inline()) return inlines();
		else return ptr_raw+1;
	}
	
	const T* ptr() const
	{
		if (is_inline()) return inlines();
		else return ptr_raw+1;
	}
	
	T count() const
	{
		if (is_inline()) return tag()>>1;
		else return ptr_raw[0];
	}
	
	void add(T val)
	{
		T* entries = ptr();
		T num = count();
		
		for (T i=0;i<num;i++)
		{
			if (entries[i]==val) return;
		}
		
		add_uniq(val);
	}
	
	//Use this if the value is known to not exist in the set already.
	void add_uniq(T val)
	{
		T num = count();
		set_count(num+1);
		ptr()[num] = val;
	}
	
	void remove(T val)
	{
		T* entries = ptr();
		T num = count();
		
		for (T i=0;i<num;i++)
		{
			if (entries[i]==val)
			{
				entries[i] = entries[num-1];
				set_count(num-1);
				break;
			}
		}
	}
	
	T* get(T& len)
	{
		len=count();
		return ptr();
	}
	
	const T* get(T& len) const
	{
		len=count();
		return ptr();
	}
	
	//Guaranteed to remain ordered until the next add() or remove().
	void sort()
	{
		::sort(ptr(), count());
	}
};

template<typename T> class multiint_outline {
	T numitems;
	T* items;
	
	void clone(const multiint_outline<T>& other)
	{
		numitems = other.numitems;
		if (numitems)
		{
			items = malloc(sizeof(T)*numitems);
			memcpy(items,other.items, sizeof(T)*numitems);
		}
		else items = NULL;
	}
	
	void swap(multiint_outline<T>& other)
	{
		numitems = other.numitems;
		//don't bother fixing other.numitems, it's not used in the destructor
		T* newitems = other.items;
		other.items = items;
		items = newitems;
	}
	
public:
	multiint_outline()
	{
		numitems=0;
		items=NULL;
	}
	
	multiint_outline(const multiint_outline<T>& prev)
	{
		clone(prev);
	}
	
	multiint_outline<T>& operator=(multiint_outline<T> other)
	{
		swap(other);
		return *this;
	}
	
	~multiint_outline()
	{
		free(items);
	}
	
	T* ptr()
	{
		return items;
	}
	
	const T* ptr() const
	{
		return items;
	}
	
	T count() const
	{
		return numitems;
	}
	
	void add(T val)
	{
		for (T i=0;i<numitems;i++)
		{
			if (items[i]==val) return;
		}
		
		add_uniq(val);
	}
	
	//Use this if the value is known to not exist in the set already.
	void add_uniq(T val)
	{
		items = realloc(items, sizeof(T)*(numitems+1));
		items[numitems] = val;
		numitems++;
	}
	
	void remove(T val)
	{
		for (T i=0;i<numitems;i++)
		{
			if (items[i]==val)
			{
				items[i] = items[numitems-1];
				numitems--;
				break;
			}
		}
	}
	
	T* get(T& len)
	{
		len = numitems;
		return items;
	}
	
	const T* get(T& len) const
	{
		len = numitems;
		return items;
	}
	
	void sort()
	{
		::sort(items, numitems);
	}
};

template<typename T, bool useinline> class multiint_select : public multiint_inline<T> {};
template<typename T> class multiint_select<T, false> : public multiint_outline<T> {};
template<typename T> class multiint : public multiint_select<T, (sizeof(T*) >= 2*sizeof(T))> {};
