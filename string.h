#pragma once
#include "global.h" // stdlib.h is enough, but I want to ensure the malloc seatbelt is enabled.

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
