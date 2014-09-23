#include "minir.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "libretro.h"

#define this This

struct libretro_raw {
	void (*set_environment)(retro_environment_t);
	void (*set_video_refresh)(retro_video_refresh_t);
	void (*set_audio_sample)(retro_audio_sample_t);
	void (*set_audio_sample_batch)(retro_audio_sample_batch_t);
	void (*set_input_poll)(retro_input_poll_t);
	void (*set_input_state)(retro_input_state_t);
	void (*init)(void);
	void (*deinit)(void);
	unsigned (*api_version)(void);
	void (*get_system_info)(struct retro_system_info * info);
	void (*get_system_av_info)(struct retro_system_av_info * info);
	void (*set_controller_port_device)(unsigned port, unsigned device);
	void (*reset)(void);
	void (*run)(void);
	size_t (*serialize_size)(void);
	bool (*serialize)(void* data, size_t size);
	bool (*unserialize)(const void* data, size_t size);
	void (*cheat_reset)(void);
	void (*cheat_set)(unsigned index, bool enabled, const char * code);
	bool (*load_game)(const struct retro_game_info * game);
	bool (*load_game_special)(unsigned game_type, const struct retro_game_info * info, size_t num_info);
	void (*unload_game)(void);
	unsigned (*get_region)(void);
	void* (*get_memory_data)(unsigned id);
	size_t (*get_memory_size)(unsigned id);
};

static bool load_raw_iface(struct dylib * lib, struct libretro_raw * interface)
{
#if defined(__GNUC__)
#define sym(name) interface->name=(__typeof(interface->name))lib->sym_func(lib, "retro_"#name); if (!interface->name) return false;
#else
#define sym(name) *(void(**)())&interface->name = (void(*)())lib->sym_func(lib, "retro_"#name); if (!interface->name) return false;
#endif
	sym(set_environment);
	sym(set_video_refresh);
	sym(set_audio_sample);
	sym(set_audio_sample_batch);
	sym(set_input_poll);
	sym(set_input_state);
	sym(init);
	sym(deinit);
	sym(api_version);
	sym(get_system_info);
	sym(get_system_av_info);
	sym(set_controller_port_device);
	sym(reset);
	sym(run);
	sym(serialize_size);
	sym(serialize);
	sym(unserialize);
	sym(cheat_reset);
	sym(cheat_set);
	sym(load_game);
	sym(load_game_special);
	sym(unload_game);
	sym(get_region);
	sym(get_memory_data);
	sym(get_memory_size);
#undef sym
	return true;
}

struct libretro_impl {
	struct libretro i;
	
	struct dylib * lib;
	char * libpath;
	char * rompath;
	struct libretro_raw raw;
	
	struct cvideo * v;
	struct audio * a;
	struct libretroinput * in;
	
	void (*message_cb)(int severity, const char * message);
	
	unsigned int videodepth;
	
	bool initialized;
	
#define audiobufsize 64
	int16_t audiobuf[audiobufsize*2];
	unsigned int audiobufpos;
	
	void * tmpptr[4];
	
	bool core_opt_changed;
	bool core_opt_list_changed;
	unsigned int core_opt_num;
	struct libretro_core_option * core_opts;
	unsigned int * core_opt_current_values;
	
	struct retro_memory_descriptor * memdesc;
	unsigned int nummemdesc;
};

#ifdef __GNUC__
static __thread struct libretro_impl * g_this;
#endif
#ifdef _MSC_VER
static __declspec(thread) struct libretro_impl * g_this;
#endif

static bool void_true(struct libretro * this)
{
	return true;
}

static bool void_false(struct libretro * this)
{
	return false;
}

static void appendtmpptr(struct libretro_impl * this, void * newptr)
{
	free(this->tmpptr[3]);
	this->tmpptr[3]=this->tmpptr[2];
	this->tmpptr[2]=this->tmpptr[1];
	this->tmpptr[1]=this->tmpptr[0];
	this->tmpptr[0]=newptr;
}

