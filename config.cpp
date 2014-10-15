#include "minir.h"
#include <string.h>
//#include <strings.h> //strcasecmp
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#define this This

#define MINIZ_HEADER_FILE_ONLY
#include "miniz.c"

#define count(array) (sizeof(array)/sizeof(*(array)))

#define nop(x) (x)
#define strdup_s(x) ((x) ? strdup(x) : NULL)

struct minirconfig_impl {
	struct minirconfig i;
	
	struct configdata global;//we can be reasonably sure that this one does not have an unaligned size
	
	struct configdata * bycore;
	struct configdata * bygame;
	unsigned int numbycore;
	unsigned int numbygame;
	
	const char * autoload;
	
	char * originalconfig;
	
	bool firstrun;
	//char padding[7];
};

//nonstatic so I can tell Valgrind to suppress _Z22initialize_to_defaultsP10configdata
void initialize_to_defaults(struct configdata * this)
{
	memset(this, 0, sizeof(struct configdata));
	for (unsigned int i=0;i<count(this->_scopes);i++) this->_scopes[i]=cfgsc_default;
#define CONFIG_CLEAR_DEFAULTS
#include "obj/generated.cpp"
#undef CONFIG_CLEAR_DEFAULTS
}

static void initialize_to_parent(struct configdata * this)
{
	memset(this, 0, sizeof(struct configdata));
	for (unsigned int i=0;i<count(this->_scopes);i++) this->_scopes[i]=cfgsc_default;
}

