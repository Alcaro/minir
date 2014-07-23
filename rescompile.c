#ifdef RESCOMPILE
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

//TODO: make this do something valid if it encounters a cross compiler
//#ifdef _WIN32
//#define WIN 1
//#else
//#define WIN 0
//#endif
//#ifdef __linux__
//#define LIN 1
//#else
//#define LIN 0
//#endif

//TODO: platform-specific things
//remember the comment coalescing cache, it could screw stuff up

//TODO: use bytecode instead of clear_defaults


enum {
	CFGB_END,
	
	CFGB_COMMENT,
	CFGB_LINEBREAK,//Linebreaks that appear only in the root node are in comments.
	
	CFGB_GLOBAL,
	CFGB_ARRAY,
	CFGB_ARRAY_SHUFFLED,
	CFGB_ARRAY_SAME,
	
	CFGB_INPUT,
	CFGB_STR,
	CFGB_INT,
	CFGB_UINT,
	CFGB_ENUM,
	CFGB_BOOL,
};

//#define V

struct mem {
	void* ptr;
	size_t len;
	size_t buflen;
};

static mz_bool mem_append(const void* pBuf, int len, void* pUser)
{
	struct mem * m=pUser;
	if (m->len + len > m->buflen)
	{
		while (m->len + len > m->buflen) m->buflen*=2;
		m->ptr=realloc(m->ptr, m->buflen);
	}
	memcpy((char*)m->ptr + m->len, pBuf, len);
	m->len+=len;
	return true;
}

static void* compress(void* data, size_t inlen, size_t * outlen)
{
	struct mem m;
	m.buflen=1024;
	m.ptr=malloc(m.buflen);
	m.len=0;
	
	tdefl_compressor d;
	tdefl_init(&d, mem_append, &m, TDEFL_DEFAULT_MAX_PROBES);
	tdefl_compress_buffer(&d, data, inlen, TDEFL_FINISH);
	
	*outlen=m.len;
	return m.ptr;
}