static char * convert_name(const char * name, const char * version)
{
	size_t len1=strlen(name);
	size_t len2=strlen(version);
	char * out=malloc(len1+1+len2+1);
	memcpy(out, name, len1);
	out[len1]=' ';
	memcpy(out+len1+1, version, len2+1);
	return out;
}

static const char * name(struct libretro * this_)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	struct retro_system_info info;
	this->raw.get_system_info(&info);
	char * out=convert_name(info.library_name, info.library_version);
	appendtmpptr(this, out);
	return out;
}

static char ** convert_extensions(const char * extensions, unsigned int * count)
{
	size_t datalen=1;
	size_t numptrs=2;//one for the first, one for the final NULL
	for (int i=0;extensions[i];i++)
	{
		datalen++;
		if (extensions[i]=='|') numptrs++;
	}
	
	char * str=malloc(sizeof(char*)*numptrs + datalen);
	char* * ptrs=(char**)str;
	str+=sizeof(char*)*numptrs;
	
	*ptrs=str;
	
	ptrs[0]=str;
	int ptrat=1;
	for (int i=0;extensions[i];i++)
	{
		*str=tolower(extensions[i]);
		if (*str=='|')
		{
			*str='\0';
			ptrs[ptrat++]=str+1;
		}
		str++;
	}
	*str='\0';
	ptrs[ptrat]=NULL;
	
	if (count) *count=ptrat;
	return ptrs;
}

static const char * no_extensions=NULL;
static const char * const * supported_extensions(struct libretro * this_, unsigned int * count)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	
	struct retro_system_info info;
	this->raw.get_system_info(&info);
	if (!info.valid_extensions) return &no_extensions;
	char ** ret=convert_extensions(info.valid_extensions, count);
	appendtmpptr(this, ret);
	return (const char * const *)ret;
}

static bool supports_extension(struct libretro * this_, const char * extension)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	int extlen=strlen(extension);
	
	struct retro_system_info info;
	this->raw.get_system_info(&info);
	
retry: ;
	const char * match=strstr(info.valid_extensions, extension);
	if (match)
	{
		if ((match!=info.valid_extensions && match[-1]!='|') ||//this blocks fc matching sfc
		    (match[extlen] && match[extlen]!='|'))//this blocks gb matching gbc
		{
			info.valid_extensions=match+1;
			goto retry;
		}
		return true;
	}
	else return false;
}

static void initialize(struct libretro_impl * this)
{
	if (!this->initialized)
	{
		this->raw.init();
		this->initialized=true;
	}
}

static void attach_interfaces(struct libretro * this_, struct cvideo * v, struct audio * a, struct libretroinput * i)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	this->v=v;
	this->a=a;
	this->in=i;
}

//TODO: remove this once rarch megapack updates and its s9x exports the mmaps, alternatively once I get unlazy enough to compile s9x myself
static void add_snes_mmap(struct libretro_impl * this)
{
#ifdef _WIN32
if(!this->memdesc)
{
struct retro_system_info info;
this->raw.get_system_info(&info);
if (strstr(info.library_name, "snes") || strstr(info.library_name, "SNES"))
{
struct retro_memory_descriptor desc={};
desc.start=0x7E0000;
desc.len=0x20000;
desc.ptr=this->raw.get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
if (desc.ptr)
{
this->memdesc=malloc(sizeof(struct retro_memory_descriptor)*1);
memcpy(this->memdesc,&desc,sizeof(struct retro_memory_descriptor)*1);
this->nummemdesc=1;
}
}
}
#endif
}

