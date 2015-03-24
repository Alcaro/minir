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

//size: three pointers, plus [two pointers plus one key_t plus one val_t] per entry
template<typename key_t, typename val_t, typename key_t_pub = key_t> class hashmap : nocopy {
protected:
	typedef size_t keyhash_t;
	
	struct node_t {
		struct node_t * next;
		keyhash_t hash;
		key_t key;
		val_t value;
	};
	struct node_t * * nodes;
	keyhash_t buckets;
	size_t entries;
	
	void resize(size_t newbuckets)
	{
		struct node_t * * newnodes = malloc(sizeof(struct node_t*)*newbuckets);
		memset(newnodes, 0, sizeof(struct node_t*)*newbuckets);
		for (size_t i=0;i<this->buckets;i++)
		{
			struct node_t * node = this->nodes[i];
			while (node)
			{
				struct node_t * next = node->next;
				keyhash_t newpos = node->hash % newbuckets;
				node->next = newnodes[newpos];
				newnodes[newpos] = node;
				node = next;
			}
		}
		free(this->nodes);
		this->nodes = newnodes;
		this->buckets = newbuckets;
	}
	
	struct node_t * * find_ref(const key_t& key) const
	{
		keyhash_t thehash = key.hash();
		struct node_t * * noderef = &this->nodes[thehash%this->buckets];
		while (true)
		{
			struct node_t * node = *noderef;
			if (!node) return NULL;
			if (node->hash == thehash && key == node->key) return noderef;
			noderef = &node->next;
		}
	}
	
	struct node_t * find(const key_t& key) const
	{
		struct node_t * * node = find_ref(key);
		if (node) return *node;
		else return NULL;
	}
	
	//use only after checking that there is no item with this name already
	struct node_t * create(const key_t& key)
	{
		struct node_t * node = malloc(sizeof(struct node_t));
		keyhash_t thehash = key.hash();
		new(&node->key) key_t(key);
		node->hash = thehash;
		node->next = this->nodes[thehash%this->buckets];
		this->nodes[thehash%this->buckets] = node;
		this->entries++;
		if (this->entries > this->buckets) resize(this->buckets*2);
		return node;
	}
	
public:
	size_t size() const { return this->entries; }
	
	bool has(const key_t& key) const { return find(key); }
	
	val_t& get(const key_t& key)
	{
		struct node_t * node=find(key);
		if (!node)
		{
			node = create(key);
			new(&node->value) val_t();
		}
		return node->value;
	}
	
	val_t get_or(const key_t& key, val_t other) const
	{
		struct node_t * node = find(key);
		if (!node) return other;
		return node->value;
	}
	
	val_t* get_ptr(const key_t& key)
	{
		struct node_t * node = find(key);
		if (node) return &node->value;
		else return NULL;
	}
	
	void set(const key_t& key, const val_t& value)
	{
		struct node_t * node = find(key);
		if (node)
		{
			node->value = value;
		}
		else
		{
			node = create(key);
			new(&node->value) val_t(value);
		}
	}
	
	void remove(const key_t& key)
	{
		struct node_t * * noderef = find_ref(key);
		if (!noderef) return;
		
		struct node_t * node = *noderef;
		*noderef = node->next;
		node->key.~key_t();
		node->value.~val_t();
		free(node);
		
		this->entries--;
		if (this->buckets>4 && this->entries < this->buckets/2) resize(this->buckets/2);
	}
	
	void reset()
	{
		for (unsigned int i=0;i<this->buckets;i++)
		{
			struct node_t * node = this->nodes[i];
			while (node)
			{
				struct node_t * next = node->next;
				node->key.~key_t();
				node->value.~val_t();
				free(node);
				node = next;
			}
		}
		free(this->nodes);
		this->nodes = NULL;
		this->buckets = 0;
		this->entries = 0;
		resize(4);
	}
	
	void each(function<void(key_t_pub key, val_t& value)> iter)
	{
		for (unsigned int i=0;i<this->buckets;i++)
		{
			struct node_t * node=this->nodes[i];
			while (node)
			{
				struct node_t * next=node->next;
				iter(node->key, node->value);
				node=next;
			}
		}
	}
	
	void remove_if(function<bool(key_t_pub key, val_t& value)> condition)
	{
		for (unsigned int i=0;i<this->buckets;i++)
		{
			struct node_t * * noderef = &this->nodes[i];
			while (*noderef)
			{
				struct node_t * node = *noderef;
				bool remove = condition(node->key, node->value);
				
				if (remove)
				{
					node->key.~key_t();
					node->value.~val_t();
					free(node);
					this->entries--;
					*noderef = node->next;
				}
				else noderef = &node->next;
			}
		}
		
		if (this->buckets>4 && this->entries < this->buckets/2)
		{
			size_t newbuckets = this->buckets;
			while (newbuckets>4 && this->entries < newbuckets/2) newbuckets /= 2;
			resize(newbuckets);
		}
	}
	
	hashmap()
	{
		this->buckets=0;
		this->nodes=NULL;
		reset();
	}
	
	~hashmap()
	{
		for (unsigned int i=0;i<this->buckets;i++)
		{
			struct node_t * item=this->nodes[i];
			while (item)
			{
				struct node_t * next=item->next;
				item->key.~key_t();
				item->value.~val_t();
				free(item);
				item=next;
			}
		}
		free(this->nodes);
	}
};

