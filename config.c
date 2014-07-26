#include "minir.h"
#include <string.h>
#include <strings.h> //strcasecmp
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

struct configdata g_config={};
struct configdata g_defaults;

#define count(array) (sizeof(array)/sizeof(*(array)))

#define nop(x) (x)
#define strdup_s(x) ((x) ? strdup(x) : NULL)

static struct configdata * global=NULL;

struct varying {
	struct configdata * config;
	size_t num;
	size_t buflen;
	size_t this;
};

static struct varying bycore;
static struct varying bygame;

static const char * autoload=NULL;

static char * originalconfig=NULL;

static void initialize_to_defaults(struct configdata * this)
{
	memset(this, 0, sizeof(struct configdata));
	for (int i=0;i<count(this->_scopes);i++) this->_scopes[i]=cfgsc_default;
#define CONFIG_CLEAR_DEFAULTS
#include "obj/generated.c"
#undef CONFIG_CLEAR_DEFAULTS
}

static void initialize_to_parent(struct configdata * this)
{
	memset(this, 0, sizeof(struct configdata));
	for (int i=0;i<count(this->_scopes);i++) this->_scopes[i]=cfgsc_default;
}

static void join_config(struct configdata * parent, struct configdata * child)
{
	if (!child) return;
#define JOIN(groupname, scopegroupname, deleteold, clone) \
		for (int i=0;i<count(parent->groupname);i++) \
		{ \
			if (child->scopegroupname[i] > parent->scopegroupname[i]) \
			{ \
				(void)deleteold(parent->groupname[i]); \
				parent->groupname[i]=child->groupname[i]; \
			} \
		}
	JOIN(inputs,_scopes_input,free,strdup_s);
	JOIN(_strings,_scopes_str,free,strdup_s);
	JOIN(_ints,_scopes_int,nop,nop);
	JOIN(_uints,_scopes_uint,nop,nop);
	JOIN(_enums,_scopes_enum,nop,nop);
	JOIN(_bools,_scopes_bool,nop,nop);
#undef JOIN
}

static size_t create_core()
{
	if (bycore.num==bycore.buflen)
	{
		bycore.buflen*=2;
		bycore.config=realloc(bycore.config, bycore.buflen*sizeof(struct configdata));
	}
	initialize_to_parent(&bycore.config[bycore.num]);
	bycore.num++;
	return bycore.num-1;
}

static size_t find_core(const char * core)
{
	size_t i;
	for (i=1;i<bycore.num;i++)
	{
		if (!strcmp(core, bycore.config[i]._corepath)) break;
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
		bycore.config[i]._corepath=strdup(core);
		bycore.config[i].support=malloc(sizeof(char*));
		bycore.config[i].support[0]=NULL;
		bycore.config[i].primary=malloc(sizeof(char*));
		bycore.config[i].primary[0]=NULL;
		return i;
	}
	return i;
}

static size_t create_game()
{
	if (bygame.num==bygame.buflen)
	{
		bygame.buflen*=2;
		bygame.config=realloc(bygame.config, bygame.buflen*sizeof(struct configdata));
	}
	initialize_to_parent(&bygame.config[bygame.num]);
	bygame.num++;
	return bygame.num-1;
}

static size_t find_game(const char * game)
{
	size_t i;
	for (i=1;i<bygame.num;i++)
	{
		if (!strcmp(game, bygame.config[i]._gamepath)) break;
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
		bygame.config[i]._gamepath=strdup(game);
		return i;
	}
	return i;
}