static bool load_rom(struct libretro * this_, const char * data, size_t datalen, const char * filename)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	
	g_this=this;
	
	free(this->rompath);
	this->rompath=NULL;
	
	free(this->memdesc);
	this->memdesc=NULL;
	this->nummemdesc=0;
	
	initialize(this);
	
	bool gameless=this->i.supports_no_game((struct libretro*)this);
	
	if (filename)
	{
		this->rompath=strdup(filename);
		
		struct retro_game_info game;
		game.path=filename;
		game.data=NULL;
		game.size=0;
		game.meta=NULL;
		if (data)
		{
			game.data=data;
			game.size=datalen;
		}
		else file_read(filename, (void**)&game.data, &game.size);
		bool ret=this->raw.load_game(&game);
		free((char*)game.data);
add_snes_mmap(this);
		return ret;
	}
	else if (data)
	{
		this->rompath=NULL;
		
		struct retro_game_info game;
		game.path=NULL;
		game.data=data;
		game.size=datalen;
		game.meta=NULL;
bool ret=this->raw.load_game(&game);
add_snes_mmap(this);
return ret;
	}
	else if (gameless)
	{
		this->rompath=strdup(this->libpath);
		return this->raw.load_game(NULL);
	}
	else return false;
}

static bool load_rom_mem_supported(struct libretro * this_)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	
	struct retro_system_info info;
	this->raw.get_system_info(&info);
	return !(info.need_fullpath);
}

static void reset(struct libretro * this_)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	g_this=this;
	this->raw.reset();
}

static void get_video_settings(struct libretro * this_, unsigned int * width, unsigned int * height, unsigned int * depth, double * fps)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	if (!this->initialized)
	{
		if (this->i.supports_no_game(this_)) initialize(this);
		else abort();
	}
	struct retro_system_av_info info;
	this->raw.get_system_av_info(&info);
	*width=info.geometry.base_width;
	*height=info.geometry.base_height;
	*depth=this->videodepth;
	*fps=info.timing.fps;
}

static double get_sample_rate(struct libretro * this_)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	struct retro_system_av_info info;
	this->raw.get_system_av_info(&info);
	return info.timing.sample_rate;
}

static bool get_core_options_changed(struct libretro * this_)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	bool ret=this->core_opt_list_changed;
	this->core_opt_list_changed=false;
	return ret;
}

static const struct libretro_core_option * get_core_options(struct libretro * this_, unsigned int * numopts)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	if (numopts) *numopts=this->core_opt_num;
	return this->core_opts;
}

static void set_core_option(struct libretro * this_, unsigned int option, unsigned int value)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	this->core_opt_current_values[option]=value;
	this->core_opt_changed=true;
}

static unsigned int get_core_option(struct libretro * this_, unsigned int option)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	return this->core_opt_current_values[option];
}

static const struct retro_memory_descriptor * get_memory_info(struct libretro * this_, unsigned int * nummemdesc)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	
	*nummemdesc=this->nummemdesc;
	return this->memdesc;
}

static void get_memory(struct libretro * this_, enum libretro_memtype which, size_t * size, void* * ptr)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	
	if (size) *size=this->raw.get_memory_size(which);
	if (ptr) *ptr=this->raw.get_memory_data(which);
}

static size_t state_size(struct libretro * this_)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	return this->raw.serialize_size();
}

static bool state_save(struct libretro * this_, void * state, size_t size)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	return this->raw.serialize(state, size);
}

static bool state_load(struct libretro * this_, const void * state, size_t size)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	return this->raw.unserialize(state, size);
}

static void run(struct libretro * this_)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	g_this=this;
	this->raw.run();
}

static void free_core_opts(struct libretro_impl * this)
{
	for (unsigned int i=0;i<this->core_opt_num;i++)
	{
		free((void*)this->core_opts[i].name_internal);
		free((void*)this->core_opts[i].name_display);
		for (unsigned int j=0;this->core_opts[i].values[j];j++)
		{
			free((void*)this->core_opts[i].values[j]);
		}
		free((void*)this->core_opts[i].values);
	}
	free((void*)this->core_opts);
	free(this->core_opt_current_values);
}

