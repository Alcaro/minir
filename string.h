#pragma once
#include "global.h" // stdlib.h is enough, but I want to ensure the malloc seatbelt is enabled.
#include "containers.h" // some functions return array<string>
#include <string.h> // memcpy

class string;
typedef const string & cstring;
typedef array<string> stringlist;

//static inline size_t strlen(cstring str);

class string {
private:
	//Variable naming convention:
	//char*:
	// utf - Valid UTF-8.
	// bytes - Unvalidated UTF-8, or sometimes another charset.
	//size_t:
	// nbyte - Byte count of possibly invalid data.
	// codepoints - Number of code points. No normalization is done.
	//Any of the above may be suffixed with anything.
	//Anything else, in particular 'ptr', 'str', 'data' and 'len', may not be used.
	//Other types may have any name.
	
	//Pure UTF-8 is beautiful and simple. However, the official UTF-8 is dirtied with various workarounds for other things (for example UTF-16) being shitty.
	//Therefore:
	//- RFC3629's restriction to the UTF-16 compatible range is ignored. The upper limit is U+8000_0000.
	//- The UTF-16 reserved range (U+D800..U+DFFF) is considered to be valid characters.
	//- Invalid sequences of high-bit characters are replaced with one or more U+FFFD. It is not specified exactly how many.
	//- U+0000 NUL is rejected. If encountered, it's replaced with U+FFFD.
	
	enum {
		//Don't change the values - there's a pile of bit math going on.
		st_ascii=0,   // Contains only ASCII-compatible data.
		st_utf8=1,    // Has contained at least one non-ASCII character. Not guaranteed to still contain any non-ASCII.
		st_invalid=3, // Was created from an invalid byte sequence. Contains or has contained one or more U+FFFD.
	};
	
	static unsigned int utf8cplen(uint32_t codepoint)
	{
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
		else if (byte<0xFC) return 5;//screw RFC3629, this is valid
		else if (byte<0xFE) return 6;
		else return 0;
	}
	
	//Bits in 'state' are set as needed, never cleared. Failure returns U+FFFD.
	//However, U+FFFD is a valid answer if the input is EF BF FD.
	//'bytes' is incremented as appropriate.
	static uint32_t utf8readcp(const char * & bytes, uint8_t * state)
	{
		unsigned int len=utf8firstlen(*bytes);
		if (len==0) goto error;
		if (len==1) return *(bytes++);
		
		if (state) *state |= st_utf8;
		
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
		if (state) *state = st_invalid;
		return 0xFFFD;
	}
	
	static void utf8writecp(char * & bytes, uint32_t codepoint)
	{
		unsigned int nbyte=utf8cplen(codepoint);
		if (nbyte==1)
		{
			*(bytes++)=codepoint;
			return;
		}
		
		for (unsigned int i=nbyte;i>1;)
		{
			i--;
			bytes[i] = 0x80 | (codepoint&0x3F);
			codepoint >>= 6;
		}
		bytes[0] = (0xFF << (8-nbyte)) | codepoint;
		bytes+=nbyte;
	}
	
	static uint8_t utf8validate(const char * bytes, size_t * codepoints, size_t * nbyte_cleaned)
	{
		uint8_t state=st_ascii;
		size_t nbyte_ret=0;
		//the optimizer probably flattens this into not actually calculating the code point
		*codepoints=0;
		while (*bytes)
		{
			nbyte_ret+=utf8cplen(utf8readcp(bytes, &state));
			*codepoints++;
		}
		*nbyte_cleaned=nbyte_ret;
		return state;
	}
	
