#pragma once
#include "global.h" // stdlib.h is enough, but I want to ensure the malloc seatbelt is enabled.
#include <string.h> // memcpy

#ifndef NEW_STRING
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


#else


class string;
typedef const string & cstring;

//static inline size_t strlen(cstring str);

class string {
public:
	//Variable naming convention:
	//char*:
	// utf - Valid UTF-8.
	// bytes - Unvalidated UTF-8.
	//size_t:
	// nbyte - Byte count of possibly invalid data.
	// codepoints - Number of code points. No normalization is done.
	//Any of the above may be suffixed with anything.
	//Anything else, in particular 'ptr', 'str', 'data' and 'len', may not be used.
	
	static unsigned int utf8cplen(uint32_t codepoint)
	{
		//if (codepoint==0) assert();
		if (codepoint<0x80) return 1;
		if (codepoint<0x800) return 2;
		if (codepoint<0x10000) return 3;
		if (codepoint<0x200000) return 4;
		if (codepoint<0x4000000) return 5;
		if (codepoint<0x80000000) return 6;
		return 7;//this won't happen, utf8readcp can't return len=7
	}
	
	static unsigned int utf8firstlen(uint8_t byte)
	{
		if(0);
		else if (byte<0x80) return 1;
		else if (byte<0xC0) return 0;
		else if (byte<0xE0) return 2;
		else if (byte<0xF0) return 3;
		else if (byte<0xF8) return 4;
		else if (byte<0xFC) return 5;
		else if (byte<0xFE) return 6;
		else return 0;
	}
	
	//'valid' remains unchanged on success, and becomes false on failure. Failure returns U+FFFD.
	//However, U+FFFD is a valid answer if the input is EF BF FD.
	//'bytes' is incremented as appropriate.
	static uint32_t utf8readcp(const char * & bytes, bool * valid)
	{
		unsigned int len=utf8firstlen(*bytes);
		if (len==0) goto error;
		if (len==1) return *(bytes++);
		
		uint32_t ret;
		ret = (bytes[0] & (0x7F>>len));
		for (unsigned int i=1;i<len;i++)
		{
			uint8_t next=bytes[i];
			if ((next&0xC0)!=0x80) goto error;
			ret = (ret<<6) | (next&0x3F);
		}
		
		if (utf8cplen(ret)!=len) goto error;
		bytes+=len;
		return ret;
		
	error:
		bytes+=1;
		if (valid) *valid=false;
		return 0xFFFD;
	}
	
	static void utf8writecp(char * & bytes, uint32_t codepoint)
	{
//printf("cp=%X ",codepoint);
		unsigned int nbyte=utf8cplen(codepoint);
//printf("bytes=%i ",nbyte);
		if (nbyte==1)
		{
			*(bytes++)=codepoint;
			return;
		}
		
		for (unsigned int i=nbyte;i>1;)
		{
			i--;
			bytes[i] = 0x80 | (codepoint&0x3F);
//printf("byte[%i]=%.2X ",i,bytes[i]);
			codepoint >>= 6;
//printf("remain=%X ",codepoint);
		}
		bytes[0] = (0xFF << (8-nbyte)) | codepoint;
//printf("byte[0]=%.2X\n",bytes[0]);
		bytes+=nbyte;
	}
	
	static bool utf8validate(const char * bytes, size_t * nbyte_cleaned)
	{
		bool ok=true;
		size_t nbyte_ret=0;
		//the optimizer probably flattens this into not actually calculating the code point
		while (*bytes) nbyte_ret+=utf8cplen(utf8readcp(bytes, &ok));
		if (nbyte_cleaned) *nbyte_cleaned=nbyte_ret;
		return ok;
	}
	
	static char * utf8dup(const char * bytes, bool * valid=NULL)
	{
		size_t nbyte_alloc;
		bool is_valid=utf8validate(bytes, &nbyte_alloc);
		if (valid) *valid=is_valid;
		
		char * utfret=malloc(nbyte_out);
		if (is_valid) memcpy(utfret, bytes, nbyte_alloc);
		else
		{
			char * utfret_w=utfret;
			while (*bytes) utf8writecp(utfret_w, utf8readcp(bytes, NULL));
			*utfret_w='\0';
		}
		return utfret;
	}
	
	static size_t utf8len(const char * utf)
	{
		size_t codepoints=0;
		while (*utf)
		{
			if ((*utf & 0xC0) != 0xC0) codepoints++;
		}
		return codepoints;
	}
	
private:
	char * utf;
	size_t nbyte;
	
	//TODO: figure out which members are needed
	//UTF-8 length
	//buffer length
	//number of code points
	//last read position, in code points and bytes
	//refcount
	
	//ensures that the object has a valid state
	bool verify_self()
	{
		//if (!this->ptr) return false;
		//size_t bytelen=strlen(this->ptr);
		//if (bytelen!=this->bytelen) return false;
		
		//ptr!=NULL
		//can evaluate strlen(ptr)
		//strlen(ptr)==utf8 length
		//strlen(ptr) < buffer length
		//count code points
		//verify read position
	}
	
	
	
	
	
	
	void swap(string& other)
	{
		//char* tmp=other.ptr;
		//other.ptr=ptr;
		//ptr=tmp;
		//len=other.len;
	}
	
public:
	//string() { set(""); }
	//string(const char * newstr) { set(newstr); }
	//string(const string& newstr) { set(newstr.ptr); }
	//~string() { free(ptr); }
	//string& operator=(const char * newstr) { char* prev=ptr; set(newstr); free(prev); return *this; }
	//string& operator=(string newstr) { swap(newstr); return *this; } // my sources tell me this can sometimes avoid copying entirely
	//uint32_t operator[](int index) {}
	//string operator+(const char * right) {}
	//string operator+(cstring right) {}
	//string operator+(uint32_t right) {}
	//string& operator+=(const char * right) {}
	//string& operator+=(cstring right) {}
	//string& operator+=(uint32_t right) {}
	//bool operator==(const char * right) {}
	//bool operator==(cstring right) {}
	//operator const char * () { return ptr; }
	
	//friend inline size_t strlen(cstring str);
};

//static inline size_t strlen(cstring str) { return str.len; }
#endif
