#include "minir.h"
#include <string.h>
#include <strings.h> //strcasecmp
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

struct minirconfig config={};

#define count(array) (sizeof(array)/sizeof(*(array)))

static struct minirconfig * global=NULL;

struct varying {
	struct minirconfig * config;
	size_t num;
	size_t buflen;
	size_t this;
};

static struct varying bycore;
static struct varying bygame;

static const char * autoload=NULL;

static char * originalconfig=NULL;

static void clear_to_defaults(struct minirconfig * this)
{
	memset(this, 0, sizeof(struct minirconfig));
	for (int i=0;i<count(this->_overrides);i++) this->_overrides[i]=true;
#define CONFIG_CLEAR_DEFAULTS
#include "obj/config.c"
#undef CONFIG_CLEAR_DEFAULTS
}

static void clear_to_parent(struct minirconfig * this)
{
	memset(this, 0, sizeof(struct minirconfig));
	for (int i=0;i<count(this->_overrides);i++) this->_overrides[i]=false;
//#define CONFIG_CLEAR_PARENT
//#include "obj/config.c"
//#undef CONFIG_CLEAR_PARENT
}

static void join_config(struct minirconfig * parent, struct minirconfig * child)
{
	if (!child) return;
#define JOIN(groupname, overridegroupname) \
		for (int i=0;i<count(parent->groupname);i++) \
		{ \
			if (child->overridegroupname[i]) parent->groupname[i]=child->groupname[i]; \
		}
	JOIN(inputs,_overrides_input);
	JOIN(_strings,_overrides_str);
	JOIN(_ints,_overrides_int);
	JOIN(_uints,_overrides_uint);
	JOIN(_enums,_overrides_enum);
	JOIN(_bools,_overrides_bool);
#undef JOIN
//#define CONFIG_JOIN
//#include "obj/config.c"
//#undef CONFIG_JOIN
}

static size_t create_core()
{
	if (bycore.num==bycore.buflen)
	{
		bycore.buflen*=2;
		bycore.config=realloc(bycore.config, bycore.buflen*sizeof(struct minirconfig));
	}
	clear_to_parent(&bycore.config[bycore.num]);
	bycore.num++;
	return bycore.num-1;
}

static size_t find_core(const char * core)
{
	size_t i;
	for (i=1;i<bycore.num;i++)
	{
		if (!strcmp(core, bycore.config[i]._path)) break;
	}
	return i;
}

static size_t find_or_create_core(const char * core)
{
	if (core==NULL) return 0;
	
	size_t i=find_core(core);
	if (i==bycore.num)
	{
		create_core();
		bycore.config[i]._path=strdup(core);
		return i;
	}
	return i;
}

static size_t create_game()
{
	if (bygame.num==bygame.buflen)
	{
		bygame.buflen*=2;
		bygame.config=realloc(bygame.config, bygame.buflen*sizeof(struct minirconfig));
	}
	clear_to_parent(&bygame.config[bygame.num]);
	bygame.num++;
	return bygame.num-1;
}

static size_t find_game(const char * game)
{
	size_t i;
	for (i=1;i<bygame.num;i++)
	{
		if (!strcmp(game, bygame.config[i]._path)) break;
	}
	return i;
}

static size_t find_or_create_game(const char * game)
{
	if (game==NULL) return 0;
	
	size_t i=find_game(game);
	if (i==bygame.num)
	{
		create_game();
		bygame.config[i]._path=strdup(game);
		return i;
	}
	return i;
}

static void delete_conf(struct minirconfig * this)
{
	free(this->_path);
	free(this->_name);
	for (int i=0;i<count(this->inputs);i++) free(this->inputs[i]);
	for (int i=0;i<count(this->_strings);i++) free(this->_strings[i]);
}



static unsigned int bitround(unsigned int in)
{
	in--;
	in|=in>>1;
	in|=in>>2;
	in|=in>>4;
	in|=in>>8;
	in|=in>>16;
	in++;
	return in;
}

static int find_place(char* * start, int len, char* new)
{
	int jumpsize=bitround(len+1)/2;
	int pos=0;
	while (jumpsize)
	{
		if (pos<0) pos+=jumpsize;
		else if (pos>=len) pos-=jumpsize;
		else if (strcmp(start[pos], new)<0) pos+=jumpsize;
		else pos-=jumpsize;
		jumpsize/=2;
	}
	if (pos<0) pos+=1;
	else if (pos>=len) pos-=0;
	else if (strcmp(start[pos], new)<0) pos+=1;
	else pos-=0;
	
	return pos;
}

