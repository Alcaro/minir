#pragma once
#include "global.h" // stdlib.h is enough, but I want to ensure the malloc seatbelt is enabled.
#include <string.h> // memcpy

class string;
typedef const string & cstring;

static inline size_t strlen(cstring str);

class string {
private:
	char * ptr;
	size_t len;
	
	void set(const char * newstr, size_t len)
	{
		if (!newstr) newstr="";
		
		this->ptr=malloc(len+1);
		memcpy(this->ptr, newstr, len+1);
		this->len=len;
	}
	
	void set(const char * newstr) { set(newstr, strlen(newstr)); }
	
public:
	string() { set(NULL); }
	string(const char * newstr) { set(newstr); }
	string(const string& newstr) { set(newstr.ptr); }
	~string() { free(ptr); }
	string& operator=(const char * newstr) { char* prev=ptr; set(newstr); free(prev); return *this; }
	string& operator=(string newstr) { char* tmp=newstr.ptr; newstr.ptr=ptr; ptr=tmp; return *this; } // my sources tell me this can sometimes avoid copying entirely
	operator const char * () { return ptr; }
	
	friend inline size_t strlen(cstring str);
};

static inline size_t strlen(cstring str) { return str.len; }



#if 0
class string;
typedef const string & cstring;

//static inline size_t strlen(cstring str);

class string {
private:
	char * ptr;
	//TODO: figure out which members are needed
	//UTF-8 length
	//buffer length
	//number of code points
	//last read position, in code points and bytes
	//refcount
	
	//ensures that the object has a valid state
	void verify()
	{
		//ptr!=NULL
		//can evaluate strlen(ptr)
		//strlen(ptr)==utf8 length
		//strlen(ptr) < buffer length
		//count code points
		//verify read position
	}
	
	static size_t utf8strnlen(const char * data, size_t len)
	{
		
	}
	
	static char * utf8strndup(const char * data, size_t len)
	{
		
	}
	
	static char * utf8strindex(const char * data, size_t i)
	{
		
	}
	
	void set(const char * newstr, size_t len)
	{
		if (!newstr) newstr="";
		
		this->ptr=malloc(len+1);
		memcpy(this->ptr, newstr, len+1);
		this->len=len;
	}
	
	void set(const char * newstr) { set(newstr, strlen(newstr)); }
	
	void swap(string& other)
	{
		char* tmp=other.ptr;
		other.ptr=ptr;
		ptr=tmp;
		len=other.len;
	}
	
public:
	string() { set(""); }
	string(const char * newstr) { set(newstr); }
	string(const string& newstr) { set(newstr.ptr); }
	~string() { free(ptr); }
	string& operator=(const char * newstr) { char* prev=ptr; set(newstr); free(prev); return *this; }
	string& operator=(string newstr) { swap(newstr); return *this; } // my sources tell me this can sometimes avoid copying entirely
	uint32_t operator[](int index) {}
	string operator+(const char * right) {}
	string operator+(cstring right) {}
	string operator+=(const char * right) {}
	string operator+=(cstring right) {}
	bool operator==(const char * right) {}
	bool operator==(cstring right) {}
	operator const char * () { return ptr; }
	
	//friend inline size_t strlen(cstring str);
};

//static inline size_t strlen(cstring str) { return str.len; }
#endif