void compileconfig(FILE * output)
{
	FILE * in=fopen("minir.cfg.tmpl", "rt");
	//FILE * out=stdout;
	
	enum {
		p_header_input, p_header_str, p_header_int,
		p_header_uint, p_header_enum, p_header_bool,
		p_header_override_input, p_header_override_str, p_header_override_int,
		p_header_override_uint, p_header_override_enum, p_header_override_bool,
		p_header_input_enum,
		p_clear_defaults_input, p_clear_defaults_str, p_clear_defaults_int,
		p_clear_defaults_uint, p_clear_defaults_enum, p_clear_defaults_bool,
		p_bytecode,
		p_last };
	
	struct mem outputs[p_last];
	char sprintfbuf[2048];
	
	for (unsigned int i=0;i<p_last;i++)
	{
		outputs[i].buflen=1024;
		outputs[i].ptr=malloc(outputs[i].buflen);
		outputs[i].len=0;
	}
	
#define emit_to(id, ...) do { mem_append(sprintfbuf, sprintf(sprintfbuf, __VA_ARGS__), &outputs[id]); } while(0)
	//fseek(in, 0, SEEK_SET);
#define emit_header_input(...) emit_to(p_header_input, __VA_ARGS__)
	emit_header_input("#ifdef CONFIG_HEADER\n");
#define emit_header_str(...) emit_to(p_header_str, __VA_ARGS__)
#define emit_header_int(...) emit_to(p_header_int, __VA_ARGS__)
#define emit_header_uint(...) emit_to(p_header_uint, __VA_ARGS__)
#define emit_header_enum(...) emit_to(p_header_enum, __VA_ARGS__)
#define emit_header_bool(...) emit_to(p_header_bool, __VA_ARGS__)
#define emit_header_this(...) emit_to(p_header_input+type_pass_id, __VA_ARGS__)
	
#define emit_header_override_input(...) emit_to(p_header_override_input, __VA_ARGS__)
#define emit_header_override_str(...) emit_to(p_header_override_str, __VA_ARGS__)
#define emit_header_override_int(...) emit_to(p_header_override_int, __VA_ARGS__)
#define emit_header_override_uint(...) emit_to(p_header_override_uint, __VA_ARGS__)
#define emit_header_override_enum(...) emit_to(p_header_override_enum, __VA_ARGS__)
#define emit_header_override_bool(...) emit_to(p_header_override_bool, __VA_ARGS__)
#define emit_header_override_this(...) emit_to(p_header_override_input+type_pass_id, __VA_ARGS__)
	
#define emit_header_input_enum(...) emit_to(p_header_input_enum, __VA_ARGS__)
	emit_header_input_enum("#ifdef CONFIG_HEADER_ENUM\n");
	
#define emit_clear_defaults_input(...) emit_to(p_clear_defaults_input, __VA_ARGS__)
	emit_clear_defaults_input("#ifdef CONFIG_CLEAR_DEFAULTS\n");
#define emit_clear_defaults_str(...) emit_to(p_clear_defaults_str, __VA_ARGS__)
#define emit_clear_defaults_int(...) emit_to(p_clear_defaults_int, __VA_ARGS__)
#define emit_clear_defaults_uint(...) emit_to(p_clear_defaults_uint, __VA_ARGS__)
#define emit_clear_defaults_enum(...) emit_to(p_clear_defaults_enum, __VA_ARGS__)
#define emit_clear_defaults_bool(...) emit_to(p_clear_defaults_bool, __VA_ARGS__)
#define emit_clear_defaults_this(...) emit_to(p_clear_defaults_input+type_pass_id, __VA_ARGS__)
	
	unsigned char bytecode[65536];
	int bytecodepos=0;
#define emit_bytecode(byte) do { bytecode[bytecodepos++]=byte; } while(0)
#define emit_bytecode_2(word) do { emit_bytecode(((word)>>8)&255); emit_bytecode(((word)>>0)&255); } while(0)
#define emit_bytecode_4(dword) do { emit_bytecode_2(((dword)>>16)&65535); emit_bytecode_2(((dword)>>0)&65535); } while(0)
	
	int numinputs=0;
	int numstrs=0;
	int numuints=0;
	int numints=0;
	int numenums=0;
	int numbools=0;
	int numoverrides=0;
	
	emit_header_input("union { struct {\n");
	emit_header_str("union { struct {\n");
	emit_header_uint("union { struct {\n");
	emit_header_int("union { struct {\n");
	emit_header_enum("union { struct {\n");
	emit_header_bool("union { struct {\n");
	emit_header_override_input("union { struct {\n");
	emit_header_override_input("union { struct {\n");
	emit_header_override_str("union { struct {\n");
	emit_header_override_uint("union { struct {\n");
	emit_header_override_int("union { struct {\n");
	emit_header_override_enum("union { struct {\n");
	emit_header_override_bool("union { struct {\n");
	
	char comment[8192];
	int commentlen=0;
	
	while (!feof(in))
	{
#define error(why) do { printf("%s: "why"\n", linecopy); exit(1); } while(0)
		char linebuf[1024];
		char linecopy[1024];
		char * line=linebuf;
		*line='\0';
		if(fgets(line, 1024, in)){};//shut up gcc, if I cast a return value to void it means I don't care.
		char * eol=strchr(line, '\n');
		if (eol) *eol='\0';
		strcpy(linecopy, line);
		
		while (*line==' ') line++;
		
		if (*line=='@')
		{
			//TODO: platform-specific stuff
		}
		
		if (*line=='#')
		{
			strcpy(comment+commentlen, line);
			commentlen+=strlen(line);
			strcpy(comment+commentlen, "\n");
			commentlen++;
			if (commentlen>4096) error("Too much comments, increase buffer size.");
		}
		else if (!strcmp(line, "-"))
		{
			strcpy(comment+commentlen, "\n");
			commentlen++;
		}
		else if (commentlen)
		{
			for (int i=0;i<commentlen;i++)
			{
				if (!(i%255))
				{
					int remaining=commentlen-i;
					emit_bytecode(CFGB_COMMENT);
					emit_bytecode(remaining>255?255:remaining);
				}
				if (comment[i]=='\r') {}
				else emit_bytecode(comment[i]);
			}
			
			commentlen=0;
		}
		
		if (!*line)
		{
			emit_bytecode(CFGB_LINEBREAK);
		}
		else if (*line=='#') {}//handled above
		else if (!strcmp(line, "-")) {}//handled above
		else if (!strncmp(line, "//", 2)) {}//should have an empty handler
		else if (islower(*line))
		{
			if (!strncmp(line, "global ", 7))
			{
				line+=7;
				emit_bytecode(CFGB_GLOBAL);
			}
			
			enum { num, str, flag, enumer } type;
			//number
				long long int rangelow=rangelow;//shut up about those too gcc, they're only used if type==num, and if it is, they're initialized.
				long long int rangehigh=rangehigh;
				enum { dec, hex } base=base;
			//string
				bool isinput=false;
			//flag
				//null
			//enumeration
				char enumtype[64];
				int numenumopts=numenumopts;
			
			char * typeend=line;
			while (islower(*typeend)) typeend++;
			if(0);
			else if (!strncmp(line, "int", typeend-line))
			{
				if (*typeend!='[') error("bad range");
				type=num;
				char * rangebegin=typeend+1;
				char * rangeend;
				if (rangebegin[1]=='x') base=hex;
				else base=dec;
				rangelow=strtoll(rangebegin, &rangeend, 0);
				if (rangebegin==rangeend || *rangeend!='-') error("bad range");
				rangebegin=rangeend+1;
				rangehigh=strtoll(rangebegin, &rangeend, 0);
				if (rangebegin==rangeend || *rangeend!=']') error("bad range");
				typeend=rangeend+1;
			}
			else if (!strncmp(line, "input", typeend-line))
			{
				type=str;
				isinput=true;
			}
			else if (!strncmp(line, "str", typeend-line))
			{
				type=str;
			}
			else if (!strncmp(line, "bool", typeend-line))
			{
				type=flag;
			}
			else if (!strncmp(line, "enum", typeend-line))
			{
				type=enumer;
				error("enum not supported");
			}
			else error("no such type");
			
			const char * basetype=basetype;
			const char * arrayname=arrayname;
			unsigned int arraypos=arraypos;
			unsigned int type_pass_id=type_pass_id;
			if (type==str && isinput)
			{
				basetype="char*";
				arrayname="inputs";
				arraypos=numinputs;
				type_pass_id=0;
			}
			if (type==str && !isinput)
			{
				basetype="char*";
				arrayname="_strings";
				arraypos=numstrs;
				type_pass_id=1;
			}
			if (type==num && rangelow<0)
			{
				basetype="signed int";
				arrayname="_ints";
				arraypos=numints;
				type_pass_id=2;
			}
			if (type==num && rangelow>=0)
			{
				basetype="unsigned int";
				arrayname="_uints";
				arraypos=numuints;
				type_pass_id=3;
			}
			if (type==flag)
			{
				basetype="bool";
				arrayname="_bools";
				arraypos=numbools;
				type_pass_id=5;
			}
			if (type==enumer)
			{
				basetype=enumtype;
				arrayname="_enums";
				arraypos=numenums;
				type_pass_id=4;
			}
			
			if (*typeend!=' ') error("bad type");
			while (*typeend==' ') typeend++;
			
			char varname[64];
			char * varend=strchr(typeend, '|');
			if (!varend) varend=strchr(typeend, '=');
			if (!varend) varend=strchr(typeend, '\0');
			memcpy(varname, typeend, varend-typeend); varname[varend-typeend]='\0';
			
			if (type==enumer)
			{
				strcpy(enumtype, "enum ");
				strcat(enumtype, varname);
			}
			
			int arraylen=1;
			bool shuffled=false;
			int arrayshowtop=1;
			char defaults[256][32];
			memset(defaults, 0, sizeof(defaults));
			const char * stddefault=(type==str)?"NULL":(type==num)?"0":(type==flag)?"false":defaults[0];
			
			char thiscfgname_c[64];//constant
			char thiscfgname_d[64];//dynamic
			
			strcpy(thiscfgname_c, varname);
			
			if (*varend=='|')
			{
				struct {
					char before[32];
					
					char keys[256][32];
					unsigned int index[256];
					unsigned int len;
					unsigned int at;
					unsigned int multiplier;
				} loop[32];
				int maxloopdepth=0;
				
				unsigned int maxdynamiclen=0;
				int loopdepth=0;
				char * at=varend+1;
				int cfgnamestart=strlen(varname);
				while (*at && *at!='=')
				{
					unsigned int thisdynamiclen=0;
					int beforelen=0;
					while (*at && *at!='=' && *at!='{' && *at!='[')
					{
						loop[loopdepth].before[beforelen]=*at;
						at++;
						cfgnamestart++;
						beforelen++;
					}
					loop[loopdepth].before[beforelen]='\0';
					if (loopdepth) maxdynamiclen+=beforelen;
					
					if(0);
					else if (*at=='=')//trailing characters, implement it as loop of length 1.
					{
						loop[loopdepth].keys[0][0]='\0';
						loop[loopdepth].index[0]=0;
						loop[loopdepth].len=1;
					}
					else if (*at=='[')
					{
						char* newat;
						int rangelow=strtol(at+1, &newat, 0);
						if (at==newat || *newat!='-') error("bad loop range");
						at=newat;
						int rangehigh=strtol(at+1, &newat, 0);
						if (at!=newat && newat[0]=='?' && newat[1]=='-')
						{
							at=newat+2;
							arrayshowtop*=(strtoul(at, &newat, 0)-rangelow+1);
						}
						else arrayshowtop*=(rangehigh-rangelow+1);
						if (at==newat || *newat!=']') error("bad loop range");
						at=newat+1;
						loop[loopdepth].len=rangehigh+1-rangelow;
						for (int i=0;i<rangehigh+1-rangelow;i++)
						{
							sprintf(loop[loopdepth].keys[i], "%i", i+rangelow);
							loop[loopdepth].index[i]=i;
							if (strlen(loop[loopdepth].keys[i])>thisdynamiclen) thisdynamiclen=strlen(loop[loopdepth].keys[i]);
						}
					}
					else if (*at=='{')
					{
						int i=0;
						while (*at!='}')
						{
							at++;
							char * key=at;
							while (*at!='=' && *at!=';' && *at!='}') at++;
							if (at==key) error("bad keys");
							strncpy(loop[loopdepth].keys[i], key, at-key);
							if (at-key>thisdynamiclen) thisdynamiclen=at-key;
							loop[loopdepth].keys[i][at-key]='\0';
							if (*at=='=')
							{
								shuffled=true;
								char * index=at+1;
								loop[loopdepth].index[i]=strtoul(index, &at, 0);
								if (*at!=';' && *at!='}') error("bad index");
							}
							else loop[loopdepth].index[i]=i;
							i++;
						}
						at++;
						loop[loopdepth].len=i;
						arrayshowtop*=i;
					}
					else error("what");
					arraylen*=loop[loopdepth].len;
					for (int i=0;i<loopdepth;i++)
					{
						loop[i].multiplier*=loop[loopdepth].len;
					}
					loop[loopdepth].multiplier=1;
					loop[loopdepth].at=0;
					loopdepth++;
					maxdynamiclen+=thisdynamiclen;
				}
				maxloopdepth=loopdepth;
				
				if (*at=='=')
				{
					int i=0;
					while (*at)
					{
						at++;
						char * start=at;
						while (*at && *at!=';') at++;
						if (at==start) error("bad value");
						strncpy(defaults[i], start, at-start);
						defaults[i][at-start]='\0';
						i++;
					}
				}
				
				strcpy(thiscfgname_c, varname);
				strcat(thiscfgname_c, loop[0].before);
				
				char table_num[4096];
				int table_num_pos=0;
				
				char table_byte[4096];
				int table_byte_pos=0;
				static char table_byte_prev[4096];
				static int table_byte_pos_prev;
				
#define append_table_num(byte) do { table_num[table_num_pos++]=byte; } while(0)
#define append_table_byte(byte) do { table_byte[table_byte_pos++]=byte; } while(0)
				
				while (true)
				{
					strcpy(thiscfgname_d, "");
					unsigned int index=0;
					
					signed int thisdepth;
					for (thisdepth=0;thisdepth<maxloopdepth;thisdepth++)
					{
						if (thisdepth) strcat(thiscfgname_d, loop[thisdepth].before);
						strcat(thiscfgname_d, loop[thisdepth].keys[loop[thisdepth].at]);
						index+=loop[thisdepth].index[loop[thisdepth].at]*loop[thisdepth].multiplier;
					}
					
					//char varandindex[256];
					//sprintf(varandindex, "%s[%i]", varname, index);
					
					append_table_num(index);
					
					int i;
					for (i=0;i<maxdynamiclen && thiscfgname_d[i];i++)
					{
						append_table_byte(thiscfgname_d[i]);
					}
					while (i++ < maxdynamiclen) append_table_byte('\0');
					
					for (thisdepth=maxloopdepth-1;thisdepth>=0;thisdepth--)
					{
						if (loop[thisdepth].at>=loop[thisdepth].len-1) loop[thisdepth].at=0;
						else
						{
							loop[thisdepth].at++;
							break;
						}
					}
					if (thisdepth<0) break;
				}
				
				if (table_byte_pos!=table_byte_pos_prev || memcmp(table_byte, table_byte_prev, table_byte_pos))
				{
					table_byte_pos_prev=table_byte_pos;
					memcpy(table_byte_prev, table_byte, table_byte_pos);
					
					if (!shuffled)
					{
						emit_bytecode(CFGB_ARRAY);
						emit_bytecode(arraylen);
						emit_bytecode(maxdynamiclen);
						emit_bytecode(arrayshowtop);
					}
					if (shuffled)
					{
						emit_bytecode(CFGB_ARRAY_SHUFFLED);
						emit_bytecode(arraylen);
						emit_bytecode(maxdynamiclen);
						emit_bytecode(arrayshowtop);
						for (int i=0;i<table_num_pos;i++) emit_bytecode(table_num[i]);
					}
					for (int i=0;i<table_byte_pos;i++) emit_bytecode(table_byte[i]);
				}
				else
				{
					emit_bytecode(CFGB_ARRAY_SAME);
					emit_bytecode(arraylen);
					emit_bytecode(maxdynamiclen);
					emit_bytecode(arrayshowtop);
				}
			}
			else if (*varend=='=')
			{
				//if (type==enumer)
				//{
				//	emit_table_str("const char * _values_enum_%s[]={ ", varname);
				//	int id=0;
				//	while (*varend)
				//	{
				//		varend++;
				//		char * nextend=strchr(varend, ';');
				//		if (!nextend) nextend=strchr(varend, '\0');
				//		int len=nextend-varend;
				//		memcpy(defaults[id], varend, len);
				//		defaults[id][len]='\0';
				//		emit_table_str("\"%s\", ", defaults[id]);
				//		varend=nextend;
				//		id++;
				//	}
				//	numenumopts=id;
				//	emit_table_str("NULL };\n");
				//}
				//else
				{
					strcpy(defaults[0], varend+1);
				}
			}
			else strcpy(defaults[0], stddefault);
			
			
			if (type==str)
			{
				if (isinput)
				{
					emit_bytecode(CFGB_INPUT);
					emit_bytecode_2(numinputs);
				}
				else
				{
					emit_bytecode(CFGB_STR);
					emit_bytecode_2(numstrs);
				}
			}
			if (type==num && rangelow>=0)
			{
				emit_bytecode(CFGB_UINT);
				emit_bytecode_2(numuints);
				emit_bytecode_4(rangelow);
				emit_bytecode_4(rangehigh);
			}
			if (type==num && rangelow<0)
			{
				emit_bytecode(CFGB_INT);
				emit_bytecode_2(numints);
				emit_bytecode_4(-rangelow);
				emit_bytecode_4(rangehigh);
			}
			if (type==flag)
			{
				emit_bytecode(CFGB_BOOL);
				emit_bytecode_2(numbools);
			}
			if (type==enumer)
			{
				//emit_bytecode_raw("CFGB_ENUM, %i,%i, ", numenums>>8, numenums&255);
			}
			emit_bytecode(strlen(thiscfgname_c));
			for (int i=0;thiscfgname_c[i];i++)
			{
				emit_bytecode(thiscfgname_c[i]);
			}
			
			
			
			
			
			
			
			
			
			
			if (arraylen==1)
			{
				if (type!=enumer)
				{
					emit_header_this("%s %s;\n", basetype, varname);
					emit_header_override_this("bool _override_%s;\n", varname);
				}
				if (type==enumer)
				{
					emit_header_enum("%s { ", basetype);
					for (int i=0;i<numenumopts;i++)
					{
						emit_header_enum("%s, ", defaults[i]);
					}
					emit_header_enum("} %s;\n", varname);
					emit_header_override_enum("bool _override_%s;\n", varname);
					//emit_bytecode("CFGB_ENUM, %i>>8, %i&255, ", numenums, numenums);
				}
				
				emit_clear_defaults_this("this->%s[%i]=", arrayname, arraypos);
				if (type==str && strcmp(defaults[0], "NULL"))
				{
					emit_clear_defaults_this("strdup(\"%s\");\n", defaults[0]);
				}
				else emit_clear_defaults_this("%s;\n", defaults[0]);
			}
			else
			{
				emit_header_this("%s %s[%u];\n", basetype, varname, arraylen);
				emit_header_override_this("bool _override_%s[%u];\n", varname, arraylen);
				if (type==enumer) error("enum array");
				
				emit_clear_defaults_this("for (unsigned int i=0;i<%u;i++)\n{\n", arraylen);
					emit_clear_defaults_this("this->%s[%u+i]=%s;\n", arrayname, arraypos, stddefault);
					//if (arrayshowtop!=arraylen)
					//{
					//	emit_clear_defaults("this->_override_%s[i]=(i<%i);\n", varname, arrayshowtop);
					//}
					//else
					//{
						//emit_clear_defaults("this->_override_%s[i]=true;\n", varname);
					//}
					emit_clear_defaults_this("}\n");
				for (unsigned int i=0;i<arraylen;i++)
				{
					if (defaults[i][0])
					{
						emit_clear_defaults_this("this->%s[%u+%u]=", arrayname, arraypos, i);
						if (type==str) emit_clear_defaults_this("strdup(\"%s\");\n", defaults[i]);
						else emit_clear_defaults_this("%s;\n", defaults[i]);
					}
				}
			}
			
			if (isinput) emit_header_input_enum("%s=%i,\n", varname, numinputs);
			if (type==str && isinput) numinputs+=arraylen;
			if (type==str && !isinput) numstrs+=arraylen;
			if (type==num && rangelow>=0) numuints+=arraylen;
			if (type==num && rangelow<0) numints+=arraylen;
			if (type==enumer) numenums+=arraylen;
			if (type==flag) numbools+=arraylen;
			numoverrides+=arraylen;
		}
		else error("bad line");
#undef error
	}
#define error(why) do { puts(why); exit(1); } while(0)
	
	emit_header_input("}; char* inputs[%i]; };\n", numinputs);
	emit_header_str("}; char* _strings[%i]; };\n", numstrs);
	emit_header_uint("}; unsigned int _uints[%i]; };\n", numuints);
	emit_header_int("}; int _ints[%i]; };\n", numints);
	emit_header_enum("}; unsigned int _enums[%i]; };\n", numenums);
	emit_header_bool("}; bool _bools[%i]; };\n", numbools);
	
	emit_header_override_input("}; bool _overrides_input[%i]; };\n", numinputs);
	emit_header_override_str("}; bool _overrides_str[%i]; };\n", numstrs);
	emit_header_override_uint("}; bool _overrides_uint[%i]; };\n", numuints);
	emit_header_override_int("}; bool _overrides_int[%i]; };\n", numints);
	emit_header_override_enum("}; bool _overrides_enum[%i]; };\n", numenums);
	emit_header_override_bool("}; bool _overrides_bool[%i]; };\n", numbools);
	emit_header_override_bool("}; bool _overrides[%i]; };\n", numoverrides);
	
	emit_bytecode(CFGB_END);
	
	if (bytecodepos > 32768) error("Resize this buffer.");
	emit_to(p_bytecode, "#ifdef CONFIG_BYTECODE\n");
	emit_to(p_bytecode, "#define CONFIG_BYTECODE_LEN %i\n", bytecodepos);
	size_t complen;
	unsigned char * comp=compress(bytecode, bytecodepos, &complen);
	for (int i=0;i<complen;i++)
	{
		emit_to(p_bytecode, "0x%.2X,", comp[i]);
		if (i%16 == 15) emit_to(p_bytecode, "\n");
	}
	emit_to(p_bytecode, "\n");
	
	emit_header_override_bool("#endif\n\n");
	emit_header_input_enum("#endif\n\n");
	emit_clear_defaults_bool("#endif\n\n");
	emit_to(p_bytecode, "#endif\n\n");
	
	for (unsigned int i=0;i<p_last;i++)
	{
		fwrite(outputs[i].ptr, 1,outputs[i].len, output);
	}
	
	fclose(in);
}

	//"Space",      "!",           "\"",         "#",           "$",          NULL,          "&",          "'",
	//"(",          ")",           "*",          "+",           ",",          "-",           ".",          "/",
	//"0",          "1",           "2",          "3",           "4",          "5",           "6",          "7",
	//"8",          "9",           ":",          ";",           "<",          "=",           ">",          "?",
	//"At",         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	//NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	//NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	//NULL,         NULL,          NULL,         "[",           "\\",         "]",           "^",          "_",
	//"`",          "a",           "b",          "c",           "d",          "e",           "f",          "g",
	//"h",          "i",           "j",          "k",           "l",          "m",           "n",          "o",
	//"p",          "q",           "r",          "s",           "t",          "u",           "v",          "w",
	//"x",          "y",           "z",          NULL,          NULL,         NULL,          NULL,         "Delete",

