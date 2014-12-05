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
		bool used;
		T value;
	};
	struct node_t * * nodes;
	unsigned int buckets;
	unsigned int entries;
	unsigned int used_entries;
	
	void resize(unsigned int newbuckets)
	{
		struct node_t * * newnodes=malloc(sizeof(struct node_t*)*newbuckets);
		memset(newnodes, 0, sizeof(struct node_t*)*newbuckets);
		for (unsigned int i=0;i<this->buckets;i++)
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
			if (node[0]->hash==thehash && !strcmp(key, node[0]->key))
			{
				if (!node[0]->used) this->used_entries++;
				node[0]->used=true;
				return node;
			}
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
		node->used=false;
		node->next=this->nodes[thehash%this->buckets];
		this->nodes[thehash%this->buckets]=node;
		this->entries++;
		if (this->entries > this->buckets) resize(this->buckets*2);
		return node;
	}
	
public:
	unsigned int size() { return this->entries; }
	unsigned int size(unsigned int * used) { *used=this->used_entries; return this->entries; }
	
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
		this->used_entries=0;
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
	
	void each(function<void(const char * key, T& value, bool& used)> iter)
	{
		for (unsigned int i=0;i<this->buckets;i++)
		{
			struct node_t * node=this->nodes[i];
			while (node)
			{
				struct node_t * next=node->next;
				iter(node->key, node->value, node->used);
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



class config : private nocopy {
protected:
	assocarr<assocarr<string> > items;
	assocarr<string>* group;
	
	void parse(char * data);
	
	
	bool parse(const char * str, const char * * out)
	{
		*out=str;
		return true;
	}
	
	bool parse(const char * str, unsigned int* value)
	{
		char* end;
		unsigned int ret=strtoul(str, &end, 10);
		if (*end) return false;
		*value=ret;
		return true;
	}
	
	bool parse(const char * str, signed int* value)
	{
		char* end;
		signed int ret=strtol(str, &end, 10);
		if (*end) return false;
		*value=ret;
		return true;
	}
	
	bool parse(const char * str, float* value)
	{
		char* end;
		float ret=strtod(str, &end);
		if (*end) return false;
		*value=ret;
		return true;
	}
	
	bool parse(const char * str, bool* value)
	{
		if(0);
		else if (!strcmp(str, "1") || !strcasecmp(str, "true"))  *value=true;
		else if (!strcmp(str, "0") || !strcasecmp(str, "false")) *value=false;
		else return false;
		return true;
	}
	
public:
	bool set_group(const char * group)
	{
		this->group=this->items.get_ptr(group ? group : "");
		return (this->group);
	}
	
	//If the requested item doesn't exist, this returns false and leaves 'error' unchanged.
	//If the requested item does exist but is not valid for that type, 'error' is set to true if non-NULL.
	//In all failure cases, 'value' remains unchanged.
	template<typename T> bool read(const char * item, T* value, bool * error=NULL)
	{
		if (!this->group) return false;
		string* ret=this->group->get_ptr(item);
		if (!ret) return false;
		if (!parse(*ret, value))
		{
			if (error) *error=true;
			return false;
		}
		return true;
	}
	
public://because the function class is stupid
	static void all_used_sub(void* ptr, const char * key, assocarr<string>& value, bool& used)
	{
		unsigned int child_used;
		unsigned int child_tot=value.size(&child_used);
		if (!used || child_used!=child_tot) *(bool*)ptr = false;
	}
public:
	bool all_used()
	{
		bool ret=true;
		this->items.each(bind_ptr(all_used_sub, &ret));
		return ret;
	}
	
	//This function will modify the given string.
	config(char * data) { parse(data); }
	//ignore destructor - we need the automatic one, but nothing explicit.
	
static void ggg(const char*a,string&b,bool&c){printf("  %s=%s (%i)\n",a,(const char*)b,c);}
static void gg(const char*a,assocarr<string>&b){unsigned int c;unsigned int d;c=b.size(&d);printf("%s (%i/%i):\n",a,d,c);b.each(bind(ggg));}
void g(){items.each(bind(gg));}
};
