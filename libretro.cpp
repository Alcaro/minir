#include "minir.h"
#include "os.h"
#include "io.h"
#include "file.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "libretro.h"

namespace {

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

static bool load_raw_iface(dylib* lib, struct libretro_raw * interface)
{
#define sym(name) *(void(**)())&interface->name = (void(*)())lib->sym_func("retro_"#name); if (!interface->name) return false;
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

#ifdef __GNUC__
#define THREAD_LOCAL __thread
#endif
#ifdef _MSC_VER
#define THREAD_LOCAL __declspec(thread)
#endif

class libretro_impl;
static THREAD_LOCAL libretro_impl* g_this;

class libretro_impl : public libretro {
public:

dylib* lib;
char * libpath;
file* rom;
struct libretro_raw raw;

video* v2d;
video* v3d;
function<video*(struct retro_hw_render_callback * desc)> create3d;

video* v;
struct audio * a;
struct libretroinput * in;

void (*message_cb)(int severity, const char * message);

uint32_t feat;
uint32_t features() { return feat; }

videoformat videodepth;

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

/*private*/ void appendtmpptr(void * newptr)
{
	free(this->tmpptr[3]);
	this->tmpptr[3]=this->tmpptr[2];
	this->tmpptr[2]=this->tmpptr[1];
	this->tmpptr[1]=this->tmpptr[0];
	this->tmpptr[0]=newptr;
}

/*private*/ static char * convert_name(const char * name, const char * version)
{
	size_t len1=strlen(name);
	size_t len2=strlen(version);
	char * out=malloc(len1+1+len2+1);
	memcpy(out, name, len1);
	out[len1]=' ';
	memcpy(out+len1+1, version, len2+1);
	return out;
}

const char * name()
{
	struct retro_system_info info;
	this->raw.get_system_info(&info);
	char * out=convert_name(info.library_name, info.library_version);
	this->appendtmpptr(out);
	return out;
}

/*private*/ static char ** convert_extensions(const char * extensions, unsigned int * count)
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

const char * const * supported_extensions(unsigned int * count)
{
	static const char * no_extensions=NULL;//TODO: replace with a plain NULL
	
	struct retro_system_info info;
	this->raw.get_system_info(&info);
	if (!info.valid_extensions) return &no_extensions;
	char ** ret=convert_extensions(info.valid_extensions, count);
	this->appendtmpptr(ret);
	return (const char * const *)ret;
}

bool supports_extension(const char * extension)
{
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

/*private*/ void initialize()
{
	if (!this->initialized)
	{
		this->raw.init();
		this->initialized=true;
	}
}

void attach_interfaces(video* v, struct audio * a, struct libretroinput * i)
{
	this->v2d=v;
	if (!this->v3d) this->v=v;
	this->a=a;
	this->in=i;
}

void enable_3d(function<video*(struct retro_hw_render_callback * desc)> creator)
{
	this->create3d=creator;
}

/*private*/ static uintptr_t v3d_get_current_framebuffer()
{
	return g_this->v3d->draw_3d_get_current_framebuffer();
}

/*private*/ static retro_proc_address_t v3d_get_proc_address(const char * sym)
{
	return g_this->v3d->draw_3d_get_proc_address(sym);
}

//TODO: remove this once rarch megapack updates and its s9x exports the mmaps, alternatively once I get unlazy enough to compile s9x myself
/*private*/ void add_snes_mmap()
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

bool load_rom(file* rom)
{
	g_this=this;
	
	delete this->rom;
	this->rom=NULL;
	
	this->nummemdesc=0;
	free(this->memdesc);
	this->memdesc=NULL;
	
	this->initialize();
	
	this->v3d=NULL;
	this->v=this->v2d;
	
	if (rom)
	{
		this->rom=rom;
		
		struct retro_game_info game;
		game.path=rom->filename;
		game.data=rom->mmap();
		game.size=rom->len;
		game.meta=NULL;
		bool ret=this->raw.load_game(&game);
		rom->unmap(game.data, rom->len);
		return ret;
	}
	else if (this->feat & f_load_none)
	{
		this->rom=NULL;
		
bool ret=this->raw.load_game(NULL);
this->add_snes_mmap();
return ret;
	}
	else return false;
}

void reset()
{
	g_this=this;
	this->raw.reset();
}

void get_video_settings(unsigned int * width, unsigned int * height, videoformat * depth, double * fps)
{
	struct retro_system_av_info info;
	memset(&info, 0, sizeof(info));
	this->raw.get_system_av_info(&info);
	if (width) *width=info.geometry.base_width;
	if (height) *height=info.geometry.base_height;
	if (depth) *depth=this->videodepth;
	if (fps) *fps=info.timing.fps;
}

double get_sample_rate()
{
	struct retro_system_av_info info;
	this->raw.get_system_av_info(&info);
	return info.timing.sample_rate;
}

bool get_core_options_changed()
{
	bool ret=this->core_opt_list_changed;
	this->core_opt_list_changed=false;
	return ret;
}

const struct libretro_core_option * get_core_options(unsigned int * numopts)
{
	if (numopts) *numopts=this->core_opt_num;
	return this->core_opts;
}

void set_core_option(unsigned int option, unsigned int value)
{
	this->core_opt_current_values[option]=value;
	this->core_opt_changed=true;
}

unsigned int get_core_option(unsigned int option)
{
	return this->core_opt_current_values[option];
}

const struct retro_memory_descriptor * get_memory_info(unsigned int * nummemdesc)
{
	*nummemdesc=this->nummemdesc;
	return this->memdesc;
}

void get_memory(enum libretro_memtype which, size_t * size, void* * ptr)
{
	if (size) *size=this->raw.get_memory_size(which);
	if (ptr) *ptr=this->raw.get_memory_data(which);
}

size_t state_size()
{
	g_this=this;
	return this->raw.serialize_size();
}

bool state_save(void * state, size_t size)
{
	g_this=this;
	return this->raw.serialize(state, size);
}

bool state_load(const void * state, size_t size)
{
	g_this=this;
	return this->raw.unserialize(state, size);
}

void run()
{
	g_this=this;
	this->raw.run();
}

/*private*/ void free_core_opts()
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

~libretro_impl()
{
	if (this->initialized)
	{
		this->raw.unload_game();
		this->raw.deinit();
	}
	this->free_core_opts();
	free(this->memdesc);
	free(this->tmpptr[0]);
	free(this->tmpptr[1]);
	free(this->tmpptr[2]);
	free(this->tmpptr[3]);
	free(this->libpath);
	delete this->rom;
	delete this->lib;
}



/*private*/ static void log_callback(enum retro_log_level level, const char * fmt, ...)
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
//Done   x     xx   xxxxxx    x  x  x     x     = 13
//Todo           xxx                 xx xx xx   = 9
//Nope xx   xxx            xxx xx xx   x     x  = 14
//Gone    xx              x                     = 3
//Detailed information on why the unsupported ones don't exist can be found in this function.
/*private*/ bool environment(unsigned cmd, void* data)
{
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
	    cmd==RETRO_ENVIRONMENT_GET_LIBRETRO_PATH || //19 [TODO: this should return the path with filename. And they should all be permanent.]
	    cmd==RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY) //30
	{
		char * ret=strdup(this->libpath);
		this->appendtmpptr(ret);
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
			this->videodepth=(videoformat)newfmt;
			return true;
		}
		else return false;
	}
	//11 SET_INPUT_DESCRIPTORS, seemingly deprecated by 35 SET_CONTROLLER_INFO.
	//12 SET_KEYBOARD_CALLBACK, no supported core uses keyboards but it may be desirable.
	//13 SET_DISK_CONTROL_INTERFACE, ignored because no supported core uses disks. Maybe Famicom Disk System, but low priority.
	if (cmd==RETRO_ENVIRONMENT_SET_HW_RENDER) //14
	{
		if (!this->create3d) return false;
		struct retro_hw_render_callback * render=(struct retro_hw_render_callback*)data;
		this->v3d=this->create3d(render);
		if (!this->v3d) return false;
		this->v=this->v3d;
		render->get_current_framebuffer=v3d_get_current_framebuffer;
		render->get_proc_address=v3d_get_proc_address;
		return true;
	}
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
		this->free_core_opts();
		
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
		this->feat |= f_load_none;
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
		const char * usepath=this->rom->filename;
		if (!usepath)
		{
			if (this->feat & f_load_none) usepath=this->libpath;
			if (!usepath)
			{
				(*(const char**)data)=NULL;
				return true;
			}
		}
		char * ret=strdup(usepath);
		this->appendtmpptr(ret);
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

/*private*/ static bool environment_s(unsigned cmd, void* data)
{
	return g_this->environment(cmd, data);
}

/*private*/ static void video_refresh(const void * data, unsigned width, unsigned height, size_t pitch)
{
	if (data==RETRO_HW_FRAME_BUFFER_VALID) g_this->v->draw_3d(width, height);
	else if (data) g_this->v->draw_2d(width, height, data, pitch);
	else g_this->v->draw_repeat();
}

/*private*/ static void audio_sample(int16_t left, int16_t right)
{
	g_this->audiobuf[g_this->audiobufpos++]=left;
	g_this->audiobuf[g_this->audiobufpos++]=right;
	if (g_this->audiobufpos==audiobufsize*2)
	{
		g_this->a->render(g_this->a, audiobufsize, g_this->audiobuf);
		g_this->audiobufpos=0;
	}
}

/*private*/ static size_t audio_sample_batch(const int16_t * data, size_t frames)
{
	g_this->a->render(g_this->a, frames, data);
	return frames;//what is this one even
}

/*private*/ static void input_poll(void)
{
}

/*private*/ static int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
	return g_this->in->query(g_this->in, port, device, index, id);
}

/*private*/ bool initialize(const char * corepath, void (*message_cb)(int severity, const char * message), bool * existed)
{
	g_this=this;
	if (existed) *existed=false;
	
	this->lib=dylib::create(corepath);
	if (!this->lib) goto cancel;
	if (!load_raw_iface(this->lib, &this->raw)) goto cancel;
	if (this->raw.api_version()!=RETRO_API_VERSION) goto cancel;
	if (!this->lib->owned())
	{
		if (existed) *existed=true;
		goto cancel;
	}
	
	this->tmpptr[0]=NULL;
	this->tmpptr[1]=NULL;
	this->tmpptr[2]=NULL;
	this->tmpptr[3]=NULL;
	
	this->feat=0;
	
	this->audiobufpos=0;
	this->videodepth=fmt_xrgb1555;
	
	this->message_cb=message_cb;
	
	this->libpath=strdup(corepath);
	this->rom=NULL;
	
	this->core_opt_changed=false;
	this->core_opt_list_changed=true;
	this->core_opt_num=0;
	this->core_opts=NULL;
	this->core_opt_current_values=NULL;
	
	this->memdesc=NULL;
	this->nummemdesc=0;
	
	this->create3d=NULL;
	this->v3d=NULL;
	
	this->raw.set_environment(environment_s);
	this->raw.set_video_refresh(video_refresh);
	this->raw.set_audio_sample(audio_sample);
	this->raw.set_audio_sample_batch(audio_sample_batch);
	this->raw.set_input_poll(input_poll);
	this->raw.set_input_state(input_state);
	
	this->initialized=false;
	
	return true;
	
cancel:
	return false;
}

};
}

libretro* libretro::create(const char * corepath, void (*message_cb)(int severity, const char * message), bool * existed)
{
	libretro_impl* ret=new libretro_impl();
	if (!ret->initialize(corepath, message_cb, existed))
	{
		delete ret;
		return NULL;
	}
	return ret;
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