static const char * const keynames[]={
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	"Backspace",   "Tab",        NULL,         NULL,          "Clear",      "Return",      NULL,         NULL,
	NULL,         NULL,          NULL,         "Pause",       NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         "Escape",      NULL,         NULL,          NULL,         NULL,
	"Space",      "Exclaim",     "QuoteD",     "Hash",        "Dollar",     NULL,          "Ampersand",  "QuoteS",
	"ParenL",     "ParenR",      "Asterisk",   "Plus",        "Comma",      "Minus",       "Period",     "Slash",
	"0",          "1",           "2",          "3",           "4",          "5",           "6",          "7",
	"8",          "9",           "Colon",      "Semicolon",   "Less",       "Equals",      "Greater",    "Question",
	"At",         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         "BracketL",    "Backslash",  "BracketR",    "Caret",      "Underscore",
	"Backtick",   "A",           "B",          "C",           "D",          "E",           "F",          "G",
	"H",          "I",           "J",          "K",           "L",          "M",           "N",          "O",
	"P",          "Q",           "R",          "S",           "T",          "U",           "V",          "W",
	"X",          "Y",           "Z",          NULL,          NULL,         NULL,          NULL,         "Delete",
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	NULL,         NULL,          NULL,         NULL,          NULL,         NULL,          NULL,         NULL,
	"KP0",        "KP1",         "KP2",        "KP3",         "KP4",        "KP5",         "KP6",        "KP7",
	"KP8",        "KP9",         "KP_Period",  "KP_Divide",   "KP_Multiply", "KP_Minus",   "KP_Plus",    "KP_Enter",
	"KP_Equals",  "Up",          "Down",       "Right",       "Left",       "Insert",      "Home",       "End",
	"PageUp",     "PageDown",    "F1",         "F2",          "F3",         "F4",          "F5",         "F6",
	"F7",         "F8",          "F9",         "F10",         "F11",        "F12",         "F13",        "F14",
	"F15",        NULL,          NULL,         NULL,          "NumLock",    "CapsLock",    "ScrollLock", "ShiftR",
	"ShiftL",     "CtrlR",       "CtrlL",      "AltR",        "AltL",       "MetaR",       "MetaL",      "SuperL",
	"SuperR",     "Mode",        "Compose",    "Help",        "Print",      "SysRq",       "Break",      "Menu",
	"Power",      "Euro",        "Undo",
};

