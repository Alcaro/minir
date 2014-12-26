#pragma once
#include "global.h"

#include <string.h> // strdup

#include <new>
template<typename T> class assocarr : nocopy {
protected:
	typedef size_t keyhash_t;
	
	static keyhash_t hash(const char * str)
	{
		keyhash_t ret=0;
		while (*str)
		{
			ret*=101;
			ret+=*str;
			str++;
		}
		return ret;
	}
	
	struct node_t {
		struct node_t * next;
		char * key;
		keyhash_t hash;
		T value;
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
	
	struct node_t * * find_ref(const char * key)
	{
		keyhash_t thehash=hash(key);
		struct node_t * * node=&this->nodes[thehash%this->buckets];
		while (true)
		{
			if (!node[0]) return NULL;
			if (node[0]->hash==thehash && !strcmp(key, node[0]->key)) return node;
			node=&(node[0]->next);
		}
	}
	
	struct node_t * find(const char * key)
	{
		struct node_t * * node=find_ref(key);
		if (node) return *node;
		else return NULL;
	}
	
	//use only after checking that there is no item with this name already
	struct node_t * create(const char * key)
	{
		struct node_t * node=malloc(sizeof(struct node_t));
		keyhash_t thehash=hash(key);
		node->key=strdup(key);
		node->hash=thehash;
		node->next=this->nodes[thehash%this->buckets];
		this->nodes[thehash%this->buckets]=node;
		this->entries++;
		if (this->entries > this->buckets) resize(this->buckets*2);
		return node;
	}
	
public:
	unsigned int size() { return this->entries; }
	
	bool has(const char * key) { return find(key); }
	
	T& get(const char * key)
	{
		struct node_t * node=find(key);
		if (!node)
		{
			node=create(key);
			new(&node->value) T();
		}
		return node->value;
	}
	
	T* get_ptr(const char * key)
	{
		struct node_t * node=find(key);
		if (node) return &node->value;
		else return NULL;
	}
	
	void set(const char * key, const T& value)
	{
		struct node_t * node=find(key);
		if (node)
		{
			node->value=value;
		}
		else
		{
			node=create(key);
			new(&node->value) T(value);
		}
	}
	
	void remove(const char * key)
	{
		struct node_t * * noderef=find_ref(key);
		if (!noderef) return;
		
		struct node_t * node=*noderef;
		*noderef=node->next;
		if (node->used) this->used_entries--;
		free(node->key);
		node->value.~T();
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
				node->value.~T();
				free(node->key);
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
	
	void each(function<void(const char * key, T& value)> iter)
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
				free(item->key);
				item->value.~T();
				free(item);
				item=next;
			}
		}
	}
};



template<typename T> class array {
	T * items;
	size_t count;
	size_t buflen;
public:
	
	T& operator[](size_t n)
	{
		
	}
	
	const T& operator[](size_t n) const
	{
		return items[n];
	}
	
	size_t len() const { return count; }
	
	~array()
	{
		for (size_t i=0;i<count;i++) items[i].~T();
		free(items);
	}
};

class string;
typedef array<string> stringlist;
