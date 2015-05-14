#pragma once
#include "global.h"
#include "string.h"
#include "intwrap.h"

template<typename T> class hasher {
public:
	static size_t hash(const T& item) { return item.hash(); }
};

//size: three pointers, plus [one pointer plus one key_t] per entry
template<typename T>
class hashset : public nocopy {
protected:
	typedef size_t keyhash_t;
	
	struct node_t {
		struct node_t * next;
		T key;
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
				keyhash_t newpos = hasher<T>::hash(node->key) % newbuckets;
				node->next = newnodes[newpos];
				newnodes[newpos] = node;
				node = next;
			}
		}
		free(this->nodes);
		this->nodes = newnodes;
		this->buckets = newbuckets;
	}
	
	template<typename T2>
	struct node_t * * find_ref(T2 key) const
	{
		struct node_t * * noderef = &this->nodes[hasher<T>::hash(key) % this->buckets];
		while (true)
		{
			struct node_t * node = *noderef;
			if (!node) return NULL;
			if (node->key == key) return noderef;
			noderef = &node->next;
		}
	}
	
	template<typename T2>
	struct node_t * find(T2 key) const
	{
		struct node_t * * node = find_ref(key);
		if (node) return *node;
		else return NULL;
	}
	
	//use only after checking that there is no item with this name already
	template<typename T2>
	struct node_t * create(T2 key)
	{
		struct node_t * node = malloc(sizeof(struct node_t));
		keyhash_t thehash = hasher<T>::hash(key);
		new(&node->key) T(key);
		node->next = this->nodes[thehash%this->buckets];
		this->nodes[thehash%this->buckets] = node;
		this->entries++;
		if (this->entries > this->buckets) resize(this->buckets*2);
		return node;
	}
	