static void sort_and_clean_core_support(struct minirconfig * core)
{
	char* * unsorted;
	int numunsorted;
	char* * sorted;
	int numsorted;
	
	
	//sort/uniq support
	unsorted=core->_support;
	numunsorted=core->_support_count;
	sorted=unsorted;
	numsorted=0;
	
	while (numunsorted)
	{
		char* this=*unsorted;
		int newpos=find_place(sorted, numsorted, this);
		if (newpos==numsorted || strcmp(this, sorted[newpos])!=0)
		{
			memmove(sorted+newpos+1, sorted+newpos, sizeof(char*)*(numsorted-newpos));
			sorted[newpos]=this;
			numsorted++;
		}
		else free(this);
		unsorted++;
		numunsorted--;
	}
	core->_support_count=numsorted;
	
	
	//sort/uniq primary, delete non-support
	char* * support=core->_support;
	int numsupport=core->_support_count;
	
	unsorted=core->_primary;
	numunsorted=core->_primary_count;
	sorted=unsorted;
	numsorted=0;
	
	while (numunsorted)
	{
		char* this=*unsorted;
		int newpos=find_place(sorted, numsorted, this);
		
		int supportpos=find_place(support, numsupport, this);
		
		if ((supportpos<numsupport && strcmp(this, support[supportpos])==0) &&
				(newpos==numsorted || strcmp(this, sorted[newpos])!=0))
		{
			memmove(sorted+newpos+1, sorted+newpos, sizeof(char*)*(numsorted-newpos));
			sorted[newpos]=this;
			numsorted++;
		}
		else free(this);
		unsorted++;
		numunsorted--;
	}
	core->_primary_count=numsorted;
}



const char * config_get_autoload()
{
	return autoload;
}



const char * * config_get_supported_extensions()
{
	int numret=0;
	for (int i=0;i<bycore.num;i++)
	{
		numret+=bycore.config[i]._primary_count;
	}
	
	const char * * ret=malloc(sizeof(const char*)*(numret+1));
	ret[numret]=NULL;
	numret=0;
	for (int i=0;i<bycore.num;i++)
	{
		memcpy(ret+numret, bycore.config[i]._primary, sizeof(const char*)*bycore.config[i]._primary_count);
		numret+=bycore.config[i]._primary_count;
	}
	return ret;
}



static void split_config(struct minirconfig * public, struct minirconfig * global,
												 struct minirconfig * bycore, struct minirconfig * bygame)
{
#define SPLIT(groupname, overridegroupname) \
		for (int i=0;i<count(public->groupname);i++) \
		{ \
			     if (bygame->overridegroupname[i]) bygame->groupname[i]=public->groupname[i]; \
			else if (bycore->overridegroupname[i]) bycore->groupname[i]=public->groupname[i]; \
			else global->groupname[i]=public->groupname[i]; \
		}
	SPLIT(inputs,_overrides_input);
	SPLIT(_strings,_overrides_str);
	SPLIT(_ints,_overrides_int);
	SPLIT(_uints,_overrides_uint);
	SPLIT(_enums,_overrides_enum);
	SPLIT(_bools,_overrides_bool);
#undef SPLIT
}

void config_load(const char * corepath, const char * gamepath)
{
	split_config(&config, global, &bycore.config[bycore.this], &bygame.config[bygame.this]);
	
	join_config(&config, global);
	
	if (corepath)
	{
		char * truecore=window_get_absolute_path(corepath);
		bycore.this=find_or_create_core(truecore);
		free(truecore);
		join_config(&config, &bycore.config[bycore.this]);
		config.corename=bycore.config[bycore.this]._name;
	}
	else config.corename=NULL;
	
	if (gamepath)
	{
		char * truegame=window_get_absolute_path(gamepath);
		bygame.this=find_or_create_game(truegame);
		free(truegame);
		join_config(&config, &bygame.config[bygame.this]);
		config.gamename=bygame.config[bygame.this]._name;
	}
	else config.gamename=NULL;
}



