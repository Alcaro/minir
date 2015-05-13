#pragma once
#include "global.h" // stdlib.h is enough, but I want to ensure the malloc seatbelt is enabled.
#include "array.h" // some functions return array<string>
#include <string.h> // memcpy
#include <ctype.h> // tolower

class string;
typedef const string & cstring;
typedef array<string> stringlist;

//static inline size_t strlen(cstring str);

class string {
private:
	char * data;
	
	static char* clone(const char * str) { return strdup(str ? str : ""); }
	
public:
	string() { this->data = clone(NULL); }
	string(const char * str) { this->data = strdup(str); }
	string(const string& other) { this->data = strdup(other.data); }
#ifdef HAVE_MOVE
	string(string&& other) { this->data = other.data; other.data = NULL; }
#endif
	~string() { free(this->data); }
	string& operator=(const char * str) { free(this->data); this->data=clone(str); return *this; }
	string& operator=(string other) // copying as the argument can sometimes avoid copying entirely
	{
		free(this->data);
		this->data = other.data;
		other.data = NULL;
		return *this;
	}
	
	string& operator+=(const char * str)
	{
		if (!str) return *this;
		char* newstr = malloc(strlen(this->data)+strlen(str)+1);
		strcpy(newstr, this->data); strcat(newstr, str);
		free(this->data);
		this->data = newstr;
		return *this;
	}
	
	string& operator+=(char * str) { return this->operator+=((const char*)str); }
	string& operator+=(cstring other) { return this->operator+=((const char*)other); }
	
	string operator+(const char * other) const { string ret(*this); ret+=other; return ret; }
	
	string operator+(char * other) const { string ret(*this); ret+=other; return ret; }
	string operator+(cstring other) const { string ret(*this); ret+=other; return ret; }
	
	bool operator==(const char * other) const { return (!strcmp(this->data, other)); }
	bool operator==(const string& other) const { return (!strcmp(this->data, other.data)); }
	
	char& operator[](size_t index) const { return this->data[index]; }
	
	operator char * () const { return this->data; }
	
	size_t len() const { return strlen(this->data); }
	operator bool() const { return (*this->data); }
	
	
	size_t hash() const
	{
		const char * str = data;
		size_t ret = 0;
		while (*str)
		{
			ret *= 101;
			ret += *str;
			str++;
		}
		return ret;
	}
	
	string lower() const
	{
		string ret = *this;
		for (size_t i=0;ret[i];i++) { ret[i]=tolower(ret[i]); }
		return ret;
	}
};
static inline void strlen(cstring){}//don't do this - use .len()
//I can't force use to give an error, but strlen() is expected to return a value, and using a void will throw.
#define S (string)

inline string to_string(int n)
{
	char out[16];
	sprintf(out, "%i", n);
	return out;
}
inline string to_string(const char * str) { return str; }
#define string(x) to_string(x)