static void free_(struct libretro * this_)
{
	struct libretro_impl * this=(struct libretro_impl*)this_;
	if (this->initialized)
	{
		this->raw.unload_game();
		this->raw.deinit();
	}
	free_core_opts(this);
	free(this->memdesc);
	free(this->tmpptr[0]);
	free(this->tmpptr[1]);
	free(this->tmpptr[2]);
	free(this->tmpptr[3]);
	free(this->libpath);
	free(this->rompath);
	this->lib->free(this->lib);
	free(this);
}



static void log_callback(enum retro_log_level level, const char * fmt, ...)
{
	if (g_this->message_cb)
	{
		va_list args;
		
		char * msgdat=malloc(64);
		
		va_start(args, fmt);
		int neededlen=vsnprintf(msgdat, 64, fmt, args);
		va_end(args);
		
		if (neededlen>=64)
		{
			free(msgdat);
			msgdat=malloc(neededlen+1);
			va_start(args, fmt);
			vsnprintf(msgdat, neededlen+1, fmt, args);
			va_end(args);
		}
		
		char * drop=strchr(msgdat, '\n');
		if (drop && drop!=msgdat && drop[-1]=='\r') drop--;
		if (drop) *drop='\0';
		
		g_this->message_cb(level, msgdat);
		free(msgdat);
	}
}