void config_create_core(const char * core, bool override_existing, const char * name, const char * const * supported_extensions)
{
	char * truecore=window_get_absolute_path(core);
	size_t id=find_or_create_core(truecore);
	free(truecore);
	
	if (override_existing || !bycore.config[id]._name)
	{
		free(bycore.config[id]._name);
		bycore.config[id]._name=strdup(name);
	}
	
	if (override_existing || !bycore.config[id]._support_count)
	{
		for (unsigned int i=0;i<bycore.config[id]._support_count;i++) free(bycore.config[id]._support[i]);
		for (unsigned int i=0;i<bycore.config[id]._primary_count;i++) free(bycore.config[id]._primary[i]);
		free(bycore.config[id]._support); bycore.config[id]._support=NULL;
		free(bycore.config[id]._primary); bycore.config[id]._primary=NULL;
		
		size_t support_buflen=2;
		char* * support=malloc(sizeof(char*)*support_buflen);
		size_t support_count=0;
		
		size_t primary_buflen=2;
		char* * primary=malloc(sizeof(char*)*primary_buflen);
		size_t primary_count=0;
		
		for (size_t i=0;supported_extensions[i];i++)
		{
			for (size_t j=0;j<support_count;j++)
			{
				if (!strcmp(support[j], supported_extensions[i])) goto nope;
			}
			
			support[support_count]=strdup(supported_extensions[support_count]);
			support_count++;
			if (support_count==support_buflen)
			{
				support_buflen*=2;
				support=realloc(support, sizeof(char*)*support_buflen);
			}
			
			for (size_t j=0;j<bycore.num;j++)
			{
				if (!bycore.config[j]._primary) continue;
				for (size_t k=0;bycore.config[j]._primary[k];k++)
				{
					if (!strcmp(bycore.config[j]._primary[k], supported_extensions[i])) goto nope;
				}
			}
			
			primary[primary_count]=strdup(supported_extensions[i]);
			primary_count++;
			if (primary_count==primary_buflen)
			{
				primary_buflen*=2;
				primary=realloc(primary, sizeof(char*)*primary_buflen);
			}
		nope: ;
		}
		
		support[support_count]=NULL;
		primary[primary_count]=NULL;
		
		bycore.config[id]._support=support;
		bycore.config[id]._support_count=support_count;
		bycore.config[id]._primary=primary;
		bycore.config[id]._primary_count=primary_count;
		
		sort_and_clean_core_support(&bycore.config[id]);
	}
}

void config_create_game(const char * game, bool override_existing, const char * name)
{
	char * truegame=window_get_absolute_path(game);
	size_t id=find_or_create_game(truegame);
	free(truegame);
	
	if (override_existing || !bygame.config[id]._name)
	{
		free(bygame.config[id]._name);
		bygame.config[id]._name=strdup(name);
	}
}

void config_delete_core(const char * core)
{
	size_t id=find_core(core);
	if (id==bycore.num) return;
	
	if (bycore.this==id) config_load(NULL, config.gamename);
	if (bycore.this>id) bycore.this--;
	
	memmove(&bycore.config[id], &bycore.config[id+1], sizeof(*bycore.config)*(bycore.num-id));
	bycore.num--;
}

void config_delete_game(const char * game)
{
	size_t id=find_core(game);
	if (id==bygame.num) return;
	
	if (bygame.this==id) config_load(config.corename, NULL);
	if (bygame.this>id) bygame.this--;
	
	memmove(&bygame.config[id], &bygame.config[id+1], sizeof(*bygame.config)*(bygame.num-id));
	bygame.num--;
}



struct minircorelist * config_get_core_for(const char * gamepath, unsigned int * count)
{
	size_t gameid=find_game(gamepath);
	if (gameid!=bygame.num)
	{
		if (bygame.config[gameid]._forcecore)
		{
			size_t coreid=find_core(bygame.config[gameid]._forcecore);
			if (coreid!=bycore.num)
			{
				struct minircorelist * ret=malloc(sizeof(struct minircorelist)*2);
				ret[0].path=bycore.config[coreid]._path;
				ret[0].name=bycore.config[coreid]._name;
				ret[1].path=NULL;
				ret[1].name=NULL;
				return ret;
			}
			else
			{
				free(bygame.config[gameid]._forcecore);
				bygame.config[gameid]._forcecore=NULL;
			}
		}
	}
	
