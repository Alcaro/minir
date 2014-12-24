#pragma once
#include "containers.h"

//GCC bug #nnnnnn makes this far less flexible than it should be
/*
namespace { inline void assocarr_mem_iter(bool* ret, const char * key, void*& ignored, bool& used) { if (!used) *ret=false; } }
template<typename T> class assocarr_mem {
	class child {
	public:
		T item;
		bool used;
		
		child() : used(false) {}
		child(const T& copy) : item(copy), used(false) {}
	};
	assocarr<child> items;
	friend class assocarr_mem_helper;
	
public:
	unsigned int size() { return items.size(); }
	bool has(const char * key) { return items.has(key); }
	
	T& get(const char * key)
	{
		child& ret=items.get(key);
		ret.used=true;
		return ret.item;
	}
	
	T* get_ptr(const char * key)
	{
		child* ret=items.get_ptr(key);
		if (ret)
		{
			ret->used=true;
			return &ret->item;
		}
		else return NULL;
	}
	
	void set(const char * key, const T& value)
	{
		items.set(key, value);
	}
	
	void remove(const char * key)
	{
		items.remove(key);
	}
	
	void reset()
	{
		items.reset();
	}
	
	
	//there is some rather nasty code around here; this is due to GCC bug #nnnnnn, which blocks the proper solution
	//void each(function<void(const char * key, T& value)> iter)
	//{
	//	class workaround {
	//	public:
	//		static void iter_sub(function<void(const char * key, T& value)>* iter, const char * key, child& value)
	//		{
	//			(*iter)(key, value.item);
	//		}
	//	};
	//	bind_ptr(workaround::iter_sub, &iter);
	//}
	//void each(function<void(const char * key, T& value, bool& used)> iter)
	//{
	//	class workaround {
	//	public:
	//		static void iter_sub(function<void(const char * key, T& value, bool& used)>* iter, const char * key, child& value)
	//		{
	//			(*iter)(key, value.item, value.used);
	//		}
	//	};
	//	bind_ptr(workaround::iter_sub, &iter);
	//	//assocarr_mem_helper::iter_used<T>(iter);
	//}
	
	bool all_used()
	{
		bool ret=true;
		this->items.each(bind_ptr((void(*)(bool*,const char*,child&,bool&))assocarr_mem_iter, &ret));
		return ret;
	}
};

//template<typename T, typename Tchild> static void assocarr_mem_helper::iter_sub_used(function<void(const char * key, T& value, bool& used)>* iter, const char * key, Tchild& value)
//{
//	(*iter)(key, value.item, value.used);
//}
//template<typename T> void assocarr_mem_helper::iter_used(function<void(const char * key, T& value, bool& used)> func)
//{
//	(bind_ptr(&assocarr_mem_helper::iter_sub_used<T, assocarr_mem<T>::child>, &func));
//}
*/

#include "string.h"
class configreader : private nocopy {
protected:
	class sub_inner {
	public:
		string item;
		bool used;
		
		sub_inner() : used(false) {}
		sub_inner(string copy) : item(copy), used(false) {}
		sub_inner(const char * copy) : item(copy), used(false) {}
	};
	class sub_outer {
	public:
		assocarr<sub_inner> items;
		bool used;
		unsigned int children_used;
		
		sub_outer() : used(false), children_used(0) {}
	};
	assocarr<sub_outer> items;
	sub_outer* group;
	
	
	static bool parse_var(const char * str, const char * * out)
	{
		*out=str;
		return true;
	}
	
	static bool parse_var(const char * str, unsigned int* value)
	{
		char* end;
		unsigned int ret=strtoul(str, &end, 10);
		if (*end) return false;
		*value=ret;
		return true;
	}
	
	static bool parse_var(const char * str, signed int* value)
	{
		char* end;
		signed int ret=strtol(str, &end, 10);
		if (*end) return false;
		*value=ret;
		return true;
	}
	
	static bool parse_var(const char * str, float* value)
	{
		char* end;
		float ret=strtod(str, &end);
		if (*end) return false;
		*value=ret;
		return true;
	}
	
	static bool parse_var(const char * str, bool* value)
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
		if (!this->group) return false;
		this->group->used=true;
		return true;
	}
	
	//If the requested item doesn't exist, this returns false and leaves 'error' unchanged.
	//If the requested item does exist but is not valid for that type, 'error' is set to true if non-NULL.
	//In all failure cases, 'value' remains unchanged.
	template<typename T> bool read(const char * item, T* value, bool * error=NULL)
	{
		if (!this->group) return false;
		sub_inner* ret=this->group->items.get_ptr(item);
		if (!ret) return false;
		if (!ret->used) this->group->children_used++;
		ret->used=true;
		if (!parse_var(ret->item, value))
		{
			if (error) *error=true;
			return false;
		}
		return true;
	}
	
	bool all_used()
	{
		class sub {
		public:
			static void iter(bool* ret, const char * left, sub_outer& right)
			{
				if (!right.used || right.children_used != right.items.size()) *ret=false;
			}
		};
		bool ret=true;
		this->items.each(bind_ptr(sub::iter, &ret));
		return ret;
	}
	
	//This function will modify the given string.
	bool parse(char * data);
	
	configreader(char * data) { parse(data); }
	configreader() {}
	//ignore destructor - we need the automatic one, but nothing explicit.
};