static void delete_conf(struct configdata * this)
{
	free(this->_corepath);
	free(this->_gamepath);
	free(this->corename);
	free(this->gamename);
	
	for (unsigned int i=0;i<count(this->inputs);i++) free(this->inputs[i]);
	for (unsigned int i=0;i<count(this->_strings);i++) free(this->_strings[i]);
	
	if (this->support) for (unsigned int i=0;this->support[i];i++) free(this->support[i]);
	if (this->primary) for (unsigned int i=0;this->primary[i];i++) free(this->primary[i]);
	free(this->support);
	free(this->primary);
	
	memset(this, 0, sizeof(*this));
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

static void sort_and_clean_core_support(struct configdata * core)
{
	char* * unsorted;
	unsigned int numunsorted;
	char* * sorted;
	unsigned int numsorted;
	
	
	//sort/uniq support
	unsorted=core->support;
	for (numunsorted=0;unsorted[numunsorted];numunsorted++) {}
	sorted=unsorted;
	numsorted=0;
	
	while (numunsorted)
	{
		char* this=*unsorted;
		unsigned int newpos=find_place(sorted, numsorted, this);
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
	
	
	//sort/uniq primary, delete non-support
	char* * support=core->support;
	unsigned int numsupport=numsorted;
	
	unsorted=core->primary;
	for (numunsorted=0;unsorted[numunsorted];numunsorted++) {}
	sorted=unsorted;
	numsorted=0;
	
	while (numunsorted)
	{
		char* this=*unsorted;
		unsigned int newpos=find_place(sorted, numsorted, this);
		
		unsigned int supportpos=find_place(support, numsupport, this);
		
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
}



const char * config_get_autoload()
{
	return autoload;
}



const char * * config_get_supported_extensions()
{
	unsigned int numret=0;
	for (unsigned int i=0;i<bycore.num;i++)
	{
		unsigned int j;
		for (j=0;bycore.config[i].primary[j];j++) {}
		numret+=j;
	}
	
	const char * * ret=malloc(sizeof(const char*)*(numret+1));
	ret[numret]=NULL;
	numret=0;
	for (int i=0;i<bycore.num;i++)
	{
		unsigned int j;
		for (j=0;bycore.config[i].primary[j];j++) {}
		
		memcpy(ret+numret, bycore.config[i].primary, sizeof(const char*)*j);
		numret+=j;
	}
	return ret;
}



static void split_config(struct configdata * public, struct configdata * global,
												 struct configdata * bycore, struct configdata * bygame)
{
#define SPLIT(groupname, scopegroupname, deleteold, clone) \
		for (int i=0;i<count(public->groupname);i++) \
		{ \
			if(0); \
			else if (public->scopegroupname[i] >= cfgsc_game) \
			{ \
				(void)deleteold(bygame->groupname[i]); \
				bygame->groupname[i]=clone(public->groupname[i]); \
			} \
			else if (public->scopegroupname[i] >= cfgsc_core) \
			{ \
				(void)deleteold(bygame->groupname[i]); \
				bycore->groupname[i]=clone(public->groupname[i]); \
			} \
			else \
			{ \
				(void)deleteold(global->groupname[i]); \
				global->groupname[i]=clone(public->groupname[i]); \
			} \
		}
	SPLIT(inputs,_scopes_input,free,strdup_s);
	SPLIT(_strings,_scopes_str,free,strdup_s);
	SPLIT(_ints,_scopes_int,nop,nop);
	SPLIT(_uints,_scopes_uint,nop,nop);
	SPLIT(_enums,_scopes_enum,nop,nop);
	SPLIT(_bools,_scopes_bool,nop,nop);
#undef SPLIT
}

void config_load(const char * corepath, const char * gamepath)
{
	join_config(&g_config, global);
	
	if (corepath)
	{
		char * truecore=window_get_absolute_path(corepath);
		bycore.this=find_or_create_core(truecore);
		free(truecore);
		g_config.corename=bycore.config[bycore.this].corename;
		g_config.support=bycore.config[bycore.this].support;
		g_config.primary=bycore.config[bycore.this].primary;
		join_config(&g_config, &bycore.config[bycore.this]);
	}
	else
	{
		g_config.corename=NULL;
		g_config.support=malloc(sizeof(char*));
		g_config.support[0]=NULL;
		g_config.primary=malloc(sizeof(char*));
		g_config.primary[0]=NULL;
	}
	
	if (gamepath)
	{
		char * truegame=window_get_absolute_path(gamepath);
		bygame.this=find_or_create_game(truegame);
		free(truegame);
		join_config(&g_config, &bygame.config[bygame.this]);
		g_config.gamename=bygame.config[bygame.this].gamename;
	}
	else g_config.gamename=NULL;
}



void config_create_core(const char * core, bool override_existing, const char * name, const char * const * supported_extensions)
{
	char * truecore=window_get_absolute_path(core);
	size_t id=find_or_create_core(truecore);
	free(truecore);
	
	if (override_existing || !bycore.config[id].corename)
	{
		free(bycore.config[id].corename);
		bycore.config[id].corename=strdup(name);
	}
	
	if (override_existing || !bycore.config[id].support[0])
	{
		for (unsigned int i=0;bycore.config[id].support[i];i++) free(bycore.config[id].support[i]);
		for (unsigned int i=0;bycore.config[id].primary[i];i++) free(bycore.config[id].primary[i]);
		free(bycore.config[id].support);
		free(bycore.config[id].primary);
		
		size_t support_buflen=2;
		char* * support=malloc(sizeof(char*)*support_buflen);
		size_t support_count=0;
		
		size_t primary_buflen=2;
		char* * primary=malloc(sizeof(char*)*primary_buflen);
		size_t primary_count=0;
		
		if (supported_extensions)
		{
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
				
				for (size_t j=1;j<bycore.num;j++)
				{
					for (size_t k=0;bycore.config[j].primary[k];k++)
					{
						if (!strcmp(bycore.config[j].primary[k], supported_extensions[i])) goto nope;
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
		}
		
		support[support_count]=NULL;
		primary[primary_count]=NULL;
		
		bycore.config[id].support=support;
		bycore.config[id].primary=primary;
		
		sort_and_clean_core_support(&bycore.config[id]);
	}
}

void config_create_game(const char * game, bool override_existing, const char * name)
{
	char * truegame=window_get_absolute_path(game);
	size_t id=find_or_create_game(truegame);
	free(truegame);
	
	if (override_existing || !bygame.config[id].gamename)
	{
		free(bygame.config[id].gamename);
		bygame.config[id].gamename=strdup(name);
	}
}

void config_delete_core(const char * core)
{
	size_t id=find_core(core);
	if (id==bycore.num) return;
	
	if (bycore.this==id) config_load(NULL, g_config.gamename);
	if (bycore.this>id) bycore.this--;
	
	memmove(&bycore.config[id], &bycore.config[id+1], sizeof(*bycore.config)*(bycore.num-id));
	bycore.num--;
}

void config_delete_game(const char * game)
{
	size_t id=find_core(game);
	if (id==bygame.num) return;
	
	if (bygame.this==id) config_load(g_config.corename, NULL);
	if (bygame.this>id) bygame.this--;
	
	memmove(&bygame.config[id], &bygame.config[id+1], sizeof(*bygame.config)*(bygame.num-id));
	bygame.num--;
}



struct configcorelist * config_get_core_for(const char * gamepath, unsigned int * count)
{
	size_t gameid=find_game(gamepath);
	if (gameid!=bygame.num)
	{
		if (bygame.config[gameid]._forcecore)
		{
			size_t coreid=find_core(bygame.config[gameid]._forcecore);
			if (coreid!=bycore.num)
			{
				struct configcorelist * ret=malloc(sizeof(struct configcorelist)*2);
				ret[0].path=bycore.config[coreid]._corepath;
				ret[0].name=bycore.config[coreid].corename;
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
	struct configcorelist * ret=malloc(sizeof(struct configcorelist)*retbuflen);
	if (extension && !strchr(extension, '/'))
	{
		extension++;
		for (int i=1;i<bycore.num;i++)
		{
			bool thisprimary=false;
			for (int j=0;bycore.config[i].support[j];j++)
			{
				if (bycore.config[i].primary[j] && !strcmp(extension, bycore.config[i].primary[j]))
				{
					thisprimary=true;
				}
				if (!strcmp(extension, bycore.config[i].support[j]))
				{
					if (thisprimary && numret!=0)
					{
						//memmove(ret+1, ret, sizeof(struct configcorelist)*(numret-1));
						ret[numret].path=ret[0].path;
						ret[numret].name=ret[0].name;
						ret[0].path=bycore.config[i]._corepath;
						ret[0].name=bycore.config[i].corename;
					}
					else
					{
						ret[numret].path=bycore.config[i]._corepath;
						ret[numret].name=bycore.config[i].corename;
					}
					
					numret++;
					if (numret==retbuflen)
					{
						retbuflen*=2;
						ret=realloc(ret, sizeof(struct configcorelist)*retbuflen);
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



static bool load_var_numu(const char * str, unsigned int * out, unsigned int rangelower, unsigned int rangeupper)
{
	const char * end;
	unsigned int tmp=strtoul(str, (char**)&end, 0);
	if (*str && !*end)
	{
		if (tmp<rangelower) *out=rangelower;
		else if (tmp>rangeupper) *out=rangeupper;
		else *out=tmp;
		return true;
	}
	return false;
}

static bool load_var_nums(const char * str, signed int * out, int rangelower, int rangeupper)
{
	const char * end;
	signed int tmp=strtol(str, (char**)&end, 0);
	if (*str && !*end)
	{
		if (tmp<rangelower) *out=rangelower;
		else if (tmp>rangeupper) *out=rangeupper;
		else *out=tmp;
		return true;
	}
	return false;
}

static bool load_var_bool(const char * str, bool * out)
{
	if (!strcasecmp(str, "true")) { *out=true; return true; }
	if (!strcasecmp(str, "false")) { *out=false; return true; }
	if (!strcmp(str, "1")) { *out=true; return true; }
	if (!strcmp(str, "0")) { *out=false; return true; }
	return false;
}

static bool load_var_str(const char * str, char* * out)
{
	free(*out);
	if (*str) *out=strdup(str);
	else *out=NULL;
	return true;
}

static bool load_var_input(const char * str, char* * out)
{
	if (!*str)
	{
		free(*out);
		*out=NULL;
		return true;
	}
	
	char * newout=inputmapper_normalize(str);
	if (!newout) return false;
	free(*out);
	*out=newout;
	return true;
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
#include "obj/generated.c"
#undef CONFIG_BYTECODE
};
unsigned char config_bytecode[CONFIG_BYTECODE_LEN];

static void load_var_from_file(const char * name, const char * value, struct configdata * thisconf, unsigned char scope)
{
	const unsigned char * thisone=config_bytecode;
	const unsigned char * arrayshuffle=arrayshuffle;//these ones are initialized by ARRAY and ARRAY_SHUFFLED
	const char * arraynames=arraynames;             //ARRAY_SAME is impossible unless one of those have happened
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
		
		if(0);
		else if (*at==CFGB_INPUT) at+=3;
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
				if (load_var_input(value, &thisconf->inputs[offset])) thisconf->_scopes_input[offset]=scope;
			}
			if (*at2==CFGB_STR)
			{
				if (load_var_str(value, &thisconf->_strings[offset])) thisconf->_scopes_str[offset]=scope;
			}
			if (*at2==CFGB_INT || *at2==CFGB_UINT)
			{
				unsigned int rangelow=(at2[3]<<24)|(at2[4]<<16)|(at2[5]<<8)|(at2[6]<<0);
				unsigned int rangehigh=(at2[7]<<24)|(at2[8]<<16)|(at2[9]<<8)|(at2[10]<<0);
				if (*at2==CFGB_INT)
				{
					if (load_var_nums(value, &thisconf->_ints[offset], -(int)rangelow, rangehigh))
					{
						thisconf->_scopes_int[offset]=scope;
					}
				}
				else
				{
					if (load_var_numu(value, &thisconf->_uints[offset], rangelow, rangehigh))
					{
						thisconf->_scopes_uint[offset]=scope;
					}
				}
			}
			if (*at2==CFGB_ENUM)
			{
				//readbool(value, &thisconf->_bools[offset], &thisconf->_overrides_bool[offset]);
			}
			if (*at2==CFGB_BOOL)
			{
				if (load_var_bool(value, &thisconf->_bools[offset])) thisconf->_scopes_bool[offset]=scope;
			}
		}
	cont:
		thisone=at+1+namelen;
	}
}

void config_read(const char * path)
{
	tinfl_decompress_mem_to_mem(config_bytecode, CONFIG_BYTECODE_LEN, config_bytecode_comp, sizeof(config_bytecode_comp), 0);
	
	bycore.num=1;
	bycore.buflen=1;
	bycore.config=malloc(sizeof(struct configdata));
	initialize_to_parent(&bycore.config[0]);
	bycore.this=0;
	
	bygame.num=1;
	bygame.buflen=1;
	bygame.config=malloc(sizeof(struct configdata));
	initialize_to_parent(&bygame.config[0]);
	bygame.this=0;
	
	struct configdata * thisconf=NULL;
	
	global=malloc(sizeof(struct configdata));
	initialize_to_parent(global);
	
	char * rawconfig;
	if (!file_read(path, &rawconfig, NULL)) return;
	
	originalconfig=strdup(rawconfig);
	
	unsigned char thisscope=cfgsc_global;
	
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
				if (thisscope==cfgsc_core && !thisconf->_corepath) \
				{ \
					delete_conf(thisconf); \
					bycore.num--; \
				} \
				else if (thisscope==cfgsc_game && !thisconf->_gamepath) \
				{ \
					delete_conf(thisconf); \
					bygame.num--; \
				} \
				else \
				{ \
					if (thisconf->_autoload) \
					{ \
						if (autoload) thisconf->_autoload=false; \
						else autoload=strdup(thisconf->_gamepath); \
					} \
					if (support) \
					{ \
						support[support_count]=NULL; \
						thisconf->support=support; \
					} \
					if (primary) \
					{ \
						primary[primary_count]=NULL; \
						thisconf->primary=primary; \
					} \
					if (thisscope==cfgsc_core) sort_and_clean_core_support(thisconf); \
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
			else if (!strcmp(thisline, "[global]")) { thisconf=global; thisscope=cfgsc_global; }
			else if (!strcmp(thisline, "[core]"))
			{
				size_t id=create_core();
				thisconf=&bycore.config[id];
				thisscope=cfgsc_core;
				
				support_buflen=2;
				support=malloc(sizeof(char*)*support_buflen);
				primary_buflen=2;
				primary=malloc(sizeof(char*)*primary_buflen);
			}
			else if (!strcmp(thisline, "[game]")) { size_t id=create_game(); thisconf=&bygame.config[id]; thisscope=cfgsc_game; }
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
			
			if (thisscope==cfgsc_core && !strcmp(thisline, "path"))
			{
				free(thisconf->_corepath);
				thisconf->_corepath=window_get_absolute_path(value);
			}
			if (thisscope==cfgsc_game && !strcmp(thisline, "path"))
			{
				free(thisconf->_gamepath);
				thisconf->_gamepath=window_get_absolute_path(value);
			}
			
			if (thisscope==cfgsc_core && !strcmp(thisline, "name"))
			{
				free(thisconf->corename);
				thisconf->corename=strdup(value);
			}
			if (thisscope==cfgsc_game && !strcmp(thisline, "name"))
			{
				free(thisconf->gamename);
				thisconf->gamename=strdup(value);
			}
			if (thisscope==cfgsc_core && !strcmp(thisline, "support"))
			{
				support[support_count]=strdup(value);
				support_count++;
				if (support_count==support_buflen)
				{
					support_buflen*=2;
					support=realloc(support, sizeof(char*)*support_buflen);
				}
			}
			if (thisscope==cfgsc_core && !strcmp(thisline, "primary"))
			{
				primary[primary_count]=strdup(value);
				primary_count++;
				if (primary_count==primary_buflen)
				{
					primary_buflen*=2;
					primary=realloc(primary, sizeof(char*)*primary_buflen);
				}
			}
			
			if (thisscope==cfgsc_game && !strcmp(thisline, "autoload"))
			{
				bool b=false;
				if (load_var_bool(value, &b)) thisconf->_autoload=b;
			}
			if (thisscope==cfgsc_game && !strcmp(thisline, "core"))
			{
				free(thisconf->_forcecore);
				thisconf->_forcecore=strdup(value);
			}
			load_var_from_file(thisline, value, thisconf, thisscope);
		}
		
		if (!nextline) break;
		thisline=nextline+1;
	}
	finishsection();
	
	//find support without corresponding primary
	for (unsigned int i=1;i<bycore.num;i++)
	{
		//unsigned int supportcount;
		//for (supportcount=0;bycore.config[i].support[supportcount];supportcount++) {}
		unsigned int primarycount;
		//if (bycore.config[i].primary)
		//{
			for (primarycount=0;bycore.config[i].primary[primarycount];primarycount++) {}
		//}
		//else primarycount=0;
		for (unsigned int j=0;bycore.config[i].support[j];j++)
		{
			for (unsigned int l=0;l<primarycount;l++)
			{
				if (!strcmp(bycore.config[i].support[j], bycore.config[i].primary[l])) goto foundit;
			}
			for (unsigned int k=1;k<bycore.num;k++)
			{
				if (k==i) continue;
				for (unsigned int l=0;bycore.config[k].primary[l];l++)
				{
					if (!strcmp(bycore.config[i].support[j], bycore.config[k].primary[l])) goto foundit;
				}
			}
			//this has poor complexity, but shouldn't be run often
			bycore.config[i].primary=realloc(bycore.config[i].primary, sizeof(char*)*(primarycount+2));
			bycore.config[i].primary[primarycount]=strdup(bycore.config[i].support[j]);
			bycore.config[i].primary[primarycount+1]=NULL;
			primarycount++;
		foundit:;
		}
	}
	
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

static void print_config_file(struct configdata * this, unsigned char verbosity, unsigned char minscope)
{
	const unsigned char * thisone=config_bytecode;
	const unsigned char * arrayshuffle=arrayshuffle;//these ones are initialized by ARRAY and ARRAY_SHUFFLED
	const char * arraynames=arraynames;             //ARRAY_SAME is impossible unless one of those have happened
	int arraynamelen=arraynamelen;
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
			if (verbosity>=cfgv_default)
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
			if (itemtype==CFGB_INPUT && this->_scopes_input[baseoffset+i]<minscope) continue;
			if (itemtype==CFGB_STR && this->_scopes_str[baseoffset+i]<minscope) continue;
			if (itemtype==CFGB_INT && this->_scopes_int[baseoffset+i]<minscope) continue;
			if (itemtype==CFGB_UINT && this->_scopes_uint[baseoffset+i]<minscope) continue;
			if (itemtype==CFGB_BOOL && this->_scopes_bool[baseoffset+i]<minscope) continue;
			
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
				if (this->_scopes_input[offset]>=minscope)
				{
					appendstr(this->inputs[offset]?this->inputs[offset]:"");
				}
			}
			if (itemtype==CFGB_STR)
			{
				if (this->_scopes_str[offset]>=minscope)
				{
					appendstr(this->_strings[offset]?this->_strings[offset]:"");
				}
			}
			if (itemtype==CFGB_INT)
			{
				if (this->_scopes_int[offset]>=minscope)
				{
					sprintf(buf, "%i", this->_ints[offset]);
					appendstr(buf);
				}
			}
			if (itemtype==CFGB_UINT)
			{
				if (this->_scopes_uint[offset]>=minscope)
				{
					sprintf(buf, "%u", this->_uints[offset]);
					appendstr(buf);
				}
			}
			if (itemtype==CFGB_BOOL)
			{
				if (this->_scopes_bool[offset]>=minscope)
				{
					appendstr(this->_bools[offset]?"true":"false");
				}
			}
			appendlf();
		}
		
		thisone=at+1+at[0];
	}
}

void config_write(unsigned char verbosity, const char * path)
{
	split_config(&g_config, global, &bycore.config[bycore.this], &bygame.config[bygame.this]);
	
	outstart=malloc(8192);
	outat=outstart;
	outend=outstart+8192;
	appendstr("[global]\n");
	print_config_file(global, verbosity, (verbosity==cfgv_default ? cfgsc_default : cfgsc_global));
	
	for (unsigned int i=1;i<bycore.num;i++)
	{
		if (outat[-2]!='\n') appendlf();
		appendstr("[core]\n");
		if (verbosity>=cfgv_default && i==1)
		{
			appendstr("#minir will only set core-specific entries here by default,"
			          "but you can copy any setting from [global] if you want to.\n"
			          "#You will, of course, still be able to change it from within minir.\n"
			          "#However, you can't change whether something is core-specific from within minir.\n"
			          "#Anything that starts core-specific stays that way, "
			              "even if it's set to the same value as the global one.\n"
			          "#Additionally, you can't copy anything originating from here into other sections; "
			              "that wouldn't make sense.\n"
			          );
		}
		if (bycore.config[i].corename)
		{
			appendstr("name="); appendstr(bycore.config[i].corename); appendlf();
		}
		appendstr("path="); appendstr(bycore.config[i]._corepath); appendlf();
		for (unsigned int j=0;bycore.config[i].primary[j];j++)
		{
			appendstr("primary=");
			appendstr(bycore.config[i].primary[j]);
			appendlf();
		}
		for (unsigned int j=0;bycore.config[i].support[j];j++)
		{
			appendstr("support=");
			appendstr(bycore.config[i].support[j]);
			appendlf();
		}
		
		//print core options HERE
		
		print_config_file(&bycore.config[i], verbosity, cfgsc_core);
	}
	
	for (unsigned int i=1;i<bygame.num;i++)
	{
		if (outat[-2]!='\n') appendstr("\n");
		appendstr("[game]\n");
		if (i==1) appendstr("#You can copypaste global settings to here too,"
												" and they will override the global or core-specific settings.\n"
												"#You can also set core=C:/path/to/core_libretro.dll to pick another"
												" core for that ROM specifically.\n"
#ifdef _WIN32
												"#Use forward slashes.\n"
#endif
												);
		if (bygame.config[i].gamename)
		{
			appendstr("name="); appendstr(bygame.config[i].gamename); appendlf();
		}
		appendstr("path="); appendstr(bygame.config[i]._gamepath); appendlf();
		if (bygame.config[i]._forcecore) { appendstr("core="); appendstr(bygame.config[i]._forcecore); appendlf(); }
		if (bygame.config[i]._autoload) { appendstr("autoload=true\n"); }
		
		//print core options HERE if any
		
		print_config_file(&bygame.config[i], verbosity, cfgsc_game);
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





//Reads the config from disk.
void config_read(const char * path);

//Tells which game to autoload. Can be NULL if none. Don't free it.
const char * config_get_autoload();

//Returns { "smc", "sfc", NULL } for all possible values. Free it when you're done, but don't free the pointers inside.
const char * * config_get_supported_extensions();

//Tells which cores support this game. Send it to free() when you're done; however, the pointers
// inside should not be freed. If a game specifies it should be opened with one specific core, only that is returned.
//count can be NULL if you don't care. Alternatively, it's terminated with a { NULL, NULL }.
struct configcorelist * config_get_core_for(const char * gamepath, unsigned int * count);

//Populates struct config with the core- or game-specific options for this. Will memorize all changed config.
//NULL is valid for either or both of them. It is not an error if a given entry doesn't exist.
//If the given game demands a specific core, the given core will be ignored. The game will always be honored unless it's NULL.
void config_load(const char * corepath, const char * gamepath);

bool config_core_supports(const char * core, const char * extension);
void config_create_core(const char * core, bool override_existing, const char * name, const char * const * supported_extensions);
void config_create_game(const char * game, bool override_existing, const char * name);
void config_set_primary_core(const char * core, const char * extension);
void config_delete_core(const char * core);
void config_delete_game(const char * game);

//Writes the changes back to the file, if there are any changes.
void config_write(unsigned char verbosity, const char * path);

struct minirconfig_impl {
	struct minirconfig i;
	
	struct configdata global;//we can be reasonably sure that this one does not have an unaligned size
	
	struct configdata * bycore;
	struct configdata * bygame;
	unsigned int numbycore;
	unsigned int numbygame;
	
	const char * autoload;
	
	char * originalconfig;
};

static const char * get_autoload(struct minirconfig * this_);
static const char * * get_supported_extensions(struct minirconfig * this_);
static struct configcorelist * get_core_for(struct minirconfig * this_, const char * gamepath, unsigned int * count);
static void data_load(struct minirconfig * this_, struct configdata * config,
                      bool free_old, const char * corepath, const char * gamepath);
static void data_save(struct minirconfig * this_, struct configdata * config);
static void data_free(struct minirconfig * this_, struct configdata * config);
static void data_destroy(struct minirconfig * this_, const char * item);
static void write(struct minirconfig * this_, unsigned char verbosity, const char * path);
static void free_(struct minirconfig * this_);

static const char * get_autoload(struct minirconfig * this_)
{
	return config_get_autoload();
}

static const char * * get_supported_extensions(struct minirconfig * this_)
{
	return config_get_supported_extensions();
}

static struct configcorelist * get_core_for(struct minirconfig * this_, const char * gamepath, unsigned int * count)
{
	return config_get_core_for(gamepath, count);
}

static void data_load(struct minirconfig * this_, struct configdata * config,
                      bool free_old, const char * corepath, const char * gamepath)
{
	if (free_old) data_free(this_, config);
	
	initialize_to_defaults(&g_config);
	config_load(corepath, gamepath);
	memcpy(config, &g_config, sizeof(*config));
	
	for (int i=0;i<count(config->inputs);i++) config->inputs[i]=strdup_s(config->inputs[i]);
	for (int i=0;i<count(config->_strings);i++) config->_strings[i]=strdup_s(config->_strings[i]);
	g_config.support=NULL;
	g_config.primary=NULL;
	delete_conf(&g_config);
}

static void data_save(struct minirconfig * this_, struct configdata * config)
{
	memcpy(&g_config, config, sizeof(*config));
	split_config(&g_config, global, &bycore.config[bycore.this], &bygame.config[bygame.this]);
	memset(&g_config, 0, sizeof(*config));
}

static void data_free(struct minirconfig * this_, struct configdata * config)
{
	delete_conf(config);
}

static void data_destroy(struct minirconfig * this_, const char * item)
{
	config_delete_core(item);
	config_delete_game(item);
}

static void write(struct minirconfig * this_, unsigned char verbosity, const char * path)
{
	config_write(verbosity, path);
}

static void free_(struct minirconfig * this_)
{
	struct minirconfig_impl * this=(struct minirconfig_impl*)this_;
	free(this);
}

static bool first=true;
struct minirconfig * config_create(const char * path)
{
	if (!first) return NULL;
	first=false;
	
	struct minirconfig_impl * this=malloc(sizeof(struct minirconfig_impl));
	this->i.get_autoload=get_autoload;
	this->i.get_supported_extensions=get_supported_extensions;
	this->i.get_core_for=get_core_for;
	this->i.data_load=data_load;
	this->i.data_save=data_save;
	this->i.data_free=data_free;
	this->i.data_destroy=data_destroy;
	this->i.write=write;
	this->i.free=free_;
	
	config_read(path);
	
	return (struct minirconfig*)this;
}