	const char * extension=strrchr(gamepath, '.');
	size_t retbuflen=1;
	size_t numret=0;
	struct minircorelist * ret=malloc(sizeof(struct minircorelist)*retbuflen);
	if (extension && !strchr(extension, '/'))
	{
		extension++;
		for (int i=0;i<bycore.num;i++)
		{
			bool thisprimary=false;
			for (int j=0;j<bycore.config[i]._support_count;j++)
			{
				if (j<bycore.config[i]._primary_count && !strcmp(extension, bycore.config[i]._primary[j]))
				{
					thisprimary=true;
				}
				if (!strcmp(extension, bycore.config[i]._support[j]))
				{
					if (thisprimary && numret!=0)
					{
						ret[numret].path=ret[0].path;
						ret[numret].name=ret[0].name;
						ret[0].path=bycore.config[i]._path;
						ret[0].name=bycore.config[i]._name;
					}
					else
					{
						ret[numret].path=bycore.config[i]._path;
						ret[numret].name=bycore.config[i]._name;
					}
					
					numret++;
					if (numret==retbuflen)
					{
						retbuflen*=2;
						ret=realloc(ret, sizeof(struct minircorelist)*retbuflen);
					}
				}
			}
		}
	}
	ret[numret].path=NULL;
	ret[numret].name=NULL;
	if (count) *count=numret;
	return ret;
}



static void readnumu(const char * str, unsigned int * out, bool * valid, unsigned int rangelower, unsigned int rangeupper)
{
	const char * end;
	unsigned int tmp=strtoul(str, (char**)&end, 0);
	if (*str && !*end)
	{
		if (tmp<rangelower) *out=rangelower;
		else if (tmp>rangeupper) *out=rangeupper;
		else *out=tmp;
		*valid=true;
	}
}

static void readnums(const char * str, signed int * out, bool * valid, int rangelower, int rangeupper)
{
	const char * end;
	signed int tmp=strtol(str, (char**)&end, 0);
	if (*str && !*end)
	{
		if (tmp<rangelower) *out=rangelower;
		else if (tmp>rangeupper) *out=rangeupper;
		else *out=tmp;
		*valid=true;
	}
}

static void readbool(const char * str, bool * out, bool * valid)
{
	if (!strcasecmp(str, "true")) { *out=true; *valid=true; }
	if (!strcasecmp(str, "false")) { *out=false; *valid=true; }
	if (!strcmp(str, "1")) { *out=true; *valid=true; }
	if (!strcmp(str, "0")) { *out=false; *valid=true; }
}

static void readstr(const char * str, char* * out, bool * valid)
{
	free(*out);
	if (*str) *out=strdup(str);
	else *out=NULL;
	*valid=true;
}

static void readinput(const char * str, char* * out, bool * valid)
{
	if (!*str)
	{
		free(*out);
		*out=NULL;
		*valid=true;
		return;
	}
	char * newout=inputmapper_normalize(str);
	if (newout)
	{
		free(*out);
		*out=newout;
		*valid=true;
	}
}

//static void readenum(const char * str, int * out, bool * valid, const char * * values)
//{
//	for (int i=0;values[i];i++)
//	{
//		if (!strcasecmp(str, values[i]))
//		{
//			*out=i;
//			*valid=true;
//			return;
//		}
//	}
//	return;
//}

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
const unsigned char config_bytecode_comp[]={
#define CONFIG_BYTECODE
#include "obj/config.c"
#undef CONFIG_BYTECODE
};
unsigned char config_bytecode[CONFIG_BYTECODE_LEN];