	static char * utf8dup(const char * bytes, size_t * codepoints, size_t * nbyte, uint8_t * state)
	{
		size_t nbyte_alloc;
		uint8_t in_state=utf8validate(bytes, codepoints, &nbyte_alloc);
		if (state) *state=in_state;
		
		if (nbyte) *nbyte=nbyte_alloc;
		char * utfret=malloc(bitround(nbyte_alloc+1));
		if (in_state!=st_invalid) memcpy(utfret, bytes, nbyte_alloc+1);
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
	
	//this can either be UTF-8 or not UTF-8, it works for both; therefore, the argument is misnamed
	static char* rounddup(const char * str)
	{
		size_t len=strlen(str);
		char* ret=malloc(bitround(len+1));
		memcpy(ret, str, len+1);
		return ret;
	}
	
private:
	char * utf;
	size_t nbyte; //utf[nbyte] is guaranteed '\0'. Buffer length is bitround(nbyte+1).
	uint16_t len_codepoints;//65535 means "too long".
	uint8_t state;
	//char padding[1];
	volatile uint16_t readpos_nbyte;
	volatile uint16_t readpos_codepoints;//If either value would go above 65535, neither is written.
	
	//TODO: figure out which members are needed
	//1 UTF-8 length (bytes)
	//0 buffer length (bytes)
	//1 string length (code points)
	//1 last read position, in code points and bytes (may be static - but wipe the cache on change of any string, including destruct)
	//1 is US-ASCII flag
	//1 is valid flag
	//0 refcount
	
	void set_to_bytes(const char * bytes)
	{
		size_t codepoints;
		this->state=st_ascii;
		this->utf=utf8dup(bytes, &codepoints, &this->nbyte, &this->state);
		if (codepoints>65535) codepoints=65535;
		this->len_codepoints=codepoints;
		this->readpos_nbyte=0;
		this->readpos_codepoints=0;
	}
	
	void set_to_str_clone(cstring other)
	{
		this->utf=rounddup(other.utf);
		this->nbyte=other.nbyte;
		this->len_codepoints=other.len_codepoints;
		this->state=other.state;
		this->readpos_nbyte=other.readpos_nbyte;
		this->readpos_codepoints=other.readpos_codepoints;
	}
	
	void set_to_str_consume(string& other)
	{
		this->utf=other.utf;
		other.utf=NULL;
		this->nbyte=other.nbyte;
		this->len_codepoints=other.len_codepoints;
		this->state=other.state;
		this->readpos_nbyte=other.readpos_nbyte;
		this->readpos_codepoints=other.readpos_codepoints;
	}
	
	//void append_bytes(const char * bytes)
	//void append_str(cstring other)
	
	uint32_t char_at(size_t index) const
	{
		if (!(this->state & st_utf8)) return this->utf[index];
		//TODO
	}
	
	void set_char_at(size_t index, uint32_t val)
	{
		if (val==0x00) val=0xFFFD;
		if (!(this->state & st_utf8) && !(val&~0x7F))
		{
			this->utf[index]=val;
			return;
		}
		//TODO
	}
	
public:
	//static string from_us_ascii(const char * bytes) {}
	
	string() { set_to_bytes(""); }
	string(const char * bytes) { set_to_bytes(bytes); }
	string(const string& other) { set_to_str_clone(other); }
	~string() { free(utf); }
	string& operator=(const char * bytes) { free(utf); set_to_bytes(bytes); }
	string& operator=(string other) { set_to_str_consume(other); } // copying as the argument can sometimes avoid copying entirely
	//string operator+(const char * bytes) const {}
	//string operator+(cstring other) const {}
	//string operator+(uint32_t other) const {}
	//string& operator+=(const char * bytes) {}
	//string& operator+=(cstring other) {}
	//string& operator+=(uint32_t other) {}
	
	//this can get non-utf, but non-utf is never equal to valid utf
	//this lets "\x80" != (string)"\x80", but comparing invalid strings is an invalid operation anyways
	bool operator==(const char * other) const { return (!strcmp(this->utf, other)); }
	bool operator==(cstring other) const { return (!strcmp(this->utf, other.utf)); }
	
	uint32_t operator[](size_t index) const { return char_at(index); }
	//TODO: operator[] that returns a fancy object that calls char_at from operator uint32_t, or set_char_at from operator =
	
	//TODO: remove
	operator const char * () const { return utf; }
	
	size_t len()
	{
		if (this->len_codepoints == 65535) return utf8len(this->utf+this->readpos_nbyte)+this->readpos_codepoints;
		else return this->len_codepoints;
	}
	
	bool valid() { return (this->state != st_invalid); }
};
static inline void strlen(cstring){}//don't do this - use .len()
//I can't force use to give an error, but strlen() is expected to return a value, and using a void will throw.