//"Supported core" means a libretro core for a supported console (NES, SNES, GB, GBC, GBA).
//"Known supported core" means a supported core that has been tested somewhat.
//Brief status on support of each environ command:
//              1         2         3
//     123456789012345678901234567890123456789
//Done   x     xx    xxxxx    x  x  x     x     = 12
//Todo           xxxx                xx xx xx   = 10
//Nope xx   xxx            xxx xx xx   x     x  = 14
//Gone    xx              x                     = 3
//Detailed information on why the unsupported ones don't exist can be found in this function.
static bool environment(unsigned cmd, void* data)
{
	struct libretro_impl * this=g_this;
	//1 SET_ROTATION, no known supported core uses that. Cores are expected to deal with failures, anyways.
	//2 GET_OVERSCAN, I have no opinion. Use the default.
	if (cmd==RETRO_ENVIRONMENT_GET_CAN_DUPE) //3
	{
		*(bool*)data=true;
		return true;
	}
	//4 was removed and can safely be ignored.
	//5 was removed and can safely be ignored.
	//6 SET_MESSAGE, ignored because I don't know what to do with that.
	//7 SHUTDOWN, ignored because no supported core has any reason to have Off buttons.
	//8 SET_PERFORMANCE_LEVEL, ignored because I don't support a wide range of powers.
	if (cmd==RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY || //9
	    cmd==RETRO_ENVIRONMENT_GET_LIBRETRO_PATH || //19
	    cmd==RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY) //30
	{
		char * ret=strdup(this->libpath);
		appendtmpptr(this, ret);
		char * retend=strrchr(ret, '/');
		if (retend) *retend='\0';
		(*(const char**)data)=ret;
		return true;
	}
	if (cmd==RETRO_ENVIRONMENT_SET_PIXEL_FORMAT) //10
	{
		enum retro_pixel_format newfmt = *(enum retro_pixel_format *)data;
		if (newfmt==RETRO_PIXEL_FORMAT_0RGB1555 || newfmt==RETRO_PIXEL_FORMAT_XRGB8888 ||
				newfmt==RETRO_PIXEL_FORMAT_RGB565)
		{
			int depths[3]={15, 32, 16};
			this->videodepth=depths[newfmt];
			return true;
		}
		else return false;
	}
	//11 SET_INPUT_DESCRIPTORS, seemingly deprecated by 35 SET_CONTROLLER_INFO.
	//12 SET_KEYBOARD_CALLBACK, no supported core uses keyboards but it may be desirable.
	//13 SET_DISK_CONTROL_INTERFACE, ignored because no supported core uses disks. Maybe Famicom Disk System, but low priority.
	//14 SET_HW_RENDER, unimplemented because it's a huge thing. I'll consider it later.
	if (cmd==RETRO_ENVIRONMENT_GET_VARIABLE) //15
	{
		struct retro_variable * variable=(struct retro_variable*)data;
		
		variable->value=NULL;
		for (unsigned int i=0;i<this->core_opt_num;i++)
		{
			if (!strcmp(variable->key, this->core_opts[i].name_internal))
			{
				variable->value=this->core_opts[i].values[this->core_opt_current_values[i]];
			}
		}
		this->core_opt_changed=false;
		return true;
	}
	if (cmd==RETRO_ENVIRONMENT_SET_VARIABLES)//16
	{
		const struct retro_variable * variables=(const struct retro_variable*)data;
		free_core_opts(this);
		
		const struct retro_variable * variables_count=variables;
		while (variables_count->key) variables_count++;
		unsigned int numvars=variables_count-variables;
		
		this->core_opt_list_changed=true;
		this->core_opt_changed=true;
		this->core_opt_num=numvars;
		this->core_opts=malloc(sizeof(struct libretro_core_option)*(numvars+1));
		this->core_opt_current_values=malloc(sizeof(unsigned int)*numvars);
		
		for (unsigned int i=0;i<numvars;i++)
		{
			this->core_opts[i].name_internal=strdup(variables[i].key);
			
			const char * values=strstr(variables[i].value, "; ");
			//if the value does not contain "; ", the core is broken, and broken cores can break shit in whatever way they want, anyways.
			//let's segfault.
			unsigned int namelen=values-variables[i].value;
			values+=2;
			char* name=malloc(namelen+1);
			memcpy(name, variables[i].value, namelen);
			name[namelen]='\0';
			this->core_opts[i].name_display=name;
			
			unsigned int numvalues=1;
			const char * valuescount=values;
			while (*valuescount)
			{
				if (*valuescount=='|') numvalues++;
				valuescount++;
			}
			
			this->core_opts[i].numvalues=numvalues;
			char** values_out=malloc(sizeof(char*)*(numvalues+1));
			const char * nextvalue=values;
			for (unsigned int j=0;j<numvalues;j++)
			{
				nextvalue=values;
				while (*nextvalue && *nextvalue!='|') nextvalue++;
				unsigned int valuelen=nextvalue-values;
				values_out[j]=malloc(valuelen+1);
				memcpy(values_out[j], values, valuelen);
				values_out[j][valuelen]='\0';
				values=nextvalue+1;
			}
			values_out[numvalues]=NULL;
			this->core_opts[i].values=(const char * const *)values_out;
			//this->core_opt_possible_values[i][numvalues-1]=strdup(values);
			
			this->core_opt_current_values[i]=0;
		}
		this->core_opts[numvars].name_internal=NULL;
		this->core_opts[numvars].name_display=NULL;
		this->core_opts[numvars].numvalues=0;
		this->core_opts[numvars].values=NULL;
		
		return true;
	}
	if (cmd==RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE) //17
	{
		*(bool*)data = this->core_opt_changed;
		return true;
	}
	if (cmd==RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME) //18
	{
		if (!*(bool*)data) return true;//if this hits, someone's joking with us.
		this->i.supports_no_game=void_true;
		return true;
	}
	//19 GET_LIBRETRO_PATH, see 9.
	//20 was removed and can safely be ignored.
	if (cmd==RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK) //21
	{
		struct retro_frame_time_callback * timecb=(struct retro_frame_time_callback*)data;
		//FIXME: This should be used in libretro->run once I've figured out how to pass around the timing in a sane way.
		timecb->callback(timecb->reference);
		return true;
	}
	//22 SET_AUDIO_CALLBACK, ignored because no non-emulator is supported.
	//23 GET_RUMBLE_INTERFACE, ignored because no known supported core uses rumble, and because it's untestable.
	if (cmd==RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES) //24
	{
		*(uint64_t*)data = (1<<RETRO_DEVICE_JOYPAD);//(1 << RETRO_DEVICE_JOYPAD) | (1 << RETRO_DEVICE_ANALOG)
		return true;
	}
	//25 GET_SENSOR_INTERFACE, experimental and we're not a phone.
	//26 GET_CAMERA_INTERFACE, experimental and we're not a phone. (Okay, maybe for laptops, but show me something that uses it first.)
	if (cmd==RETRO_ENVIRONMENT_GET_LOG_INTERFACE) //27
	{
		struct retro_log_callback * logcb=(struct retro_log_callback*)data;
		logcb->log=log_callback;
		return true;
	}
	//28 GET_PERF_INTERFACE, ignored because (besides get_cpu_features) its only uses seem to be debugging and breaking fastforward.
	//29 GET_LOCATION_INTERFACE, ignored because we're not a phone.
	//30 GET_CONTENT_DIRECTORY, see 9.
	if (cmd==RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY) //31
	{
		const char * usepath=this->rompath;
		if (!usepath)
		{
			if (this->i.supports_no_game((struct libretro*)this)) usepath=this->libpath;
			if (!usepath)
			{
				(*(const char**)data)=NULL;
				return true;
			}
		}
		char * ret=strdup(usepath);
		appendtmpptr(this, ret);
		char * retend=strrchr(ret, '/');
		if (retend) *retend='\0';
		(*(const char**)data)=ret;
		return true;
	}
	//32 SET_SYSTEM_AV_INFO, should be added. Seems reasonably easy.
	//33 SET_PROC_ADDRESS_CALLBACK, ignored because there are no extensions.
	//34 SET_SUBSYSTEM_INFO, should probably be added.
	//35 SET_CONTROLLER_INFO, should probably be added.
	if (cmd==RETRO_ENVIRONMENT_SET_MEMORY_MAPS) //36
	{
		struct retro_memory_map * map=(struct retro_memory_map*)data;
		free(this->memdesc);
		this->nummemdesc=map->num_descriptors;
		this->memdesc=malloc(sizeof(struct retro_memory_descriptor)*map->num_descriptors);
		memcpy(this->memdesc, map->descriptors, sizeof(struct retro_memory_descriptor)*map->num_descriptors);
		return true;
	}
	//37 SET_GEOMETRY, should be added.
	//38 GET_USERNAME, not until I get netplay working.
	//39 GET_LANGUAGE, I don't support localization.
	
	const char * const names[]={
		"(invalid)",
		"SET_ROTATION",
		"GET_OVERSCAN",
		"GET_CAN_DUPE",
		"(removed)",
		"(removed)",
		"SET_MESSAGE",
		"SHUTDOWN",
		"SET_PERFORMANCE_LEVEL",
		"GET_SYSTEM_DIRECTORY",
		"SET_PIXEL_FORMAT",
		"SET_INPUT_DESCRIPTORS",
		"SET_KEYBOARD_CALLBACK",
		"SET_DISK_CONTROL_INTERFACE",
		"SET_HW_RENDER",
		"GET_VARIABLE",
		"SET_VARIABLES",
		"GET_VARIABLE_UPDATE",
		"SET_SUPPORT_NO_GAME",
		"GET_LIBRETRO_PATH",
		"(removed)",
		"SET_FRAME_TIME_CALLBACK",
		"SET_AUDIO_CALLBACK",
		"GET_RUMBLE_INTERFACE",
		"GET_INPUT_DEVICE_CAPABILITIES",
		"GET_SENSOR_INTERFACE",
		"GET_CAMERA_INTERFACE",
		"GET_LOG_INTERFACE",
		"GET_PERF_INTERFACE",
		"GET_LOCATION_INTERFACE",
		"GET_CONTENT_DIRECTORY",
		"GET_SAVE_DIRECTORY",
		"SET_SYSTEM_AV_INFO",
		"SET_PROC_ADDRESS_CALLBACK",
		"SET_SUBSYSTEM_INFO",
		"SET_CONTROLLER_INFO",
		"SET_MEMORY_MAPS",
		"SET_GEOMETRY",
		"GET_USERNAME",
		"GET_LANGUAGE",
	};
	if ((cmd&~RETRO_ENVIRONMENT_EXPERIMENTAL) < sizeof(names)/sizeof(*names))
	{
		log_callback(RETRO_LOG_WARN, "Unsupported environ command #%u %s.", cmd, names[cmd&~RETRO_ENVIRONMENT_EXPERIMENTAL]);
	}
	else log_callback(RETRO_LOG_WARN, "Unsupported environ command #%u.", cmd);
	return false;
}