static void read_bytecode(const char * name, const char * value, struct minirconfig * thisconf, bool global)
{
	const unsigned char * thisone=config_bytecode;
	const unsigned char * arrayshuffle;
	const char * arraynames;
	while (*thisone!=CFGB_END)
	{
		const unsigned char * at=thisone;
		
		if (*at==CFGB_LINEBREAK)
		{
			thisone=at+1;
			continue;
		}
		if (*at==CFGB_COMMENT)
		{
			thisone=at+2+at[1];
			continue;
		}
		
		bool array=false;
		
		if (*at==CFGB_GLOBAL) at++;
		
		if (*at==CFGB_ARRAY)
		{
			//we can't put this inside the memcmp, as the memcmp'd code doesn't always run, and we could hit CFGB_ARRAY_SAME
			array=true;
			arrayshuffle=NULL;
			arraynames=(char*)at+4;
			at+=4+at[1]*at[2];
		}
		if (*at==CFGB_ARRAY_SHUFFLED)
		{
			array=true;
			arrayshuffle=at+4;
			arraynames=(char*)at+4+at[1];
			at+=4+at[1]+at[1]*at[2];
		}
		if (*at==CFGB_ARRAY_SAME)
		{
			array=true;
			at+=4;
		}
		
		     if (*at==CFGB_INPUT) at+=3;
		else if (*at==CFGB_STR) at+=3;
		else if (*at==CFGB_INT) at+=11;
		else if (*at==CFGB_UINT) at+=11;
		//else if (*at==CFGB_ENUM) at++;
		else if (*at==CFGB_BOOL) at+=3;
		
		int namelen=*at;
		
		if (!memcmp(name, (char*)at+1, namelen) && (array || name[namelen]=='\0'))
		{
			const unsigned char * at2=thisone;
			
			if (*at2==CFGB_GLOBAL)
			{
				at2++;
				if (!global) goto cont;
			}
			
			int arrayoffset=0;
			
			if (array)
			{
				const char * namevary=name+namelen;
				int arrlen=at2[1];
				int arrnamelen=at2[2];
				
				const char * nameat=(char*)arraynames;
				for (arrayoffset=0;arrayoffset<arrlen;arrayoffset++)
				{
					if (!strncmp(namevary, nameat+(arrayoffset*arrnamelen), arrnamelen) &&
					    (!nameat[(arrayoffset+1)*arrnamelen-1] || !namevary[arrnamelen])) break;
				}
				if (arrayoffset==arrlen) goto cont;
				
				if (arrayshuffle) arrayoffset=arrayshuffle[arrayoffset];
				
				if (*at2==CFGB_ARRAY) at2+=4+at2[1]*at2[2];
				if (*at2==CFGB_ARRAY_SHUFFLED) at2+=4+at2[1]+at2[1]*at2[2];
				if (*at2==CFGB_ARRAY_SAME) at2+=4;
			}
			
			int offset=((at2[1]<<8)|(at2[2]))+arrayoffset;
			
			if (*at2==CFGB_INPUT)
			{
				readinput(value, &thisconf->inputs[offset], &thisconf->_overrides_input[offset]);
			}
			if (*at2==CFGB_STR)
			{
				readstr(value, &thisconf->_strings[offset], &thisconf->_overrides_str[offset]);
			}
			if (*at2==CFGB_INT || *at2==CFGB_UINT)
			{
				unsigned int rangelow=(at2[3]<<24)|(at2[4]<<16)|(at2[5]<<8)|(at2[6]<<0);
				unsigned int rangehigh=(at2[7]<<24)|(at2[8]<<16)|(at2[9]<<8)|(at2[10]<<0);
				if (*at2==CFGB_INT) readnums(value, &thisconf->_ints[offset], &thisconf->_overrides_int[offset], -rangelow, rangehigh);
				else readnumu(value, &thisconf->_uints[offset], &thisconf->_overrides_uint[offset], rangelow, rangehigh);
			}
			if (*at2==CFGB_ENUM)
			{
				//readbool(value, &thisconf->_bools[offset], &thisconf->_overrides_bool[offset]);
			}
			if (*at2==CFGB_BOOL)
			{
				readbool(value, &thisconf->_bools[offset], &thisconf->_overrides_bool[offset]);
			}
		}
	cont:
		thisone=at+1+namelen;
	}
}