public:
	size_t size() const { return this->entries; }
	
	bool has(T key) const { return find((const T&)key); }
	
	template<typename T2>
	bool has_t(T2 key) const { return find(key); }
	
	template<typename T2>
	T& get_t(T2 key)
	{
		struct node_t * node = find(key);
		if (!node) node = create(key);
		return node->key;
	}
	
	T& get(T key) { return get_t((const T&)key); }
	
	template<typename T2>
	T& operator[](T2 key) { return get_t(key); }
	
	template<typename T2>
	T get_or_t(T2 key, const T& other) const
	{
		struct node_t * node = find(key);
		if (!node) return other;
		return node->value;
	}
	
	T get_or(T key, T other) const { return get_or_t((const T&)key, (const T&)other); }
	
	template<typename T2>
	T* get_ptr_t(T2 key)
	{
		struct node_t * node = find(key);
		if (node) return &node->key;
		else return NULL;
	}
	
	T* get_ptr(T key) { return get_ptr_t((const T&)key); }
	
	template<typename T2>
	void add_t(const T2& key)
	{
		if (!find(key)) create(key);
	}
	
	void add(const T& key) { return add_t(key); }
	
	template<typename T2>
	void remove_t(const T2& key)
	{
		struct node_t * * noderef = find_ref(key);
		if (!noderef) return;
		
		struct node_t * node = *noderef;
		*noderef = node->next;
		node->key.~T();
		free(node);
		
		this->entries--;
		if (this->buckets>4 && this->entries < this->buckets/2) resize(this->buckets/2);
	}
	
	void remove(T key) { remove_t(key); }
	void remove(const T& key) { remove_t(key); }
	
	void reset()
	{
		for (unsigned int i=0;i<this->buckets;i++)
		{
			struct node_t * node = this->nodes[i];
			while (node)
			{
				struct node_t * next = node->next;
				node->key.~T();
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
	
	
	class iterator {
		friend class hashset;
		hashset<T>* parent;
		size_t bucket;
		node_t* * current;
		
		iterator(hashset<T>* parent, size_t bucket, node_t* * current) : parent(parent), bucket(bucket), current(current) {}
		
		void next_valid()
		{
			while (!*current && bucket < parent->buckets)
			{
				bucket++;
				if (bucket < parent->buckets) current = &parent->nodes[bucket];
				else current = NULL;
			}
		}
		
	public:
		T& operator*()
		{
			return (*current)->key;
		}
		
		iterator& operator++()
		{
			current = &(*current)->next;
			next_valid();
			return *this;
		}
		
		iterator operator++(int)
		{
			iterator prev = *this;
			this->operator++();
			return prev;
		}
		
		bool operator==(const iterator& other)
		{
			//no need to check 'bucket'; if it's different, 'current' is too, so that one is enough.
			//however, 'current' can be NULL
			return (this->parent == other.parent && this->current == other.current);
		}
		
		operator bool() { return current; }
		
		void remove()
		{
			struct node_t * node = *current;
			*current = node->next;
			node->key.~T();
			free(node);
			parent->entries--;
			next_valid();
		}
	};
	friend class iterator;
	
	iterator begin() { return iterator(this, 0, &this->nodes[0]); }
	iterator end() { return iterator(this, 0, NULL); }
	
	
	template<typename T2>
	void each(function<void(T2 key)> iter)
	{
		for (unsigned int i=0;i<this->buckets;i++)
		{
			struct node_t * node=this->nodes[i];
			while (node)
			{
				struct node_t * next=node->next;
				iter(node->key);
				node=next;
			}
		}
	}
	
	template<typename T2>
	void remove_if(function<bool(T2 key)> condition)
	{
		for (unsigned int i=0;i<this->buckets;i++)
		{
			struct node_t * * noderef = &this->nodes[i];
			while (*noderef)
			{
				struct node_t * node = *noderef;
				bool remove = condition(node->key);
				
				if (remove)
				{
					struct node_t * nextnode = node->next;
					node->key.~T();
					free(node);
					this->entries--;
					*noderef = nextnode;
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
	
	hashset()
	{
		this->buckets=0;
		this->nodes=NULL;
		reset();
	}
	
	~hashset()
	{
		for (unsigned int i=0;i<this->buckets;i++)
		{
			struct node_t * item=this->nodes[i];
			while (item)
			{
				struct node_t * next=item->next;
				item->key.~T();
				free(item);
				item=next;
			}
		}
		free(this->nodes);
	}
};

template<typename K, typename V>
class hashmap {
	struct entry {
		K key;
		V value;
		size_t hash() const { return hasher<K>::hash(key); }
		bool operator==(const entry& other) const { return (this->key == other.key); }
		bool operator==(const K& key) const { return (this->key == key); }
		entry(K key) : key(key), value() {}
		entry(K key, V value) : key(key), value(value) {}
	};
	
	hashset<entry> items;
	
public:
	size_t size() const { return items.size(); }
	
	bool has(K key) const { return items.has_t((const K&)key); }
	
	V& get(K key) { return items.get_t((const K&)key).value; }
	
	V& operator[](K key) { return get((const K&)key); }
	
	V get_or(const K& key, const V& def)
	{
		entry* e = items.get_ptr_t(key);
		if (e) return e->value;
		else return def;
	}
	
	V* get_ptr(const K& key)
	{
		entry* e = items.get_ptr_t(key);
		if (e) return &e->value;
		else return NULL;
	}
	
	void set(const K& key, const V& value)
	{
		entry& e = items.get_t(key);
		e.value = value;
	}
	
	void remove(K key) { items.remove_t((const K&)key); }
	
	void reset() { items.reset(); }
	
	template<typename K2, typename V2>
	void each(function<void(K2 key, V2& value)> iter)
	{
		typename hashset<entry>::iterator begin = items.begin();
		typename hashset<entry>::iterator end = items.end();
		while (begin!=end)
		{
			entry& e = *begin;
			iter(e.key, e.value);
			begin++;
		}
	}
	
	template<typename K2, typename V2>
	void remove_if(function<bool(K2 key, V2& value)> condition)
	{
		typename hashset<entry>::iterator begin = items.begin();
		typename hashset<entry>::iterator end = items.end();
		while (begin!=end)
		{
			entry& e = *begin;
			if (condition(e.key, e.value)) begin.remove();
			else begin++;
		}
	}
};



//each of those hash functions is reversible
//it would be desirable to generate hash algorithms for all other values too, but zimbry chose to not publish his
// source codes nor results for other sizes, and I don't understand the relevant math well enough to recreate it
//it's not really important, anyways; this isn't OpenSSL
template<> class hasher<uint32_t> {
public:
	static size_t hash(uint32_t val)
	{
		//https://code.google.com/p/smhasher/wiki/MurmurHash3
		val ^= val >> 16;
		val *= 0x85ebca6b;
		val ^= val >> 13;
		val *= 0xc2b2ae35;
		val ^= val >> 16;
		return val;
	}
};

template<> class hasher<uint64_t> {
public:
	static size_t hash(uint64_t val)
	{
		//http://zimbry.blogspot.se/2011/09/better-bit-mixing-improving-on.html
		//using Mix13 because it gives the lowest mean error on low incoming entropy
		val ^= val >> 30;
		val *= 0xbf58476d1ce4e5b9;
		val ^= val >> 27;
		val *= 0x94d049bb133111eb;
		val ^= val >> 31;
		return val;
	}
};

template<> class hasher<uint8_t> {
public:
	static size_t hash(uint8_t val) { return hasher<uint32_t>::hash(val); }
};

template<> class hasher<uint16_t> {
public:
	static size_t hash(uint16_t val) { return hasher<uint32_t>::hash(val); }
};

template<typename T> class hasher<T*> {
public:
	static size_t hash(T* val)
	{
		return hasher<uintptr_t>::hash((uintptr_t)val);
	}
};