static void video_refresh(const void * data, unsigned width, unsigned height, size_t pitch)
{
	g_this->v->draw(g_this->v, width, height, data, pitch);
}

static void audio_sample(int16_t left, int16_t right)
{
	g_this->audiobuf[g_this->audiobufpos++]=left;
	g_this->audiobuf[g_this->audiobufpos++]=right;
	if (g_this->audiobufpos==audiobufsize*2)
	{
		g_this->a->render(g_this->a, audiobufsize, g_this->audiobuf);
		g_this->audiobufpos=0;
	}
}

static size_t audio_sample_batch(const int16_t * data, size_t frames)
{
	g_this->a->render(g_this->a, frames, data);
	return frames;//what is this one even
}

static void input_poll(void)
{
}

static int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
	return g_this->in->query(g_this->in, port, device, index, id);
}

struct libretro libretro_iface = {
	name, supported_extensions, supports_extension,
	void_false,//supports_no_game
	attach_interfaces,
	load_rom, load_rom_mem_supported,
	get_video_settings, get_sample_rate,
	get_core_options_changed, get_core_options, set_core_option, get_core_option,
	get_memory, get_memory_info, reset,
	state_size, state_save, state_load,
	run,
	free_
};

struct libretro * libretro_create(const char * corepath, void (*message_cb)(int severity, const char * message), bool * existed)
{
	struct libretro_impl * this=malloc(sizeof(struct libretro_impl));
	memcpy(&this->i, &libretro_iface, sizeof(libretro_iface));
	