void compilekeynames(FILE * out)
{
	char keynamesr[8192];//size ended up as 869 last time I checked, but let's have a bit of margin.
	int keynameslenr=0;
	for (int i=0;i<sizeof(keynames)/sizeof(*keynames);i++)
	{
		keynameslenr+=sprintf(keynamesr+keynameslenr, "%s%c", keynames[i]?keynames[i]:"", '\0');
	}
	if (keynameslenr > 4096) error("Resize this buffer.");
	
	size_t keynameslenc;
	unsigned char * keynamesc=compress(keynamesr, keynameslenr, &keynameslenc);
	//size_t keynameslenc=keynameslenr;
	//unsigned char * keynamesc=keynamesr;
	
	fprintf(out, "#ifdef KEYNAMES_COMP\n");
	fprintf(out, "#define KEYNAMES_DECOMP_LEN %i\n", (int)keynameslenr);
	fprintf(out, "#define NUM_COMP_KEYNAMES %i\n", (int)(sizeof(keynames)/sizeof(*keynames)));
	for (int i=0;i<keynameslenc;i++)
	{
		fprintf(out, "0x%.2X,", keynamesc[i]);
		if (i%16 == 15) fprintf(out, "\n");
	}
	fprintf(out, "\n");
	fprintf(out, "#endif\n");
}
#undef error

int main()
{
	FILE * out=fopen("obj/generated.c", "wt");
	compileconfig(out);
	compilekeynames(out);
	fclose(out);
}
#endif