void config_read(const char * path)
{
	tinfl_decompress_mem_to_mem(config_bytecode, CONFIG_BYTECODE_LEN, config_bytecode_comp, sizeof(config_bytecode_comp), TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
	
	bycore.num=1;
	bycore.buflen=1;
	bycore.config=malloc(sizeof(struct minirconfig));
	clear_to_parent(&bycore.config[0]);
	bycore.this=0;
	
	bygame.num=1;
	bygame.buflen=1;
	bygame.config=malloc(sizeof(struct minirconfig));
	clear_to_parent(&bygame.config[0]);
	bygame.this=0;
	
	struct minirconfig * thisconf=NULL;
	
	if (!global) global=malloc(sizeof(struct minirconfig));
	clear_to_defaults(global);
	
	char * rawconfig;
	if (!file_read(path, &rawconfig, NULL))
	{
		memcpy(&config, global, sizeof(struct minirconfig));
		return;
	}
	
	originalconfig=strdup(rawconfig);
	
	enum { glob, core, game } thisconftype=glob;
	
	size_t support_buflen=0;
	char* * support=NULL;
	size_t support_count=0;
	
	size_t primary_buflen=0;
	char* * primary=NULL;
	size_t primary_count=0;
	
	char * thisline=rawconfig;
#define finishsection() \
			if (thisconf) \
			{ \
				if (thisconftype==core && !thisconf->_path) \
				{ \
					delete_conf(thisconf); \
					bycore.num--; \
				} \
				else if (thisconftype==game && !thisconf->_path) \
				{ \
					delete_conf(thisconf); \
					bygame.num--; \
				} \
				else \
				{ \
					if (thisconf->_autoload) \
					{ \
						if (autoload) thisconf->_autoload=false; \
						else autoload=strdup(thisconf->_path); \
					} \
					if (support) \
					{ \
						support[support_count]=NULL; \
						thisconf->_support=support; \
						thisconf->_support_count=support_count; \
					} \
					if (primary) \
					{ \
						primary[primary_count]=NULL; \
						thisconf->_primary=primary; \
						thisconf->_primary_count=primary_count; \
					} \
					if (thisconftype==core) sort_and_clean_core_support(thisconf); \
				} \
				\
				support_buflen=0; \
				support=NULL; \
				support_count=0; \
				primary_buflen=0; \
				primary=NULL; \
				primary_count=0; \
			}
	while (true)
	{
		char * nextline=strchr(thisline, '\n');
		char * carriage=strchr(thisline, '\r');
		if (carriage && carriage[1]=='\n') *carriage='\0';
		if (nextline) *nextline='\0';
		
		char * comment=strchr(thisline, '#');
		if (comment) *comment='\0';
		
		if (thisline[0]=='[')
		{
			finishsection();
			if(0);
			else if (!strcmp(thisline, "[global]")) { thisconf=global; thisconftype=glob; }
			else if (!strcmp(thisline, "[core]"))
			{
				size_t id=create_core();
				thisconf=&bycore.config[id];
				thisconftype=core;
				
				support_buflen=2;
				support=malloc(sizeof(char*)*support_buflen);
				primary_buflen=2;
				primary=malloc(sizeof(char*)*primary_buflen);
			}
			else if (!strcmp(thisline, "[game]")) { size_t id=create_game(); thisconf=&bygame.config[id]; thisconftype=game; }
			else thisconf=NULL;
		}
		
		char * value=strchr(thisline, '=');
		if (value && thisconf)
		{
			//leading on key
			while (isspace(*thisline)) thisline++;
			
			//trailing on key
			char * trailing=value;
			while (isspace(trailing[-1]) && trailing!=thisline) trailing--;
			*trailing='\0';
			
			//leading on value
			value++;
			while (isspace(*value)) value++;
			
			//trailing on value
			trailing=strchr(value, '\0');
			while (isspace(trailing[-1]) && trailing!=thisline) trailing--;
			*trailing='\0';
			
			if ((thisconftype==core || thisconftype==game) && !strcmp(thisline, "path"))
			{
				free(thisconf->_path);
				thisconf->_path=window_get_absolute_path(value);
			}
			
			if ((thisconftype==core || thisconftype==game) && !strcmp(thisline, "name"))
			{
				free(thisconf->_name);
				thisconf->_name=strdup(value);
			}
			if (thisconftype==core && !strcmp(thisline, "support"))
			{
				support[support_count]=strdup(value);
				support_count++;
				if (support_count==support_buflen)
				{
					support_buflen*=2;
					support=realloc(support, sizeof(char*)*support_buflen);
				}
			}
			if (thisconftype==core && !strcmp(thisline, "primary"))
			{
				primary[primary_count]=strdup(value);
				primary_count++;
				if (primary_count==primary_buflen)
				{
					primary_buflen*=2;
					primary=realloc(primary, sizeof(char*)*primary_buflen);
				}
			}
			
			if (thisconftype==game && !strcmp(thisline, "autoload"))
			{
				bool b=false;
				bool valid=false;
				readbool(value, &b, &valid);
				if (valid) thisconf->_autoload=b;
			}
			if (thisconftype==game && !strcmp(thisline, "core"))
			{
				free(thisconf->_forcecore);
				thisconf->_forcecore=strdup(value);
			}
			read_bytecode(thisline, value, thisconf, (thisconftype==glob));
		}
		
		if (!nextline) break;
		thisline=nextline+1;
	}
	finishsection();
	
	//find support without corresponding primary
	for (int i=0;i<bycore.num;i++)
	{
		for (int j=0;j<bycore.config[i]._support_count;j++)
		{
			for (int l=0;l<bycore.config[i]._primary_count;l++)
			{
				if (!strcmp(bycore.config[i]._support[j], bycore.config[i]._primary[l])) goto foundit;
			}
			for (int k=0;k<bycore.num;k++)
			{
				for (int l=0;l<bycore.config[k]._primary_count;l++)
				{
					if (!strcmp(bycore.config[i]._support[j], bycore.config[k]._primary[l])) goto foundit;
				}
			}
			//this has poor complexity, but shouldn't be run often
			bycore.config[i]._primary=realloc(bycore.config[i]._primary, sizeof(char*)*(bycore.config[i]._primary_count+2));
			bycore.config[i]._primary[bycore.config[i]._primary_count]=strdup(bycore.config[i]._support[j]);
			bycore.config[i]._primary[bycore.config[i]._primary_count+1]=NULL;
			bycore.config[i]._primary_count++;
		foundit:;
		}
	}
	
	memcpy(&config, global, sizeof(struct minirconfig));
	free(rawconfig);
}



static char * outstart;
static char * outat;
static char * outend;

static void reserve(unsigned int size)
{
	if (outat+size>outend)
	{
		int buflen=(outend-outstart)*2;
		int bufat=outat-outstart;
		outstart=realloc(outstart, buflen);
		outat=outstart+bufat;
		outend=outstart+buflen;
	}
}

static void appenddat(const char * str, int len)
{
	reserve(len);
	memcpy(outat, str, len);
	outat+=len;
}

static void appendstr(const char * str)
{
	appenddat(str, strlen(str));
}

static void appendlf()
{
	appenddat("\n", 1);
}

static void print_config_file(struct minirconfig * this, bool isroot)
{
	const unsigned char * thisone=config_bytecode;
	const unsigned char * arrayshuffle;
	const char * arraynames;
	int arraynamelen;
	char buf[32];
	while (*thisone!=CFGB_END)
	{
		const unsigned char * at=thisone;
		int arraylen=1;
		int arrayend=1;
		
		if (*at==CFGB_LINEBREAK)
		{
			if (outat[-2]!='\n') appendlf();
			thisone=at+1;
			continue;
		}
		if (*at==CFGB_COMMENT)
		{
			if (isroot)
			{
				appenddat((char*)at+2, at[1]);
			}
			thisone=at+2+at[1];
			continue;
		}
		
		if (*at==CFGB_GLOBAL) at++;
		
		if (*at==CFGB_ARRAY)
		{
			arrayshuffle=NULL;
			arraynames=(char*)at+4;
			arraylen=at[1];
			arraynamelen=at[2];
			arrayend=at[3];
			at+=4+at[1]*at[2];
		}
		if (*at==CFGB_ARRAY_SHUFFLED)
		{
			arrayshuffle=at+4;
			arraynames=(char*)at+4+at[1];
			arraylen=at[1];
			arraynamelen=at[2];
			arrayend=at[3];
			at+=4+at[1]+at[1]*at[2];
		}
		if (*at==CFGB_ARRAY_SAME)
		{
			arraylen=at[1];
			arrayend=at[3];
			at+=4;
		}
		
		int baseoffset=((at[1]<<8)|(at[2]));
		
		unsigned char itemtype=*at;
		
		     if (*at==CFGB_INPUT) at+=3;
		else if (*at==CFGB_STR) at+=3;
		else if (*at==CFGB_INT) at+=11;
		else if (*at==CFGB_UINT) at+=11;
		//else if (*at==CFGB_ENUM) at++;
		else if (*at==CFGB_BOOL) at+=3;
		
		for (int i=0;i<arraylen;i++)
		{
			if (itemtype==CFGB_INPUT && !this->_overrides_input[baseoffset+i]) continue;
			if (itemtype==CFGB_STR && !this->_overrides_str[baseoffset+i]) continue;
			if (itemtype==CFGB_INT && !this->_overrides_int[baseoffset+i]) continue;
			if (itemtype==CFGB_UINT && !this->_overrides_uint[baseoffset+i]) continue;
			if (itemtype==CFGB_BOOL && !this->_overrides_bool[baseoffset+i]) continue;
			
			if (i>=arrayend)
			{
				if (itemtype==CFGB_INPUT && !this->inputs[baseoffset+i]) continue;
				if (itemtype==CFGB_STR && !this->_strings[baseoffset+i]) continue;
				if (itemtype==CFGB_INT && !this->_ints[baseoffset+i]) continue;
				if (itemtype==CFGB_UINT && !this->_uints[baseoffset+i]) continue;
				if (itemtype==CFGB_BOOL && !this->_bools[baseoffset+i]) continue;
			}
			
			int arrayoffset=0;
			appenddat((char*)at+1, *at);
			if (arraylen!=1)
			{
				int dynlen=arraynamelen;
				while (arraynames[arraynamelen*i + dynlen-1]=='\0') dynlen--;
				appenddat((char*)arraynames+arraynamelen*i, dynlen);
				arrayoffset=arrayshuffle?arrayshuffle[i]:i;
			}
			int offset=baseoffset+arrayoffset;
			appendstr("=");
			if (itemtype==CFGB_INPUT)
			{
				if (this->_overrides_input[offset])
				{
					appendstr(this->inputs[offset]?this->inputs[offset]:"");
				}
			}
			if (itemtype==CFGB_STR)
			{
				if (this->_overrides_str[offset])
				{
					appendstr(this->_strings[offset]?this->_strings[offset]:"");
				}
			}
			if (itemtype==CFGB_INT)
			{
				if (this->_overrides_int[offset])
				{
					sprintf(buf, "%i", this->_ints[offset]);
					appendstr(buf);
				}
			}
			if (itemtype==CFGB_UINT)
			{
				if (this->_overrides_uint[offset])
				{
					sprintf(buf, "%u", this->_uints[offset]);
					appendstr(buf);
				}
			}
			if (itemtype==CFGB_BOOL)
			{
				if (this->_overrides_bool[offset])
				{
					appendstr(this->_bools[offset]?"true":"false");
				}
			}
			appendlf();
		}
		
		thisone=at+1+at[0];
	}
}

void config_write(const char * path)
{
	split_config(&config, global, &bycore.config[bycore.this], &bygame.config[bygame.this]);
	
	outstart=malloc(8192);
	outat=outstart;
	outend=outstart+8192;
	appendstr("[global]\n");
	print_config_file(global, true);
	
	for (unsigned int i=1;i<bycore.num;i++)
	{
		if (outat[-2]!='\n') appendlf();
		appendstr("[core]\n");
		if (i==1) appendstr("#minir will only set core-specific entries here by default,"
		                    "but you can copy any setting from [global] if you want to.\n"
		                    "#You will, of course, still be able to change it from within minir.\n"
		                    "#However, you can't change whether something is core-specific from within minir.\n"
		                    "#Anything that starts core-specific stays that way, "
		                        "even if it's set to the same value as the global one.\n"
		                    "#Additionally, you can't copy anything originating from here into other sections; "
		                        "that wouldn't make sense.\n"
		                    );
		if (bycore.config[i]._name)
		{
			appendstr("name="); appendstr(bycore.config[i]._name); appendlf();
		}
		appendstr("path="); appendstr(bycore.config[i]._path); appendlf();
		for (unsigned int j=0;j<bycore.config[i]._primary_count;j++)
		{
			appendstr("primary=");
			appendstr(bycore.config[i]._primary[j]);
			appendlf();
		}
		for (unsigned int j=0;j<bycore.config[i]._support_count;j++)
		{
			appendstr("support=");
			appendstr(bycore.config[i]._support[j]);
			appendlf();
		}
		
		//print core options HERE
		
		print_config_file(&bycore.config[i], false);
	}
	
	for (unsigned int i=1;i<bygame.num;i++)
	{
		if (outat[-2]!='\n') appendstr("\n");
		appendstr("[game]\n");
		if (i==1) appendstr("#You can copypaste global settings to here too,"
												" and they will override the global or core-specific settings.\n"
												"#You can also set core=C:/path/to/core_libretro.dll to pick another"
												" core for that ROM specifically.\n"
												"#Use forward slashes, even on Windows.\n"
												);
		if (bygame.config[i]._name)
		{
			appendstr("name="); appendstr(bygame.config[i]._name); appendlf();
		}
		appendstr("path="); appendstr(bygame.config[i]._path); appendlf();
		if (bygame.config[i]._forcecore) { appendstr("core="); appendstr(bygame.config[i]._forcecore); appendlf(); }
		if (bygame.config[i]._autoload) { appendstr("autoload=true\n"); }
		
		//print core options HERE if any
		
		print_config_file(&bygame.config[i], false);
	}
	
	if (outat[-2]=='\n') outat--;
	//and a null terminator
	reserve(1);
	*outat='\0';
	
#ifdef LINEBREAK_CRLF
	size_t len=0;
	for (char * outlen=outstart;*outlen;outlen++)
	{
		if (*outlen=='\n') len++;
		len++;
	}
	char * newout=malloc(len+1);
	char * newoutat=newout;
	for (char * inat=outstart;*inat;inat++)
	{
		if (*inat=='\n') *(newoutat++)='\r';
		*(newoutat++)=*inat;
	}
	*newoutat='\0';
	free(outstart);
	outstart=newout;
#else
	size_t len=outat-outstart;
#endif
	
	if (!originalconfig || strcmp(outstart, originalconfig))
	{
		file_write(path, outstart, len);
	}
	free(outstart);
}