	g_this=this;
	if (existed) *existed=false;
	
	this->lib=dylib_create(corepath);
	if (!this->lib) goto cancel;
	if (!load_raw_iface(this->lib, &this->raw)) goto cancel;
	if (this->raw.api_version()!=RETRO_API_VERSION) goto cancel;
	if (!this->lib->owned(this->lib))
	{
		if (existed) *existed=true;
		goto cancel;
	}
	
	this->tmpptr[0]=NULL;
	this->tmpptr[1]=NULL;
	this->tmpptr[2]=NULL;
	this->tmpptr[3]=NULL;
	
	this->audiobufpos=0;
	this->videodepth=15;
	
	this->message_cb=message_cb;
	
	this->libpath=strdup(corepath);
	this->rompath=NULL;
	
	this->core_opt_changed=false;
	this->core_opt_list_changed=true;
	this->core_opt_num=0;
	this->core_opts=NULL;
	this->core_opt_current_values=NULL;
	
	this->memdesc=NULL;
	this->nummemdesc=0;
	
	this->raw.set_environment(environment);
	this->raw.set_video_refresh(video_refresh);
	this->raw.set_audio_sample(audio_sample);
	this->raw.set_audio_sample_batch(audio_sample_batch);
	this->raw.set_input_poll(input_poll);
	this->raw.set_input_state(input_state);
	
	this->initialized=false;
	
	return (struct libretro*)this;
	
cancel:
	if (this->lib) this->lib->free(this->lib);
	free(this);
	return NULL;
}



