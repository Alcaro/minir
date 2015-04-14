#pragma once
#include "global.h"

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
//these two are the same, only the name varies
template<typename T1, typename T2> class ptrmap : public hashmap<hashable_int<T1, true>, T2, T1> {};
template<typename T1, typename T2> class intmap : public hashmap<hashable_int<T1, true>, T2, T1> {};

template<typename T> class intset : public hashmap<hashable_int<T, false>, void, T> {};
