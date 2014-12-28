#pragma once
#include "global.h"

#include <string.h> // strdup

#include <new>
template<typename key_t, typename val_t, typename key_t_pub = key_t> class assocarr : nocopy {
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
		struct node_t * * newnodes=malloc(sizeof(struct node_t*)*newbuckets);
		memset(newnodes, 0, sizeof(struct node_t*)*newbuckets);
		for (size_t i=0;i<this->buckets;i++)
		{
			struct node_t * node=this->nodes[i];
			while (node)
			{
				struct node_t * next=node->next;
				keyhash_t newpos=node->hash % newbuckets;
				node->next=newnodes[newpos];
				newnodes[newpos]=node;
				node=next;
			}
		}
		free(this->nodes);
		this->nodes=newnodes;
		this->buckets=newbuckets;
	}
	
	struct node_t * * find_ref(const key_t& key)
	{
		keyhash_t thehash=key.hash();
		struct node_t * * node=&this->nodes[thehash%this->buckets];
		while (true)
		{
			if (!node[0]) return NULL;
			if (node[0]->hash==thehash && key==node[0]->key) return node;
			node=&(node[0]->next);
		}
	}
	
	struct node_t * find(const key_t& key)
	{
		struct node_t * * node=find_ref(key);
		if (node) return *node;
		else return NULL;
	}
	
	//use only after checking that there is no item with this name already
	struct node_t * create(const key_t& key)
	{
		struct node_t * node=malloc(sizeof(struct node_t));
		keyhash_t thehash=key.hash();
		new(&node->key) key_t(key);
		node->hash=thehash;
		node->next=this->nodes[thehash%this->buckets];
		this->nodes[thehash%this->buckets]=node;
		this->entries++;
		if (this->entries > this->buckets) resize(this->buckets*2);
		return node;
	}
	
public:
	unsigned int size() { return this->entries; }
	
	bool has(const key_t& key) { return find(key); }
	
	val_t& get(const key_t& key)
	{
		struct node_t * node=find(key);
		if (!node)
		{
			node=create(key);
			new(&node->value) val_t();
		}
		return node->value;
	}
	
	val_t* get_ptr(const key_t& key)
	{
		struct node_t * node=find(key);
		if (node) return &node->value;
		else return NULL;
	}
	
	void set(const key_t& key, const val_t& value)
	{
		struct node_t * node=find(key);
		if (node)
		{
			node->value=value;
		}
		else
		{
			node=create(key);
			new(&node->value) val_t(value);
		}
	}
	
	void remove(const key_t& key)
	{
		struct node_t * * noderef=find_ref(key);
		if (!noderef) return;
		
		struct node_t * node=*noderef;
		*noderef=node->next;
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
			struct node_t * node=this->nodes[i];
			while (node)
			{
				struct node_t * next=node->next;
				node->key.~key_t();
				node->value.~val_t();
				free(node);
				node=next;
			}
		}
		free(this->nodes);
		this->nodes=NULL;
		this->buckets=0;
		this->entries=0;
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
	
	assocarr()
	{
		this->buckets=0;
		this->nodes=NULL;
		reset();
	}
	
	~assocarr()
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
template<typename T> class stringmap : public assocarr<simple_string, T, const char *> {};

template<typename T> class simple_ptr {
	uintptr_t ptr;
	simple_ptr();
public:
	simple_ptr(T other) { ptr=(uintptr_t)other; }
	simple_ptr(const simple_ptr& other) { ptr=other.ptr; }
	bool operator==(const simple_ptr& other) const { return (ptr==other.ptr); }
	operator T() const { return (T)ptr; }
	uintptr_t hash() const { return ptr; }
};
template<typename T1, typename T2> class ptrmap : public assocarr<simple_ptr<T1>, T2, T1> {};



template<typename T> class array {
	T * items;
	size_t count;
	
	void resize_grow(size_t count)
	{
		if (this->count >= count) return;
		size_t bufsize_pre=bitround(this->count);
		size_t bufsize_post=bitround(count);
		if (bufsize_pre != bufsize_post) this->items=realloc(this->items, sizeof(T)*bufsize_post);
		for (size_t i=this->count;i<count;i++)
		{
			new(&items[i]) T();
		}
		this->count=count;
	}
	
	void resize_shrink(size_t count)
	{
		if (this->count < count) return;
		for (size_t i=count;i<this->count;i++)
		{
			items[i]->~T();
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
	operator T*() { return items; }
	operator const T*() const { return items; }
	
	T join()
	{
		T out=items[0];
		for (size_t n=1;n<count;n++)
		{
			out+=items[n];
		}
		return out;
	}
	
	T join(T between)
	{
		T out=items[0];
		for (size_t n=1;n<count;n++)
		{
			out+=between;
			out+=items[n];
		}
		return out;
	}
	
	T join(char between)
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
	
	~array()
	{
		for (size_t i=0;i<this->count;i++) this->items[i].~T();
		free(this->items);
	}
};