static void join_config(struct configdata * parent, struct configdata * child)
{
	if (!child) return;
#define JOIN(groupname, scopegroupname, deleteold, clone) \
		for (unsigned int i=0;i<count(parent->groupname);i++) \
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

static size_t create_core(struct minirconfig_impl * this)
{
	if ((this->numbycore & (this->numbycore-1)) == 0)
	{
		this->bycore=realloc(this->bycore, sizeof(struct configdata)*this->numbycore*2);
	}
	initialize_to_parent(&this->bycore[this->numbycore]);
	this->numbycore++;
	return this->numbycore-1;
}

static size_t find_core(struct minirconfig_impl * this, const char * core)
{
	size_t i;
	for (i=1;i<this->numbycore;i++)
	{
		if (!strcmp(core, this->bycore[i]._corepath)) break;
	}
	return i;
}

static size_t find_or_create_core(struct minirconfig_impl * this, const char * core)
{
	if (core==NULL) return 0;
	
	size_t i=find_core(this, core);
	if (i==this->numbycore)
	{
		create_core(this);
		this->bycore[i]._corepath=strdup(core);
		this->bycore[i].support=malloc(sizeof(char*));
		this->bycore[i].support[0]=NULL;
		this->bycore[i].primary=malloc(sizeof(char*));
		this->bycore[i].primary[0]=NULL;
		return i;
	}
	return i;
}

static size_t create_game(struct minirconfig_impl * this)
{
	if ((this->numbygame & (this->numbygame-1)) == 0)
	{
		this->bygame=realloc(this->bygame, sizeof(struct configdata)*this->numbygame*2);
	}
	initialize_to_parent(&this->bygame[this->numbygame]);
	this->numbygame++;
	return this->numbygame-1;
}

static size_t find_game(struct minirconfig_impl * this, const char * game)
{
	size_t i;
	for (i=1;i<this->numbygame;i++)
	{
		if (!strcmp(game, this->bygame[i]._gamepath)) break;
	}
	return i;
}

static size_t find_or_create_game(struct minirconfig_impl * this, const char * game)
{
	if (game==NULL) return 0;
	
	size_t i=find_game(this, game);
	if (i==this->numbygame)
	{
		create_game(this);
		this->bygame[i]._gamepath=strdup(game);
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

static int find_place(char* * start, int len, char* newitem)
{
	int jumpsize=bitround(len+1)/2;
	int pos=0;
	while (jumpsize)
	{
		if (pos<0) pos+=jumpsize;
		else if (pos>=len) pos-=jumpsize;
		else if (strcmp(start[pos], newitem)<0) pos+=jumpsize;
		else pos-=jumpsize;
		jumpsize/=2;
	}
	if (pos<0) pos+=1;
	else if (pos>=len) pos-=0;
	else if (strcmp(start[pos], newitem)<0) pos+=1;
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



static void split_config(struct configdata * Public, struct configdata * global,
												 struct configdata * bycore, struct configdata * bygame)
{
#define SPLIT(groupname, scopegroupname, deleteold, clone) \
		for (unsigned int i=0;i<count(Public->groupname);i++) \
		{ \
			if(0); \
			else if (Public->scopegroupname[i] >= cfgsc_game) \
			{ \
				(void)deleteold(bygame->groupname[i]); \
				bygame->groupname[i]=clone(Public->groupname[i]); \
			} \
			else if (Public->scopegroupname[i] >= cfgsc_core) \
			{ \
				(void)deleteold(bygame->groupname[i]); \
				bycore->groupname[i]=clone(Public->groupname[i]); \
			} \
			else \
			{ \
				(void)deleteold(global->groupname[i]); \
				global->groupname[i]=clone(Public->groupname[i]); \
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



static void set_support(struct minirconfig_impl * this, unsigned int id, char * * support_in, char * * primary_in)
{
	//check if the support list is unchanged (it should be unless this core is fresh)
	if (support_in)
	{
		for (unsigned int i=0;true;i++)
		{
			if (!this->bycore[id].support[i] && !support_in[i])
			{
				support_in=NULL;
				break;
			}
			if (!this->bycore[id].support[i] || !support_in[i] || strcmp(this->bycore[id].support[i], support_in[i])) break;
		}
	}
	//same for primary
	if (primary_in)
	{
		for (unsigned int i=0;true;i++)
		{
			if (!this->bycore[id].primary[i] && !primary_in[i])
			{
				primary_in=NULL;
				break;
			}
			if (!this->bycore[id].primary[i] || !primary_in[i] || strcmp(this->bycore[id].primary[i], primary_in[i])) break;
		}
	}
	
	if (!support_in && !primary_in) return;
	
	for (unsigned int i=0;this->bycore[id].primary[i];i++) free(this->bycore[id].primary[i]);
	this->bycore[id].primary[0]=NULL;
	
	size_t primary_buflen=2;
	char* * primary=malloc(sizeof(char*)*primary_buflen);
	size_t primary_count=0;
	
	if (support_in)
	{
		for (unsigned int i=0;this->bycore[id].support[i];i++) free(this->bycore[id].support[i]);
		this->bycore[id].support[0]=NULL;
		
		size_t support_buflen=2;
		char* * support=malloc(sizeof(char*)*support_buflen);
		size_t support_count=0;
		
		for (size_t i=0;support_in[i];i++)
		{
			//check if it's already known by this core; if it is, discard it
			for (size_t j=0;j<support_count;j++)
			{
				if (!strcmp(support[j], support_in[i])) goto nope;
			}
			
			support[support_count]=strdup(support_in[i]);
			support_count++;
			if (support_count==support_buflen)
			{
				support_buflen*=2;
				support=realloc(support, sizeof(char*)*support_buflen);
			}
			
			//check if it's known by other cores; if it is, do not set it as primary
			for (size_t j=1;j<this->numbycore;j++)
			{
				for (size_t k=0;this->bycore[j].primary[k];k++)
				{
					if (!strcmp(this->bycore[j].primary[k], support_in[i])) goto nope;
				}
			}
			
			primary[primary_count]=strdup(support_in[i]);
			primary_count++;
			if (primary_count==primary_buflen)
			{
				primary_buflen*=2;
				primary=realloc(primary, sizeof(char*)*primary_buflen);
			}
		nope: ;
		}
		
		support[support_count]=NULL;
		free(this->bycore[id].support);
		this->bycore[id].support=support;
	}
	
	if (primary_in)
	{
		for (size_t i=0;primary_in[i];i++)
		{
			//if we're already primary for this one, we don't need to set anything
			for (size_t j=0;j<primary_count;j++)
			{
				if (!strcmp(primary[j], primary_in[i])) goto nope2;
			}
			
			//someone else has claimed this extension as their primary, throw them out
			
			for (size_t j=1;j<this->numbycore;j++)
			{
				for (size_t k=0;this->bycore[j].primary[k];k++)
				{
					if (!strcmp(this->bycore[j].primary[k], primary_in[i]))
					{
						//found it; let's shift the others
						while (this->bycore[j].primary[k])
						{
							this->bycore[j].primary[k]=this->bycore[j].primary[k+1];
							k++;
						}
						break;
					}
				}
			}
			
			primary[primary_count]=strdup(primary_in[i]);
			primary_count++;
			if (primary_count==primary_buflen)
			{
				primary_buflen*=2;
				primary=realloc(primary, sizeof(char*)*primary_buflen);
			}
			
		nope2: ;
		}
	}
	
	primary[primary_count]=NULL;
	free(this->bycore[id].primary);
	this->bycore[id].primary=primary;
	
	sort_and_clean_core_support(&this->bycore[id]);
}

static const char * get_autoload(struct minirconfig * this_)
{
	struct minirconfig_impl * this=(struct minirconfig_impl*)this_;
	return this->autoload;
}

static const char * * get_supported_extensions(struct minirconfig * this_)
{
	struct minirconfig_impl * this=(struct minirconfig_impl*)this_;
	
	unsigned int numret=0;
	for (unsigned int i=1;i<this->numbycore;i++)
	{
		unsigned int j;
		for (j=0;this->bycore[i].primary[j];j++) {}
		numret+=j;
	}
	
	const char * * ret=malloc(sizeof(const char*)*(numret+1));
	ret[numret]=NULL;
	numret=0;
	for (unsigned int i=1;i<this->numbycore;i++)
	{
		unsigned int j;
		for (j=0;this->bycore[i].primary[j];j++) {}
		
		memcpy(ret+numret, this->bycore[i].primary, sizeof(const char*)*j);
		numret+=j;
	}
	return ret;
}

static struct configcorelist * get_core_for(struct minirconfig * this_, const char * gamepath, unsigned int * count)
{
	struct minirconfig_impl * this=(struct minirconfig_impl*)this_;
	
	size_t gameid=find_game(this, gamepath);
	if (gameid!=this->numbygame)
	{
		if (this->bygame[gameid]._forcecore)
		{
			size_t coreid=find_core(this, this->bygame[gameid]._forcecore);
			//size_t coreid=find_or_create_core(this->bygame[gameid]._forcecore);
			if (coreid!=this->numbycore)
			{
				struct configcorelist * ret=malloc(sizeof(struct configcorelist)*2);
				ret[0].path=this->bygame[gameid]._forcecore;
				ret[0].name=this->bycore[coreid].corename;
				ret[1].path=NULL;
				ret[1].name=NULL;
				if (count) *count=1;
				return ret;
			}
			else
			{
				free(this->bygame[gameid]._forcecore);
				this->bygame[gameid]._forcecore=NULL;
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
		for (unsigned int i=1;i<this->numbycore;i++)
		{
			bool thisprimary=false;
			for (int j=0;this->bycore[i].primary[j];j++)
			{
				if (!strcmp(extension, this->bycore[i].primary[j]))
				{
					thisprimary=true;
				}
			}
			for (int j=0;this->bycore[i].support[j];j++)
			{
				if (!strcmp(extension, this->bycore[i].support[j]))
				{
					if (thisprimary && numret!=0)
					{
						//memmove(ret+1, ret, sizeof(struct configcorelist)*(numret-1));
						ret[numret].path=ret[0].path;
						ret[numret].name=ret[0].name;
						ret[0].path=this->bycore[i]._corepath;
						ret[0].name=this->bycore[i].corename;
					}
					else
					{
						ret[numret].path=this->bycore[i]._corepath;
						ret[numret].name=this->bycore[i].corename;
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

static void data_load(struct minirconfig * this_, struct configdata * config,
                      bool free_old, const char * corepath, const char * gamepath)
{
	struct minirconfig_impl * this=(struct minirconfig_impl*)this_;
	if (free_old) delete_conf(config);
	
	initialize_to_defaults(config);
	join_config(config, &this->global);
	
	if (corepath)
	{
		char * truecore=window_get_absolute_path(corepath);
		unsigned int id=find_or_create_core(this, truecore);
		free(truecore);
		config->corename=this->bycore[id].corename;
		config->_corepath=this->bycore[id]._corepath;
		config->support=this->bycore[id].support;
		config->primary=this->bycore[id].primary;
		join_config(config, &this->bycore[id]);
	}
	else
	{
		config->corename=NULL;
		config->support=malloc(sizeof(char*));
		config->support[0]=NULL;
		config->primary=malloc(sizeof(char*));
		config->primary[0]=NULL;
	}
	
	if (gamepath)
	{
		char * truegame=window_get_absolute_path(gamepath);
		unsigned int id=find_or_create_game(this, truegame);
		free(truegame);
		join_config(config, &this->bygame[id]);
		config->gamename=this->bygame[id].gamename;
		config->_gamepath=this->bygame[id]._gamepath;
	}
	else config->gamename=NULL;
	
	for (unsigned int i=0;i<count(config->inputs);i++) config->inputs[i]=strdup_s(config->inputs[i]);
	for (unsigned int i=0;i<count(config->_strings);i++) config->_strings[i]=strdup_s(config->_strings[i]);
	
	unsigned int i;
	for (i=0;config->support[i];i++) {}
	i++;//for the NULL
	char * * oldsupport=config->support;
	config->support=malloc(sizeof(char*)*i);
	for (i=0;oldsupport[i];i++) config->support[i]=strdup(oldsupport[i]);
	config->support[i]=NULL;
	
	for (i=0;config->primary[i];i++) {}
	i++;
	char * * oldprimary=config->primary;
	config->primary=malloc(sizeof(char*)*i);
	for (i=0;oldprimary[i];i++) config->primary[i]=strdup(oldprimary[i]);
	config->primary[i]=NULL;
	
	if (config->corename) config->corename=strdup(config->corename);
	if (config->gamename) config->gamename=strdup(config->gamename);
	if (config->_corepath) config->_corepath=strdup(config->_corepath);
	if (config->_gamepath) config->_gamepath=strdup(config->_gamepath);
	
	config->firstrun=this->firstrun;
}

static void data_save(struct minirconfig * this_, struct configdata * config)
{
	struct minirconfig_impl * this=(struct minirconfig_impl*)this_;
	size_t coreid=find_or_create_core(this, config->_corepath);
	size_t gameid=find_or_create_game(this, config->_gamepath);
	split_config(config, &this->global, &this->bycore[coreid], &this->bygame[gameid]);
	
	if (coreid)
	{
		set_support(this, coreid, config->support, config->primary);
		free(this->bycore[coreid].corename);
		this->bycore[coreid].corename=strdup_s(config->corename);
	}
	if (gameid)
	{
		free(this->bygame[gameid].gamename);
		this->bygame[gameid].gamename=strdup_s(config->gamename);
		//free(this->bygame[gameid]._forcecore);
		//this->bygame[gameid]._forcecore=strdup(config->_forcecore);
	}
}

static void data_free(struct minirconfig * this_, struct configdata * config)
{
	delete_conf(config);
}

static void data_destroy(struct minirconfig * this_, const char * item)
{
	struct minirconfig_impl * this=(struct minirconfig_impl*)this_;
	
	size_t id=find_core(this, item);
	if (id!=this->numbycore)
	{
		delete_conf(&this->bycore[id]);
		memmove(&this->bycore[id], &this->bycore[id+1], sizeof(struct configdata)*(this->numbycore-id));
		this->numbycore--;
	}
	
	id=find_game(this, item);
	if (id!=this->numbygame)
	{
		free(this->bygame[id]._forcecore);
		this->bygame[id]._forcecore=NULL;
		delete_conf(&this->bygame[id]);
		memmove(&this->bygame[id], &this->bygame[id+1], sizeof(struct configdata)*(this->numbygame-id));
		this->numbygame--;
	}
}



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
	CFGB_STR_MULTI,
	CFGB_STR_MAP,
};
static const unsigned char config_bytecode_comp[]={
#define CONFIG_BYTECODE
#include "obj/generated.cpp"
#undef CONFIG_BYTECODE
};
static unsigned char config_bytecode[CONFIG_BYTECODE_LEN];


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

static void print_config_file(struct configdata * this, unsigned char minscope, bool fullarrays, bool comments)
{
	const unsigned char * thisone=config_bytecode;
	const unsigned char * arrayshuffle;
	const char * arraynames;
	int arraynamelen;
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
			if (comments)
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
		
		if(0);
		else if (*at==CFGB_INPUT) at+=3;
		else if (*at==CFGB_STR) at+=3;
		else if (*at==CFGB_INT) at+=11;
		else if (*at==CFGB_UINT) at+=11;
		//else if (*at==CFGB_ENUM) at++;
		else if (*at==CFGB_BOOL) at+=3;
		else if (*at==CFGB_STR_MULTI) at+=3;
		else if (*at==CFGB_STR_MAP) at+=3;
		
		for (int i=0;i<arraylen;i++)
		{
			if (itemtype==CFGB_INPUT && this->_scopes_input[baseoffset+i]<minscope) continue;
			if (itemtype==CFGB_STR && this->_scopes_str[baseoffset+i]<minscope) continue;
			if (itemtype==CFGB_INT && this->_scopes_int[baseoffset+i]<minscope) continue;
			if (itemtype==CFGB_UINT && this->_scopes_uint[baseoffset+i]<minscope) continue;
			if (itemtype==CFGB_BOOL && this->_scopes_bool[baseoffset+i]<minscope) continue;
			if (itemtype==CFGB_STR_MULTI && this->_scopes_strlist[baseoffset+i]<minscope) continue;
			if (itemtype==CFGB_STR_MAP && this->_scopes_strmap[baseoffset+i]<minscope) continue;
			
			if (i>=arrayend && !fullarrays)
			{
				if (itemtype==CFGB_INPUT && !this->inputs[baseoffset+i]) continue;
				if (itemtype==CFGB_STR && !this->_strings[baseoffset+i]) continue;
				if (itemtype==CFGB_INT && !this->_ints[baseoffset+i]) continue;
				if (itemtype==CFGB_UINT && !this->_uints[baseoffset+i]) continue;
				if (itemtype==CFGB_BOOL && !this->_bools[baseoffset+i]) continue;
				if (itemtype==CFGB_STR_MULTI && !this->_strlists[baseoffset+i]) continue;
				if (itemtype==CFGB_STR_MAP && !this->_strmaps[baseoffset+i]) continue;
			}
			
			char varname[256];
			unsigned int varnamepos=0;
			
			int arrayoffset=0;
			memcpy(varname+varnamepos, at+1, *at); varnamepos+=*at;
			if (arraylen!=1)
			{
				int dynlen=arraynamelen;
				while (arraynames[arraynamelen*i + dynlen-1]=='\0') dynlen--;
				memcpy(varname+varnamepos, arraynames + arraynamelen*i, dynlen); varnamepos+=dynlen;
				arrayoffset=arrayshuffle?arrayshuffle[i]:i;
			}
			int offset=baseoffset+arrayoffset;
			varname[varnamepos++]='=';
			if (itemtype!=CFGB_STR_MULTI && itemtype!=CFGB_STR_MAP)
			{
				appenddat(varname, varnamepos);
				if (itemtype==CFGB_INPUT)
				{
					appendstr(this->inputs[offset]?this->inputs[offset]:"");
				}
				if (itemtype==CFGB_STR)
				{
					appendstr(this->_strings[offset]?this->_strings[offset]:"");
				}
				if (itemtype==CFGB_INT)
				{
					char buf[32];
					sprintf(buf, "%i", this->_ints[offset]);
					appendstr(buf);
				}
				if (itemtype==CFGB_UINT)
				{
					char buf[32];
					sprintf(buf, "%u", this->_uints[offset]);
					appendstr(buf);
				}
				if (itemtype==CFGB_BOOL)
				{
					appendstr(this->_bools[offset]?"true":"false");
				}
				appendlf();
			}
			else
			{
				char** tmp;
				if (itemtype==CFGB_STR_MULTI)
				{
					tmp=this->_strlists[offset];
				}
				else
				{
					tmp=this->_strmaps[offset];
					varname[varnamepos-1]='_';
				}
				if (!tmp || !*tmp)
				{
					appenddat(varname, varnamepos);//set the list to an empty string
					if (itemtype==CFGB_STR_MAP) appendstr("=");
					appendlf();
					continue;
				}
				while (*tmp)
				{
					appenddat(varname, varnamepos);
					appendstr(*tmp);
					appendlf();
					tmp++;
				}
			}
		}
		
		thisone=at+1+at[0];
	}
}

static void write(struct minirconfig * this_, const char * path)
{
	struct minirconfig_impl * this=(struct minirconfig_impl*)this_;
	
	outstart=malloc(8192);
	outat=outstart;
	outend=outstart+8192;
	appendstr("[global]\n");
	print_config_file(&this->global, (this->global.verbosity>=cfgv_default ? cfgsc_default : cfgsc_global),
	                  this->global.verbosity>=cfgv_maximum, this->global.verbosity>=cfgv_default);
	
	for (unsigned int i=1;i<this->numbycore;i++)
	{
		if (outat[-2]!='\n') appendlf();
		appendstr("[core]\n");
		if (this->global.verbosity>=cfgv_default && i==1)
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
		if (this->bycore[i].corename)
		{
			appendstr("name="); appendstr(this->bycore[i].corename); appendlf();
		}
		appendstr("path="); appendstr(this->bycore[i]._corepath); appendlf();
		for (unsigned int j=0;this->bycore[i].primary[j];j++)
		{
			appendstr("primary=");
			appendstr(this->bycore[i].primary[j]);
			appendlf();
		}
		for (unsigned int j=0;this->bycore[i].support[j];j++)
		{
			appendstr("support=");
			appendstr(this->bycore[i].support[j]);
			appendlf();
		}
		
		//print core options HERE
		
		print_config_file(&this->bycore[i], cfgsc_core, false, false);
	}
	
	for (unsigned int i=1;i<this->numbygame;i++)
	{
		if (outat[-2]!='\n') appendstr("\n");
		appendstr("[game]\n");
		if (this->global.verbosity>=cfgv_default && i==1)
		{
			appendstr("#You can copypaste global settings to here too,"
			          " and they will override the global or core-specific settings.\n"
			          "#You can also set core=C:/path/to/core_libretro.dll to pick another"
			          " core for that ROM specifically.\n"
#ifdef _WIN32
			          "#Use forward slashes.\n"
#endif
			          );
		}
		if (this->bygame[i].gamename)
		{
			appendstr("name="); appendstr(this->bygame[i].gamename); appendlf();
		}
		appendstr("path="); appendstr(this->bygame[i]._gamepath); appendlf();
		if (this->bygame[i]._forcecore) { appendstr("core="); appendstr(this->bygame[i]._forcecore); appendlf(); }
		if (this->bygame[i]._autoload) { appendstr("autoload=true\n"); }
		
		//print core options HERE if any
		
		print_config_file(&this->bygame[i], cfgsc_game, false, false);
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
	
	if (!this->originalconfig || strcmp(outstart, this->originalconfig))
	{
		file_write(path, outstart, len);
		free(this->originalconfig);
		this->originalconfig=outstart;
	}
	else free(outstart);
}



static void free_(struct minirconfig * this_)
{
	struct minirconfig_impl * this=(struct minirconfig_impl*)this_;
	delete_conf(&this->global);
	for (size_t i=0;i<this->numbycore;i++) delete_conf(&this->bycore[i]);
	for (size_t i=0;i<this->numbygame;i++)
	{
		free(this->bygame[i]._forcecore);
		this->bygame[i]._forcecore=NULL;
		delete_conf(&this->bygame[i]);
	}
	free(this->originalconfig);
	free(this);
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

static void load_var_from_file(const char * name, const char * value, struct configdata * thisconf, unsigned char scope)
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
		bool global=false;
		
		if (*at==CFGB_GLOBAL)
		{
			at++;
			global=true;
		}
		
		if (*at==CFGB_ARRAY)
		{
			//we can't put this inside the memcmp, as the memcmp'd code doesn't always run, and we could hit CFGB_ARRAY_SAME after that
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
		
		unsigned char type=*at;
		if(0);
		else if (type==CFGB_INPUT) at+=3;
		else if (type==CFGB_STR) at+=3;
		else if (type==CFGB_INT) at+=11;
		else if (type==CFGB_UINT) at+=11;
		//else if (type==CFGB_ENUM) at++;
		else if (type==CFGB_BOOL) at+=3;
		else if (type==CFGB_STR_MULTI) at+=3;
		else if (type==CFGB_STR_MAP) at+=3;
		
		int namelen=*at;
		
		if (!memcmp(name, (char*)at+1, namelen) && (array || name[namelen]=='\0' || (name[namelen]=='_' && type==CFGB_STR_MAP)))
		{
			if (global && scope!=cfgsc_global) goto cont;
			const unsigned char * at2=thisone;
			
			if (*at2==CFGB_GLOBAL) at2++;
			
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
			if (*at2==CFGB_STR_MULTI)
			{
				//if (load_var_bool(value, &thisconf->_bools[offset])) thisconf->_scopes_bool[offset]=scope;
			}
			if (*at2==CFGB_STR_MAP)
			{
				//if (load_var_bool(value, &thisconf->_bools[offset])) thisconf->_scopes_bool[offset]=scope;
			}
		}
	cont:
		thisone=at+1+namelen;
	}
}

static void read_from_file(struct minirconfig_impl * this, char * rawconfig)
{
	this->originalconfig=strdup(rawconfig);
	
	unsigned char thisscope=cfgsc_global;
	
	size_t support_buflen=0;
	char* * support=NULL;
	size_t support_count=0;
	
	size_t primary_buflen=0;
	char* * primary=NULL;
	size_t primary_count=0;
	
	char * thisline=rawconfig;
	
	struct configdata * thisconf=NULL;
#define finishsection() \
			if (thisconf) \
			{ \
				if (thisscope==cfgsc_core && !thisconf->_corepath) \
				{ \
					delete_conf(thisconf); \
					this->numbycore--; \
				} \
				else if (thisscope==cfgsc_game && !thisconf->_gamepath) \
				{ \
					delete_conf(thisconf); \
					this->numbygame--; \
				} \
				else \
				{ \
					if (thisconf->_autoload) \
					{ \
						if (this->autoload) thisconf->_autoload=false; \
						else this->autoload=strdup(thisconf->_gamepath); \
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
			else if (!strcmp(thisline, "[global]")) { thisconf=&this->global; thisscope=cfgsc_global; }
			else if (!strcmp(thisline, "[core]"))
			{
				size_t id=create_core(this);
				thisconf=&this->bycore[id];
				thisscope=cfgsc_core;
				
				support_buflen=2;
				support=malloc(sizeof(char*)*support_buflen);
				primary_buflen=2;
				primary=malloc(sizeof(char*)*primary_buflen);
			}
			else if (!strcmp(thisline, "[game]")) { size_t id=create_game(this); thisconf=&this->bygame[id]; thisscope=cfgsc_game; }
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
	for (unsigned int i=1;i<this->numbycore;i++)
	{
		//unsigned int supportcount;
		//for (supportcount=0;this->bycore[i].support[supportcount];supportcount++) {}
		unsigned int primarycount;
		//if (this->bycore[i].primary)
		//{
			for (primarycount=0;this->bycore[i].primary[primarycount];primarycount++) {}
		//}
		//else primarycount=0;
		for (unsigned int j=0;this->bycore[i].support[j];j++)
		{
			for (unsigned int l=0;l<primarycount;l++)
			{
				if (!strcmp(this->bycore[i].support[j], this->bycore[i].primary[l])) goto foundit;
			}
			for (unsigned int k=1;k<this->numbycore;k++)
			{
				if (k==i) continue;
				for (unsigned int l=0;this->bycore[k].primary[l];l++)
				{
					if (!strcmp(this->bycore[i].support[j], this->bycore[k].primary[l])) goto foundit;
				}
			}
			//this has poor complexity, but shouldn't be run often
			this->bycore[i].primary=realloc(this->bycore[i].primary, sizeof(char*)*(primarycount+2));
			this->bycore[i].primary[primarycount]=strdup(this->bycore[i].support[j]);
			this->bycore[i].primary[primarycount+1]=NULL;
			primarycount++;
		foundit:;
		}
	}
}

struct minirconfig * config_create(const char * path)
{
	struct minirconfig_impl * this=malloc(sizeof(struct minirconfig_impl));
	memset(this, 0, sizeof(*this));
	this->i.get_autoload=get_autoload;
	this->i.get_supported_extensions=get_supported_extensions;
	this->i.get_core_for=get_core_for;
	this->i.data_load=data_load;
	this->i.data_save=data_save;
	this->i.data_free=data_free;
	this->i.data_destroy=data_destroy;
	this->i.write=write;
	this->i.free=free_;
	
	tinfl_decompress_mem_to_mem(config_bytecode, CONFIG_BYTECODE_LEN, config_bytecode_comp, sizeof(config_bytecode_comp), 0);
	
	this->numbycore=1;
	this->bycore=malloc(sizeof(struct configdata));
	initialize_to_parent(&this->bycore[0]);
	
	this->numbygame=1;
	this->bygame=malloc(sizeof(struct configdata));
	initialize_to_parent(&this->bygame[0]);
	
	initialize_to_defaults(&this->global);
	
	char * rawconfig;
	this->firstrun=true;
	if (file_read(path, (void**)&rawconfig, NULL))
	{
		this->firstrun=false;
		read_from_file(this, rawconfig);
		free(rawconfig);
	}
	
	return (struct minirconfig*)this;
}