class simple_string {
	char* ptr;
	simple_string();
public:
	simple_string(const char * other) { ptr=strdup(other); }
	simple_string(const simple_string& other) { ptr=strdup(other.ptr); }
	~simple_string() { free(ptr); }
	bool operator==(const simple_string& other) const { return (!strcmp(ptr, other.ptr)); }
	operator const char *() const { return ptr; }
	uintptr_t hash() const
	{
		const char * str=ptr;
		size_t ret=0;
		while (*str)
		{
			ret*=101;
			ret+=*str;
			str++;
		}
		return ret;
	}
};
template<typename T> class stringmap : public hashmap<simple_string, T, const char *> {};

template<typename T, bool shuffle> class hashable_int {
	T ptr;
	hashable_int();
public:
	hashable_int(T other) { ptr=other; }
	hashable_int(const hashable_int& other) { ptr=other.ptr; }
	bool operator==(const hashable_int& other) const { return (ptr==other.ptr); }
	operator T() const { return ptr; }
	uintptr_t hash() const
	{
		if (!shuffle) return (uintptr_t)ptr;
		static_assert(!shuffle || sizeof(T)==4 || sizeof(T)==8);
		//each of those hash functions is reversible
		//it would be desirable to generate hash algorithms for all other values too, but zimbry chose to not publish his
		// source codes nor results for other sizes, and I don't understand the relevant math well enough to recreate it
		//it's not really important, anyways; this isn't OpenSSL
		if (sizeof(T) == 4)
		{
			//https://code.google.com/p/smhasher/wiki/MurmurHash3
			uint32_t val = (uint32_t)ptr;
			val ^= val >> 16;
			val *= 0x85ebca6b;
			val ^= val >> 13;
			val *= 0xc2b2ae35;
			val ^= val >> 16;
			return val;
		}
		if (sizeof(T) == 8)
		{
			//http://zimbry.blogspot.se/2011/09/better-bit-mixing-improving-on.html
			//using Mix13 because it gives the lowest mean error on low incoming entropy
			uint64_t val = (uint64_t)ptr;
			val ^= val >> 30;
			val *= 0xbf58476d1ce4e5b9;
			val ^= val >> 27;
			val *= 0x94d049bb133111eb;
			val ^= val >> 31;
			return val;
		}
	}
};
//these two support the same types, but act differently
//intmap shuffles the keys before using them, as integers sometimes have high predictability, and the hashmap doesn't like that
//ptrmap doesn't, because pointers are far less predictable
template<typename T1, typename T2> class ptrmap : public hashmap<hashable_int<T1, false>, T2, T1> {};
template<typename T1, typename T2> class intmap : public hashmap<hashable_int<T1, true>, T2, T1> {};

template<typename T> class intset : public hashmap<hashable_int<T, false>, void, T> {};



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
		T* newptr = other.ptr;
		other.ptr = ptr;
		ptr = newptr;
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
