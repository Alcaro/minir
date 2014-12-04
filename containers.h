#pragma once
#include "global.h"
#include <string.h> // strdup

class string {
private:
	char * ptr;
	void set(const char * newstr) { if (newstr) ptr=strdup(newstr); else ptr=NULL; }
public:
	string() : ptr(NULL) {}
	string(const char * newstr) { set(newstr); }
	string(const string& newstr) { set(newstr.ptr); }
	~string() { free(ptr); }
	string& operator=(const char * newstr) { char* prev=ptr; set(newstr); free(prev); return *this; }
	string& operator=(string newstr) { char* tmp=newstr.ptr; newstr.ptr=ptr; ptr=tmp; return *this; } // my sources tell me this can sometimes avoid copying entirely
	operator const char * () { return ptr; }
};


#include <new>
template<typename T> class assocarr : nocopy {
private:
	typedef uint16_t keyhash_t;
	
	keyhash_t hash(const char * str)
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
	
	struct node {
		struct node * next;
		char * key;
		keyhash_t hash;
		T value;
	};
	struct node * * nodes;
	unsigned int buckets;
	unsigned int entries;
	
	void resize(unsigned int newbuckets)
	{
		struct node * * newnodes=malloc(sizeof(node*)*newbuckets);
		for (unsigned int i=0;i<this->buckets;i++)
		{
			struct node * item=this->nodes[i];
			while (item->key)
			{
				struct node * next=item->next;
				keyhash_t newpos=item->hash % newbuckets;
				item->next=newnodes[newpos];
				newnodes[newpos]=item;
				item=next;
			}
		}
		free(this->nodes);
		this->nodes=newnodes;
		this->buckets=newbuckets;
	}
	
	template<bool create, void(*construct_ptr)(void* loc, T* obj)>
	struct node * find(const char * key, T* initial)
	{
		keyhash_t thehash=hash(key);
		struct node * ret=this->nodes[thehash%this->buckets];
		while (true)
		{
			if (!ret->key)
			{
				if (!create) return NULL;
				ret->next=malloc(sizeof(struct node));
				this->entries++;
				ret->next->key=NULL;
				ret->key=strdup(key);
				ret->hash=thehash;
				construct_ptr(&ret->value, initial);
				if (this->entries > this->buckets) resize(this->buckets*2);
				return ret;
			}
			if (ret->hash==thehash && !strcmp(key, ret->key)) return ret;
			ret=ret->next;
		}
	}
	
	static void construct_none(void* loc, T* obj) {}
	struct node * find_if_exists(const char * key) { return find<false, assocarr<T>::construct_none>(key, NULL); }
	
	static void construct(void* loc, T* obj) { new(loc) T(); }
	struct node * find_or_create(const char * key) { return find<true, assocarr<T>::construct>(key, NULL); }
	
	static void construct_copy(void* loc, T* obj) { if (obj) new(loc) T(*obj); else new(loc) T(); }
	struct node * find_or_copy(const char * key, T* item) { return find<true, assocarr<T>::construct_copy>(key, item); }
	
public:
	unsigned int count() { return this->entries; }
	
	bool has(const char * key) { return find_if_exists(key); }
	T& get(const char * key) { return find_or_create(key)->value; }
	
	T* get_ptr(const char * key)
	{
		struct node * ret=find_if_exists(key);
		if (ret) return &ret->value;
		else return NULL;
	}
	
	void set(const char * key, T& value) { find_or_create(key, &value); }
	
	void remove(const char * key)
	{
		struct node * removal=find<false, assocarr<T>::construct_none>(key, NULL);
		if (!removal) return;
		struct node * next=removal->next;
		free(removal->key);
		removal->value->~T();
		free(removal->value);
		*removal=*next;
		free(next);
		this->entries--;
		if (this->buckets>4 && this->entries < this->buckets/2) resize(this->buckets/2);
	}
	
	assocarr()
	{
		this->buckets=0;
		resize(4);
	}
	
	~assocarr()
	{
		for (unsigned int i=0;i<this->buckets;i++)
		{
			struct node * item=this->nodes[i];
			while (item->key)
			{
				struct node * next=item->next;
				free(item->key);
				item->value.~T();
				free(item);
				item=next;
			}
		}
	}
};



class config : private nocopy {
private:
	assocarr<assocarr<string> > items;
	assocarr<string>* group;
public:
	bool set_group(const char * group)
	{
		this->group=this->items.get_ptr(group ? group : "");
		return group;
	}
	
	bool read(const char * item, const char * & value)
	{
		if (!this->group) return false;
		string* ret=this->group->get_ptr(item);
		if (!ret) return false;
		value=*ret;
		return true;
	}
	
	bool read(const char * item, unsigned int& value)
	{
		if (!this->group) return false;
		string* str=this->group->get_ptr(item);
		if (!str) return false;
		char* end;
		unsigned int ret=strtoul(*str, &end, 10);
		if (end) return false;
		value=ret;
		return true;
	}
	
	bool read(const char * item, float& value)
	{
		if (!this->group) return false;
		string* str=this->group->get_ptr(item);
		if (!str) return false;
		char* end;
		float ret=strtod(*str, &end);
		if (end) return false;
		value=ret;
		return true;
	}
	
	bool read(const char * item, bool& value)
	{
		if (!this->group) return false;
		string* str=this->group->get_ptr(item);
		if (!str) return false;
		if(0);
		else if (!strcmp(*str, "1") || !strcasecmp(*str, "true")) value=true;
		else if (!strcmp(*str, "0") || !strcasecmp(*str, "false")) value=false;
		else return false;
		return true;
	}
	
	operator bool()
	{
		//if the object is valid, there is a global namespace (though it may be empty)
		return (this->items.count()!=0);
	}
	
protected:
	config(const char * data);
	//ignore destructor - we need the automatic one, but nothing explicit.
};