#ifndef WINDOW_MINIMAL
static char* * corepaths;
static size_t corenum;
static size_t corebuflen;

static char * core_this_path;
static size_t core_this_path_len;
static int core_this_path_offsets[5];

static const char * dylibext;

static void core_set_path_component(const char * path, int depth, bool bottom)
{
	size_t thislen=strlen(path);
	if (core_this_path_offsets[depth]+thislen>=core_this_path_len)
	{
		while (core_this_path_offsets[depth]+thislen>=core_this_path_len) core_this_path_len*=2;
		core_this_path=realloc(core_this_path, core_this_path_len);
	}
	strcpy(core_this_path+core_this_path_offsets[depth], path);
	if (bottom)
	{
		core_this_path[core_this_path_offsets[depth]+thislen]='\0';
	}
	else
	{
		core_this_path[core_this_path_offsets[depth]+thislen]='/';
		core_this_path[core_this_path_offsets[depth]+thislen+1]='\0';
	}
	core_this_path_offsets[depth+1]=core_this_path_offsets[depth]+thislen+1;
}

static void core_append(const char * path, int depth)
{
	if (corenum==corebuflen)
	{
		corebuflen*=2;
		corepaths=realloc(corepaths, sizeof(char*)*corebuflen);
	}
	if (path)
	{
		core_set_path_component(path, depth, true);
		corepaths[corenum]=strdup(core_this_path);
	}
	else corepaths[corenum]=NULL;
	corenum++;
}

static void core_look_in_path(const char * path, bool look_in_all, bool look_in_retro, bool delete_non_retro, int depth)
{
	core_set_path_component(path, depth, false);
	
	char* childpath;
	bool isdir;
	void* find=file_find_create(core_this_path);
	
	while (file_find_next(find, &childpath, &isdir))
	{
		if (isdir)
		{
			if (look_in_retro && (strstr(childpath, "retro") || strstr(childpath, "core")))
			{
				core_look_in_path(childpath, look_in_all, false, false, depth+1);
			}
			else if (look_in_all)
			{
				core_look_in_path(childpath, false, look_in_retro, delete_non_retro, depth+1);
			}
		}
		if (!isdir && (!delete_non_retro || strstr(childpath, "retro")) && strstr(childpath, dylibext))
		{
			core_append(childpath, depth+1);
		}
		free(childpath);
	}
	file_find_close(find);
}

static void core_setup_search()
{
	for (size_t i=0;i<corenum;i++) free(corepaths[i]);
	free(corepaths);
	
	core_this_path_len=64;
	core_this_path=malloc(sizeof(char)*core_this_path_len);
	core_this_path_offsets[0]=0;
	
	corenum=0;
	corebuflen=4;
	corepaths=malloc(sizeof(char*)*corebuflen);
	
	dylibext=dylib_ext();
}

const char * const * libretro_default_cores()
{
	core_setup_search();
	
	const char * selfpath=window_get_proc_path();
	const char * selfpathend=strrchr(selfpath, '/');
	
	//if minir is run out of a browser download dir, don't look for cores there
	if (!strstr(selfpathend, "download") && !strstr(selfpathend, "Download"))
	{
		core_look_in_path(selfpath, true, true, false, 0);
	}
#ifdef DYLIB_POSIX
	core_look_in_path("/lib", true, true, true, 0);
	core_look_in_path("/usr/lib", true, true, true, 0);
	core_look_in_path("/usr/local/lib", true, true, true, 0);
#endif
#ifdef DYLIB_WIN32
	//no plausible sys lib paths
#endif
	
	core_append(NULL, 0);
	free(core_this_path);
	
	return (const char * const *)corepaths;
}

const char * const * libretro_nearby_cores(const char * rompath)
{
	core_setup_search();
	
	core_look_in_path(rompath, true, true, false, 0);
	
	core_append(NULL, 0);
	free(core_this_path);
	
	return (const char * const *)corepaths;
}
#endif
