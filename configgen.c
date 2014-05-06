#ifdef CONFIGGEN
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

//TODO: platform-specific things
//remember the comment coalescing cache, it could screw stuff up

#define error(why) do { printf("%s: "why"\n", linecopy); return 1; } while(0);

int main()
{
	FILE * in=fopen("minir.cfg.tmpl", "rt");
	FILE * out=fopen("obj/config.c", "wt");
	//FILE * out=stdout;
	
	enum {
		p_header_input, p_header_str, p_header_int,
		p_header_uint, p_header_enum, p_header_bool,
		p_header_override_input, p_header_override_str, p_header_override_int,
		p_header_override_uint, p_header_override_enum, p_header_override_bool,
		p_header_input_enum,
		p_clear_defaults,
		p_bytecode,
		p_last };
	
	for (int pass=0;pass<p_last;pass++)
	{
		fseek(in, 0, SEEK_SET);
#define emit_header_input(...) do { if (pass==p_header_input) fprintf(out, __VA_ARGS__); } while(0)
		emit_header_input("#ifdef CONFIG_HEADER\n");
#define emit_header_str(...) do { if (pass==p_header_str) fprintf(out, __VA_ARGS__); } while(0)
#define emit_header_uint(...) do { if (pass==p_header_uint) fprintf(out, __VA_ARGS__); } while(0)
#define emit_header_int(...) do { if (pass==p_header_int) fprintf(out, __VA_ARGS__); } while(0)
#define emit_header_enum(...) do { if (pass==p_header_enum) fprintf(out, __VA_ARGS__); } while(0)
#define emit_header_bool(...) do { if (pass==p_header_bool) fprintf(out, __VA_ARGS__); } while(0)
		
#define emit_header_override_input(...) do { if (pass==p_header_override_input) fprintf(out, __VA_ARGS__); } while(0)
#define emit_header_override_str(...) do { if (pass==p_header_override_str) fprintf(out, __VA_ARGS__); } while(0)
#define emit_header_override_uint(...) do { if (pass==p_header_override_uint) fprintf(out, __VA_ARGS__); } while(0)
#define emit_header_override_int(...) do { if (pass==p_header_override_int) fprintf(out, __VA_ARGS__); } while(0)
#define emit_header_override_enum(...) do { if (pass==p_header_override_enum) fprintf(out, __VA_ARGS__); } while(0)
#define emit_header_override_bool(...) do { if (pass==p_header_override_bool) fprintf(out, __VA_ARGS__); } while(0)
		
#define emit_header_input_enum(...) do { if (pass==p_header_input_enum) fprintf(out, __VA_ARGS__); } while(0)
		emit_header_input_enum("#ifdef CONFIG_HEADER_ENUM\n");
		
#define emit_clear_defaults(...) do { if (pass==p_clear_defaults) fprintf(out, __VA_ARGS__); } while(0)
		emit_clear_defaults("#ifdef CONFIG_CLEAR_DEFAULTS\n");
		
#define emit_bytecode(...) do { if (pass==p_bytecode) fprintf(out, __VA_ARGS__); } while(0)
		emit_bytecode("#ifdef CONFIG_BYTECODE\n");
		
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
		
		char comment[1024];
		int commentlen=0;
		
		while (!feof(in))
		{
			char linebuf[1024];
			char linecopy[1024];
			char * line=linebuf;
			*line='\0';
			if(fgets(line, 1024, in)){};//shut up gcc, if I cast a return value to void it means I don't care.
			char * eol=strchr(line, '\n');
			if (eol) *eol='\0';
			strcpy(linecopy, line);
			
			while (*line==' ') line++;
			
			if (*line=='#')
			{
				strcpy(comment+commentlen, line);
				commentlen+=strlen(line);
				strcpy(comment+commentlen, "\n");
				commentlen++;
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
						if (i!=0) emit_bytecode("\n");
						int remaining=commentlen-i;
						emit_bytecode("CFGB_COMMENT, %i, ", remaining>255?255:remaining);
					}
					if (comment[i]=='\r') {}
					else if (comment[i]=='\n') emit_bytecode("'\\n',");
					else if (strchr("'\"\\", comment[i])) emit_bytecode("'\\%c',", comment[i]);
					else emit_bytecode("'%c',", comment[i]);
				}
				emit_bytecode("\n");
				
				commentlen=0;
			}
			
			if (!*line)
			{
				emit_bytecode("CFGB_LINEBREAK,\n");
			}
			else if (*line=='#') {}//handled above
			else if (!strcmp(line, "-")) {}//handled above
			else if (!strncmp(line, "//", 2)) {}//should have an empty handler
			else if (islower(*line))
			{
				if (!strncmp(line, "global ", 7))
				{
					line+=7;
					emit_bytecode("CFGB_GLOBAL, ");
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
				if (type==num && rangelow>=0) basetype="unsigned int";
				if (type==num && rangelow<0) basetype="signed int";
				if (type==str) basetype="char*";
				if (type==flag) basetype="bool";
				if (type==enumer) basetype=enumtype;
				
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
					char * table_num_end=table_num;
					
					char table_byte[4096];
					static char table_byte_prev[4096];
					char * table_byte_end=table_byte;
					
#define append_table_num(...) do { if (pass==p_bytecode) table_num_end+=sprintf(table_num_end, __VA_ARGS__); } while(0)
#define append_table_byte(...) do { if (pass==p_bytecode) table_byte_end+=sprintf(table_byte_end, __VA_ARGS__); } while(0)
					
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
						
						char varandindex[256];
						sprintf(varandindex, "%s[%i]", varname, index);
						
						append_table_num("%u,", index);
						
						int i;
						for (i=0;i<maxdynamiclen && thiscfgname_d[i];i++)
						{
							append_table_byte("'%c',", thiscfgname_d[i]);
						}
						while (i++ < maxdynamiclen) append_table_byte("'\\0',");
						append_table_byte(" ");
						
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
					
					if (pass==p_bytecode)
					{
						if (strcmp(table_byte, table_byte_prev))
						{
							strcpy(table_byte_prev, table_byte);
							
							if (!shuffled) emit_bytecode("CFGB_ARRAY, %i,%i,%i,\n\t", arraylen, maxdynamiclen, arrayshowtop);
							if (shuffled) emit_bytecode("CFGB_ARRAY_SHUFFLED, %i,%i,%i, %s\n\t", arraylen, maxdynamiclen, arrayshowtop, table_num);
							emit_bytecode("%s\n\t", table_byte);
						}
						else
						{
							emit_bytecode("CFGB_ARRAY_SAME, %i,%i,%i, ", arraylen, maxdynamiclen, arrayshowtop);
						}
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
						emit_bytecode("CFGB_INPUT, %i,%i, ", numinputs>>8, numinputs&255);
					}
					else
					{
						emit_bytecode("CFGB_STR, %i,%i, ", numstrs>>8, numstrs&255);
					}
				}
				if (type==num && rangelow>=0)
				{
					emit_bytecode("CFGB_UINT, %i,%i, ", numuints>>8, numuints&255);
					emit_bytecode("%lli,%lli,%lli,%lli, ", (rangelow>>24)&255, (rangelow>>16)&255,
					                                       (rangelow>>8 )&255, (rangelow>>0 )&255);
					emit_bytecode("%lli,%lli,%lli,%lli, ", (rangehigh>>24)&255, (rangehigh>>16)&255,
					                                       (rangehigh>>8 )&255, (rangehigh>>0 )&255);
				}
				if (type==num && rangelow<0)
				{
					emit_bytecode("CFGB_INT, %i,%i, ", numints>>8, numints&255);
					emit_bytecode("%lli,%lli,%lli,%lli, ", ((-rangelow)>>24)&255, ((-rangelow)>>16)&255,
					                                       ((-rangelow)>>8 )&255, ((-rangelow)>>0 )&255);
					emit_bytecode("%lli,%lli,%lli,%lli, ", (rangehigh>>24)&255, (rangehigh>>16)&255,
					                                       (rangehigh>>8 )&255, (rangehigh>>0 )&255);
				}
				if (type==flag)
				{
					emit_bytecode("CFGB_BOOL, %i,%i, ", numbools>>8, numbools&255);
				}
				if (type==enumer)
				{
					//emit_bytecode("CFGB_ENUM, %i,%i, ", numenums>>8, numenums&255);
				}
				emit_bytecode("%i, ", (int)strlen(thiscfgname_c));
				for (int i=0;thiscfgname_c[i];i++)
				{
					emit_bytecode("'%c',", thiscfgname_c[i]);
				}
				emit_bytecode("\n");
				
				
				
				
				
				
				
				
				
				
				if (arraylen==1)
				{
					if (type==str)
					{
						if (isinput)
						{
							emit_header_input("%s %s;\n", basetype, varname);
							emit_header_override_input("bool _override_%s;\n", varname);
						}
						else
						{
							emit_header_str("%s %s;\n", basetype, varname);
							emit_header_override_str("bool _override_%s;\n", varname);
						}
					}
					if (type==num && rangelow>=0)
					{
						emit_header_uint("%s %s;\n", basetype, varname);
						emit_header_override_uint("bool _override_%s;\n", varname);
					}
					if (type==num && rangelow<0)
					{
						emit_header_int("%s %s;\n", basetype, varname);
						emit_header_override_int("bool _override_%s;\n", varname);
					}
					if (type==flag)
					{
						emit_header_bool("%s %s;\n", basetype, varname);
						emit_header_override_bool("bool _override_%s;\n", varname);
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
					
					emit_clear_defaults("this->%s=", varname);
					if (type==str && strcmp(defaults[0], "NULL"))
					{
						emit_clear_defaults("strdup(\"%s\");\n", defaults[0]);
					}
					else
					{
						emit_clear_defaults("%s;\n", defaults[0]);
					}
				}
				else
				{
					if (type==str)
					{
						if (isinput)
						{
							emit_header_input("%s %s[%u];\n", basetype, varname, arraylen);
							emit_header_override_input("bool _override_%s[%u];\n", varname, arraylen);
						}
						else
						{
							emit_header_str("%s %s[%u];\n", basetype, varname, arraylen);
							emit_header_override_str("bool _override_%s[%u];\n", varname, arraylen);
						}
					}
					if (type==num && rangelow<0)
					{
						emit_header_int("%s %s[%u];\n", basetype, varname, arraylen);
						emit_header_override_int("bool _override_%s[%u];\n", varname, arraylen);
					}
					if (type==num && rangelow>=0)
					{
						emit_header_uint("%s %s[%u];\n", basetype, varname, arraylen);
						emit_header_override_uint("bool _override_%s[%u];\n", varname, arraylen);
					}
					if (type==enumer) error("enum array");
					if (type==flag)
					{
						emit_header_bool("%s %s[%u];\n", basetype, varname, arraylen);
						emit_header_override_bool("bool _override_%s[%u];\n", varname, arraylen);
					}
					
					emit_clear_defaults("for (unsigned int i=0;i<%i;i++)\n{\n", arraylen);
						emit_clear_defaults("this->%s[i]=%s;\n", varname, stddefault);
						//if (arrayshowtop!=arraylen)
						//{
						//	emit_clear_defaults("this->_override_%s[i]=(i<%i);\n", varname, arrayshowtop);
						//}
						//else
						//{
							//emit_clear_defaults("this->_override_%s[i]=true;\n", varname);
						//}
						emit_clear_defaults("}\n");
						for (unsigned int i=0;i<arraylen;i++)
						{
							if (defaults[i][0])
							{
								emit_clear_defaults("this->%s[%u]=", varname, i);
								if (type==str) emit_clear_defaults("strdup(\"%s\");\n", defaults[i]);
								else emit_clear_defaults("%s;\n", defaults[i]);
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
		}
		
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
		
		if (pass>=p_header_override_bool)
		{
			fprintf(out, "#endif\n\n");
		}
	}
	fclose(in);
	fclose(out);
}
#endif
