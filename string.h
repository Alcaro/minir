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
			(*codepoints)++;
		}
		*nbyte_cleaned=nbyte_ret;
		return state;
	}
	
	static void utf8copy(char * utf_dst, const char * bytes_src)
	{
		while (*bytes_src) utf8writecp(utf_dst, utf8readcp(bytes_src, NULL));
		*utf_dst='\0';
	}
	
	static char * utf8dup(const char * bytes, size_t * codepoints, size_t * nbyte, uint8_t * state)
	{
		size_t nbyte_alloc;
		uint8_t in_state=utf8validate(bytes, codepoints, &nbyte_alloc);
		if (state) *state=in_state;
		
		if (nbyte) *nbyte=nbyte_alloc;
		char * utfret=malloc(bitround(nbyte_alloc+1));
		if (in_state!=st_invalid) memcpy(utfret, bytes, nbyte_alloc+1);
		else utf8copy(utfret, bytes);
		return utfret;
	}
	
	static char * utf8append(char * utf, size_t nbyte_utf,
	                         const char * bytes, size_t * codepoints, size_t * nbyte, uint8_t * state)
	{
		size_t nbyte_new;
		uint8_t in_state=utf8validate(bytes, codepoints, &nbyte_new);
		if (state) *state=in_state;
		
		size_t nbyte_alloc = nbyte_utf + nbyte_new;
		
		if (nbyte) *nbyte=nbyte_alloc;
		if (bitround(nbyte_alloc+1) != bitround(nbyte_utf+1)) utf=realloc(utf, bitround(nbyte_alloc+1));
		if (in_state!=st_invalid) memcpy(utf+nbyte_utf, bytes, nbyte_new+1);
		else utf8copy(utf+nbyte_utf, bytes);
		return utf;
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
		if (!bytes) bytes="";
		this->state=st_ascii;
		size_t codepoints;
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
	
	void reserve_bytes(size_t newbytes)
	{
		size_t insize=bitround(this->nbyte+1);
		size_t outsize=bitround(this->nbyte + newbytes + 1);
		if (insize!=outsize) this->utf=realloc(this->utf, outsize);
	}
	
	size_t find_nbyte_for_codepoint(size_t index) const
	{
		if (!(this->state & st_utf8)) return index;
		const char * utf=this->utf;
		while (index)
		{
			if (!*utf) return (size_t)-1;
			utf+=utf8firstlen(*utf);
			index--;
		}
		return (utf - this->utf);
	}
	
	uint32_t char_at(size_t index) const
	{
		if (!(this->state & st_utf8)) return this->utf[index];
		size_t nbyte=find_nbyte_for_codepoint(index);
		
		if (nbyte==(size_t)-1) return 0xFFFD;
		
		const char * utf=this->utf + nbyte;
		return utf8readcp(utf, NULL);
	}
	
	void set_char_at(size_t index, uint32_t val)
	{
		if (val==0x00)
		{
			val=0xFFFD;
			this->state=st_invalid;
		}
		if (!(this->state & st_utf8) && val<=0x7F)
		{
			this->utf[index]=val;
			return;
		}
		
		size_t nbyte=find_nbyte_for_codepoint(index);
		if (nbyte==(size_t)-1)
		{
			this->state=st_invalid;
			return;
		}
		
		char * utf=this->utf + nbyte;
		unsigned int nbyte_now=utf8firstlen(*utf);
		unsigned int nbyte_new=utf8cplen(val);
		if (nbyte_now!=nbyte_new)
		{
			if (nbyte_new > nbyte_now)
			{
				reserve_bytes(nbyte_new);
				utf = this->utf + nbyte;
			}
			memmove(this->utf+nbyte+nbyte_new, this->utf+nbyte+nbyte_now, this->nbyte-nbyte-nbyte_now+1);
		}
		utf8writecp(utf, val);
	}
	
public:
	//static string from_us_ascii(const char * bytes) {}
	
	string() { set_to_bytes(NULL); }
	string(const char * bytes) { set_to_bytes(bytes); }
	string(const string& other) { set_to_str_clone(other); }
#ifdef HAVE_MOVE
	string(string&& other) { set_to_str_consume(other); }
#endif
	~string() { free(utf); }
	string& operator=(const char * bytes) { free(this->utf); set_to_bytes(bytes); return *this; }
	string& operator=(string other) // copying as the argument can sometimes avoid copying entirely
	{
		free(this->utf);
		set_to_str_consume(other);
		return *this;
	}
	
	string& operator+=(const char * bytes)
	{
		size_t codepoints;
		this->utf=utf8append(this->utf, this->nbyte, bytes, &codepoints, &this->nbyte, &this->state);
		codepoints += this->len_codepoints;
		if (codepoints>65535) codepoints=65535;
		this->len_codepoints=codepoints;
		return *this;
	}
	
	string& operator+=(cstring other)
	{
		reserve_bytes(other.nbyte);
		
		memcpy(this->utf + this->nbyte, other.utf, other.nbyte+1);
		this->nbyte += other.nbyte;
		this->state |= other.state;
		if (this->len_codepoints + other.len_codepoints < 0x10000) this->len_codepoints += other.len_codepoints;
		else this->len_codepoints = 0xFFFF;
		
		return *this;
	}
	
	string& operator+=(uint32_t other)
	{
		if (other==0x0000) { other=0xFFFD; this->state=st_invalid; }
		if (other >= 0x80) this->state |= st_utf8;
		
		size_t newbytes=utf8cplen(other);
		reserve_bytes(newbytes);
		char * tmp = this->utf + this->nbyte;
		utf8writecp(tmp, other);
		
		this->nbyte += newbytes;
		if (this->len_codepoints != 0xFFFF) this->len_codepoints++;
		return *this;
	}
	
	string operator+(const char * other) const { string ret(*this); ret+=other; return ret; }
	string operator+(cstring other) const { string ret(*this); ret+=other; return ret; }
	string operator+(uint32_t other) const { string ret(*this); ret+=other; return ret; }
	
	//this can get non-utf, but non-utf is never equal to valid utf
	//this lets (string)"\x80" != "\x80", but comparing invalid strings is stupid anyways
	bool operator==(const char * other) const { return (!strcmp(this->utf, other)); }
	bool operator==(cstring other) const { return (!strcmp(this->utf, other.utf)); }
	
	uint32_t operator[](size_t index) const { return char_at(index); }
	//TODO: operator[] that returns a fancy object that calls char_at from operator uint32_t, or set_char_at from operator =
	
	operator const char * () const { return utf; }
	
	size_t len()
	{
		if (this->len_codepoints == 65535) return utf8len(this->utf+this->readpos_nbyte)+this->readpos_codepoints;
		else return this->len_codepoints;
	}
	operator bool() const { return (*this->utf); }
	
	stringlist split(const char * sep) const
	{
		stringlist ret;
		char * at=this->utf;
		char * next=strstr(at, sep);
		while (next)
		{
			char tmp=*next;
			*next='\0';
			ret.append(at);
			*next=tmp;
			at=next+strlen(sep);
			next=strstr(at, sep);
		}
		ret.append(at);
		return ret;
	}
	
	stringlist split(uint32_t sep) const
	{
		if (sep>=0x80)
		{
			char sep_s[6];
			char * sep_s_p=sep_s;
			utf8writecp(sep_s_p, sep);
			*sep_s_p='\0';
			return split(sep_s);
		}
		
		stringlist ret;
		char * at=this->utf;
		char * next=strchr(at, sep);
		while (next)
		{
			char tmp=*next;
			*next='\0';
			ret.append(at);
			*next=tmp;
			at=next+utf8cplen(sep);
			next=strchr(at, sep);
		}
		ret.append(at);
		return ret;
	}
	
	string replace(const char * bytes_from, const char * bytes_to) const
	{
		char * start=this->utf;
		char * next=strstr(start, bytes_from);
		if (!next) return *this;
		
		string out;
		string to=bytes_to;
		while (next)
		{
			char tmp=*next;
			*next='\0';
			out+=start;
			out+=to;
			*next=tmp;
			start=next+strlen(bytes_from);
			next=strstr(start, bytes_from);
		}
		out+=start;
		return out;
	}
	
	bool valid() { return (this->state != st_invalid); }
	
	bool contains(const char * other) { return (strstr(this->utf, other)); }
};
static inline void strlen(cstring){}//don't do this - use .len()
//I can't force use to give an error, but strlen() is expected to return a value, and using a void will throw.
#define S (string)
