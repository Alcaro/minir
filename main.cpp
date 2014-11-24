#include "minir.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#define VERSION "0.91"

//yes, this file is a mess; the plan is to rewrite it from scratch.

/*
//not supported by gcc 4.8
#if __i386__ || __x86_64__
#include <x86intrin.h>
void unalign_lock()   { __writeeflags(__readeflags()| (1<<18)); }
void unalign_unlock() { __writeeflags(__readeflags()&~(1<<18)); }
#else
void unalign_lock()   {}
void unalign_unlock() {}
#endif
*/
extern void unalign_lock();
extern void unalign_unlock();
#if __x86_64__
__asm__(
"unalign_lock:\n"
"pushf\n"
"movl $(1<<18),%eax\n"
"orl %eax,(%rsp)\n"
"popf\n"
"ret\n"
"unalign_unlock:\n"
"pushf\n"
"movl $(~(1<<18)),%eax\n"
"andl %eax,(%rsp)\n"
"popf\n"
"ret\n"
);
#elif __i386__
__asm__(
"_unalign_lock:\n"
"pushf\n"
"movl $(1<<18),%eax\n"
"orl %eax,(%esp)\n"
"popf\n"
"ret\n"
"_unalign_unlock:\n"
"pushf\n"
"movl $(~(1<<18)),%eax\n"
"andl %eax,(%esp)\n"
"popf\n"
"ret\n"
);
#else
void unalign_lock() {}
void unalign_unlock() {}
#endif


#define rewind rewind_//go away, stdio

#ifndef HAVE_ASPRINTF
void asprintf(char * * ptr, const char * fmt, ...);
#else
//if I cast it to void, that means I do not care, so shut the hell up about warn_unused_result.
static inline void shutupgcc(int x){}
#define asprintf(...) shutupgcc(asprintf(__VA_ARGS__))
#endif

video* vid;
video* vid3d;
struct audio * aud;
struct inputmapper * inp;
struct libretroinput * retroinp;
struct window * wndw;
struct windowmenu * menu;
struct widget_viewport * draw;
struct libretro * core;
struct rewindstack * rewind;
struct minircheats * cheats;

bool load_rom(const char * rom);
bool load_core_as_rom(const char * rom);
void load_rom_finish();
char * coreloaded=NULL;
char * romloaded=NULL;
char * romname=NULL;

void deinit();
bool exit_called=false;

//points to the ROM, split by "/home/admin/minir/roms/zeldaseasons" ".gbc"; fits "-yyyymmdd-hhmmss-##.png"
char * byrompath=NULL;
char * byrompathend;

//points to the minir executable, "/home/admin/minir/" "minir.exe"; fits selfname+".123"
char * selfpath=NULL;
char * selfpathend;
char * selfname;//the name minir is invoked as, without path or extension

char * state_buf=NULL;
size_t state_size=0;

int savestate_slot=0;

bool save_state(int index);
bool load_state(int index);

int rewind_timer;
bool rewind_held;

bool turbo;
bool turbo_toggle;

time_t lastscreenshot=0;
int numscreenshotsthissecond;

void update_menu();

uint64_t statusbar_expiry=0;
void set_status_bar(const char * fmt, ...);
void set_status_bar_duration(unsigned int ms);
char * statusbar_to_load=NULL;

uint64_t now;
bool new_second;

//=0: normal
//>0: enable vsync only every (N+1)th frame, multiply audio rate by (N+1)
//<0: draw blank N times per frame, divide audio rate by (N+1)
int speed_change;
int speed_change_num_blanks;

bool pause;
uint64_t pause_next_fwd;

bool handle_cli_args(const char * const * filenames, bool coresonly);

struct minirconfig * configmgr;
struct configdata config;



bool try_set_interface_video(unsigned int id, uintptr_t windowhandle,
                             unsigned int videowidth, unsigned int videoheight, videoformat videodepth, double videofps)
{
	video* device=list_video[id]->create2d(windowhandle);
	if (!device) return false;
	if (!config.driver_video || strcasecmp(config.driver_video, list_video[id]->name))
	{
		free(config.driver_video);
		config.driver_video=strdup(list_video[id]->name);
	}
	vid=device;
	if (config.video_thread)
	{
		video* outer=video_create_thread();
		outer->set_chain(vid);
		vid=outer;
	}
	vid->initialize();
	vid->set_source(videowidth*config.video_scale, videoheight*config.video_scale, videodepth);
	
	return true;
}

void create_interface_video(uintptr_t windowhandle, unsigned int videowidth, unsigned int videoheight,
                                                    videoformat videodepth, double videofps)
{
	if (config.driver_video)
	{
		for (unsigned int i=0;list_video[i]->name;i++)
		{
			if (!strcasecmp(config.driver_video, list_video[i]->name))
			{
				if (try_set_interface_video(i, windowhandle, videowidth, videoheight, videodepth, videofps)) return;
				break;
			}
		}
	}
	for (unsigned int i=0;true;i++)
	{
		if (try_set_interface_video(i, windowhandle, videowidth, videoheight, videodepth, videofps)) return;
	}
}

video* create3d(struct retro_hw_render_callback * desc)
{
	uintptr_t window=draw->get_window_handle();
	if (config.driver_video)
	{
		for (unsigned int i=0;list_video[i];i++)
		{
			if (list_video[i]->create3d && !strcasecmp(config.driver_video, list_video[i]->name))
			{
				vid3d=list_video[i]->create3d(window, desc);
				if (vid3d) return vid3d;
				break;
			}
		}
	}
	for (unsigned int i=0;list_video[i];i++)
	{
		if (list_video[i]->create3d)
		{
			vid3d=list_video[i]->create3d(window, desc);
			if (vid3d) return vid3d;
		}
	}
	return NULL;
}

bool try_create_interface_audio(const char * interface)
{
	aud=audio_create(interface, draw->get_window_handle(),
	                 (core ? (core->get_sample_rate(core)) : 8000), config.audio_latency);
	if (aud)
	{
		if (interface!=config.driver_audio)
		{
			free(config.driver_audio);
			config.driver_audio=strdup(interface);
		}
		return true;
	}
	return false;
}

bool try_set_interface_input(unsigned int id, uintptr_t windowhandle)
{
	inputkb* device=list_inputkb[id].create(windowhandle);
	if (!device) return false;
	if (!config.driver_inputkb || strcasecmp(config.driver_inputkb, list_inputkb[id].name))
	{
		free(config.driver_inputkb);
		config.driver_inputkb=strdup(list_inputkb[id].name);
	}
	inp->set_keyboard(inp, device);
	return true;
}

void create_interface_input(uintptr_t windowhandle)
{
	if (config.driver_inputkb)
	{
		for (unsigned int i=0;list_inputkb[i].name;i++)
		{
			if (!strcasecmp(config.driver_inputkb, list_inputkb[i].name))
			{
				if (try_set_interface_input(i, windowhandle)) return;
				break;
			}
		}
	}
	for (unsigned int id=0;true;id++)
	{
		if (try_set_interface_input(id, windowhandle)) return;
	}
}

void create_interfaces(unsigned int videowidth, unsigned int videoheight, videoformat videodepth, double videofps)
{
	if (vid) delete vid; vid=NULL;
	if (aud) aud->free(aud); aud=NULL;
	
	if (vid3d)
	{
		vid3d->initialize();
		vid3d->set_source(videowidth, videoheight, fmt_xrgb8888);
		vid=vid3d;
	}
	else
	{
		create_interface_video(draw->get_window_handle(), videowidth, videoheight, videodepth, videofps);
	}
	
	if (!config.driver_audio || !try_create_interface_audio(config.driver_audio))
	{
		const char * const * drivers=audio_supported_backends();
		while (true)
		{
			if (try_create_interface_audio(*drivers)) break;
			drivers++;
		}
	}
	create_interface_input(draw->get_window_handle());
}

void reset_config()
{
	//calling this before creating the window is so config.video_scale can get loaded properly, which helps initial placement
	//and also for savestate_auto, which is also used before creating the window; it could be delayed, but this method is easier.
	configmgr->data_save(configmgr, &config);
	
	if (romloaded==coreloaded) configmgr->data_load(configmgr, &config, true, NULL, romloaded);
	else configmgr->data_load(configmgr, &config, true, coreloaded, romloaded);
	if (!inp) return;
	
	unsigned int videowidth=320;
	unsigned int videoheight=240;
	videoformat videodepth=fmt_rgb565;
	double videofps=60.0;
	
	if (core) core->get_video_settings(core, &videowidth, &videoheight, &videodepth, &videofps);
	
printf("Chosen zoom: %ix%i * %i\n",videowidth,videoheight,config.video_scale);
	draw->set_hide_cursor(config.cursor_hide);
	draw->resize(videowidth*config.video_scale, videoheight*config.video_scale);
	
	create_interfaces(videowidth, videoheight, videodepth, videofps);
printf("Chosen drivers: %s, %s, %s\n", config.driver_video, config.driver_audio, config.driver_inputkb);
	
	aud->set_sync(aud, config.audio_sync);
	vid->set_dest_size(videowidth*config.video_scale, videoheight*config.video_scale);
	vid->set_vsync(config.video_sync);
	
	if (config.savestate_disable || !config.rewind_enable)
	{
		rewind->reset(rewind, 1, 0);
		rewind_timer=0;
	}
	else
	{
		if (core) rewind->reset(rewind, core->state_size(core)+sizeof(rewind_timer), config.rewind_mem*1024*1024);
		rewind_timer=config.rewind_granularity;
		rewind_held=false;
	}
	
	if (core) core->attach_interfaces(core, vid, aud, retroinp);
	
	retroinp->set_input(retroinp, inp);
	retroinp->joypad_set_inputs(retroinp, 0, input_joy+16*0, 16*4);
	retroinp->joypad_set_inputs(retroinp, 1, input_joy+16*4, 16*4);
	retroinp->joypad_set_block_opposing(retroinp, config.joypad_block_opposing);
	
	turbo=false;
	turbo_toggle=false;
	
	speed_change=0;
	
	pause=false;
	
	for (int i=0;i<input_count;i++)
	{
		if (!inp->map_key(inp, config.inputs[i], i))
		{
			free(config.inputs[i]);
			config.inputs[i]=NULL;
		}
	}
	
	if (core)
	{
		//do not attempt to optimize this, the funky math is needed to ensure we don't overflow the multiplication
		//the compiler optimizes it well enough anyways
		if (((size_t)-1)/1024/1024 > 65536 && config.cheat_mem > ((size_t)-1)/1024/1024)
		{
			cheats->set_core(cheats, core, (size_t)-1);
		}
		else
		{
			cheats->set_core(cheats, core, config.cheat_mem*1024*1024);
		}
	}
	
	update_menu();
}



void create_byrom_path()
{
	if (!byrompath)
	{
		size_t rompathlen=strlen(romloaded);
		byrompath=malloc(rompathlen+1+8+1+6+1+2+4+1);//rom-yyyymmdd-hhmmss-##.png
		memcpy(byrompath, romloaded, rompathlen+1);
		
		char * slash=strrchr(byrompath, '/');
		if (!slash) slash=byrompath;
		
		char * dot=strrchr(slash, '.');
		if (!dot) dot=strrchr(slash, '\0');
		
		byrompathend=dot;
	}
}

const char * sram_path()
{
	create_byrom_path();
	
	byrompathend[0]='.';
	byrompathend[1]='s';
	byrompathend[2]='r';
	byrompathend[3]='m';
	byrompathend[4]='\0';
	return byrompath;
}

void unload_rom()
{
	if (romloaded && config.savestate_auto)
	{
		save_state(-1);
	}
	if (romloaded && config.sram_write_unload)
	{
		size_t sramsize;
		void* sramptr;
		core->get_memory(core, libretromem_sram, &sramsize, &sramptr);
		if (sramsize) file_write(sram_path(), sramptr, sramsize);
	}
	if (romloaded==coreloaded)
	{
		if (core) core->free(core);
		core=NULL;
		coreloaded=NULL;
	}
	free(romloaded);
	romloaded=NULL;
	free(romname);
	romname=NULL;
	free(byrompath);
	byrompath=NULL;
	if (wndw) wndw->set_title(wndw, "minir");
}

bool study_core(const char * path, struct libretro * core)
{
//printf("study=%s\n",path); fflush(stdout);
	bool freecore=(!core);
	struct libretro * thiscore = core ? core : libretro_create(path, NULL, NULL);
	if (!thiscore) return false;
	
	struct configdata coreconfig;
	configmgr->data_load(configmgr, &coreconfig, false, path, NULL);
	
	//ugly tricks ahead...
	for (unsigned int i=0;coreconfig.support[i];i++) free(coreconfig.support[i]);
// printf("free%i=%p [%p]\n",i,coreconfig.support[i],coreconfig.support),
//fflush(stdout);
	free(coreconfig.support);
	coreconfig.support=(char**)thiscore->supported_extensions(thiscore, NULL);
//printf("ext=%s\n",coreconfig.support[0]);
//printf("ext=%s\n",coreconfig.support[1]);
//printf("ext=%s\n",coreconfig.support[2]);
	
	free(coreconfig.corename); coreconfig.corename=(char*)thiscore->name(thiscore);
	
	configmgr->data_save(configmgr, &coreconfig);
	coreconfig.support=NULL;
	coreconfig.corename=NULL;
	configmgr->data_free(configmgr, &coreconfig);
	
	if (freecore) thiscore->free(thiscore);
	return true;
}

void message_cb(int severity, const char * message)
{
const
char
*
const
severities
[]
={
"debug",
"info",
"warning",
"error"
};
printf(
"[%s: %s]\n",
severities
[severity],
message
);
//TODO
}

bool load_core(const char * path, bool keep_rom)
{
	char * kept_rom=NULL;
	if (romloaded==coreloaded) keep_rom=false;
	if (keep_rom) kept_rom=strdup(romloaded);
	
	unload_rom();
	free(coreloaded);
	coreloaded=NULL;
	if (core) core->free(core);
	
	core=libretro_create(path, message_cb, NULL);
	if (!core)
	{
		if (wndw) wndw->set_title(wndw, "minir");
		free(kept_rom);
		return false;
	}
	coreloaded=strdup(path);
	
	study_core(path, core);
	core->attach_interfaces(core, vid, aud, retroinp);
	
	if (kept_rom)
	{
		load_rom(kept_rom);
		free(kept_rom);
	}
	return true;
}

bool select_cores(const char * wanted_extension)
{
	const char * extension[2]={ dylib_ext(), NULL };
	const char * const * cores=window_file_picker(wndw, "Select libretro cores", extension, "libretro cores", true, true);
	if (cores) handle_cli_args(cores, true);
	return (bool)cores;
}

void set_window_title()
{
	if (!wndw || !romloaded) return;
	char* title;
	if (romloaded==coreloaded) asprintf(&title, "%s", config.gamename);
	else asprintf(&title, "%s - %s", config.gamename, config.corename);
	wndw->set_title(wndw, title);
	free(title);
}

bool load_rom(const char * rom)
{
	const char * extension=strrchr(rom, '.');
	if (extension && !strcasecmp(extension, dylib_ext()))
	{
		if (load_core_as_rom(rom)) return true;
		//I doubt there is any core that can load a dll, but why not try? Worst case, it just fails, as it would have anyways.
	}
	if (!core ||
			(extension && !core->supports_extension(core, extension+1)))
	{
		struct configcorelist * newcores;
		newcores=configmgr->get_core_for(configmgr, rom, NULL);
		if (config.auto_locate_cores)
		{
			if (!newcores[0].path)
			{
				free(newcores);
				const char * const * cores=libretro_nearby_cores(rom);
				for (int i=0;cores[i];i++) study_core(cores[i], NULL);
				newcores=configmgr->get_core_for(configmgr, rom, NULL);
			}
			if (!newcores[0].path)
			{
				free(newcores);
				const char * const * cores=libretro_default_cores();
				for (int i=0;cores[i];i++) study_core(cores[i], NULL);
				newcores=configmgr->get_core_for(configmgr, rom, NULL);
			}
		}
		if (!newcores[0].path)
		{
			//MBOX:No libretro core found for this file type. Do you wish to look for one?
			free(newcores);
			if (!select_cores(rom)) return false;
			newcores=configmgr->get_core_for(configmgr, rom, NULL);
		}
		if (!newcores[0].path) return false;
		unload_rom();
		if (!load_core(newcores[0].path, false))
		{
			configmgr->data_destroy(configmgr, newcores[0].path);
			if (wndw) wndw->set_title(wndw, "minir");
			//MBOX: "Couldn't load core at %s", newcores[0].path
			return false;
		}
		free(newcores);
	}
	unload_rom();
	delete vid; vid=NULL;
	vid3d=NULL;
	core->enable_3d(core, bind(create3d));
	if (!core->load_rom(core, NULL, 0, rom))
	{
		if (wndw) wndw->set_title(wndw, "minir");
		//MBOX: "Couldn't load %s with %s", romloaded, core->name(core)
		core->free(core);
		core=NULL;
		free(coreloaded);
		coreloaded=NULL;
		return false;
	}
	
	romloaded=strdup(rom);
	reset_config();
	
	if (!config.gamename)
	{
		char * basenamestart=strrchr(romloaded, '/');
		if (basenamestart) basenamestart++;
		else basenamestart=romloaded;
		char * basenameend=strrchr(basenamestart, '.');
		if (basenameend) *basenameend='\0';
		config.gamename=strdup(basenamestart);
		if (basenameend) *basenameend='.';
	}
	
	load_rom_finish();
	
	return true;
}

bool load_core_as_rom(const char * rom)
{
	if (!load_core(rom, false) || !core->load_rom(core, NULL, 0, NULL))
	{
		wndw->set_title(wndw, "minir");
		return false;
	}
	romloaded=coreloaded;
	
	reset_config();
	load_rom_finish();
	
	free(config.corename);
	config.corename=strdup(core->name(core));
	
	return true;
}

void load_rom_finish()
{
	free(state_buf);
	state_size=core->state_size(core);
	state_buf=malloc(state_size);
	
	size_t sramsize;
	void* sramptr;
	core->get_memory(core, libretromem_sram, &sramsize, &sramptr);
	if (sramsize) file_read_to(sram_path(), sramptr, sramsize);
	
	if (config.savestate_auto)
	{
		load_state(-1);
	}
	
	set_window_title();
	if (romloaded==coreloaded) set_status_bar("Loaded %s", config.gamename);
	else set_status_bar("Loaded %s with %s", config.gamename, config.corename);
printf("Chosen core: %s\n", coreloaded);
printf("Chosen ROM: %s\n", romloaded);
}

void select_rom()
{
	const char * * extensions=configmgr->get_supported_extensions(configmgr);
	if (!*extensions && config.auto_locate_cores)
	{
		free(extensions);
		const char * const * cores=libretro_default_cores();
		for (int i=0;cores[i];i++) study_core(cores[i], NULL);
		extensions=configmgr->get_supported_extensions(configmgr);
	}
	const char * const * roms=window_file_picker(wndw, "Select ROM to open", extensions, "All supported ROMs", false, false);
	free(extensions);
	if (roms) load_rom(*roms);
}

void create_self_path(const char * argv0)
{
	const char * selfnamestart=strrchr(argv0, '/');
	if (selfnamestart) selfnamestart++;
	else selfnamestart=argv0;
	
	const char * selfnameend=strchr(selfnamestart, '.');
	if (!selfnameend) selfnameend=strchr(selfnamestart, '\0');
	
	selfname=malloc(selfnameend-selfnamestart+1);
	memcpy(selfname, selfnamestart, selfnameend-selfnamestart);
	selfname[selfnameend-selfnamestart]='\0';
	
	const char * selfpathraw=window_get_proc_path();
	int selfpathrawlen=strlen(selfpathraw);
	selfpath=malloc(selfpathrawlen+1+(selfnameend-selfnamestart)+4+1);
	memcpy(selfpath, selfpathraw, selfpathrawlen);
	selfpath[selfpathrawlen]='/';
	selfpathend=selfpath+selfpathrawlen+1;
	selfpath[selfpathrawlen+1]='\0';
}

bool handle_cli_args(const char * const * filenames, bool coresonly)
{
	if (!*filenames) return false;
	
	bool badload=false;
	//bool coreok;
	
	char* badcore=NULL;
	
	//char* newcore=NULL;
	int numnewcores=0;
	
	//char* newexts=NULL;
	//char* newextalts=NULL;
	
	char * load=NULL;
	bool load_is_core=false;
	const char * ext=dylib_ext();
	for (int i=0;filenames[i];i++)
	{
		char * path=window_get_absolute_path_cwd(filenames[i], true);
		const char * end=strrchr(path, '.');
		if (coresonly || (end && !strcmp(end, ext)))
		{
			bool existed;
			struct libretro * thiscore=libretro_create(path, NULL, &existed);
			if (thiscore)
			{
				if (thiscore->supports_no_game(thiscore))
				{
					load_is_core=true;
					if (!load) load=strdup(path);
					else badload=true;
				}
				else
				{
					study_core(path, thiscore);
				}
				thiscore->free(thiscore);
				numnewcores++;
			}
			else if (!existed)//if someone tells us to open the current core, we want to ignore it
			{
				if (badcore)
				{
					int len1=strlen(badcore);
					int len2=strlen(path);
					badcore=realloc(badcore, len1+2+len2+1);
					badcore[len1+0]=',';
					badcore[len1+1]=' ';
					memcpy(badcore+len1+2, path, len2+1);//includes the NUL
				}
				else badcore=strdup(path);
			}
		}
		else
		{
			if (!load) load=strdup(path);
			else badload=true;
		}
		free(path);
	}
//STBAR: Added &5 cores; can now handle '&smc', '&smc' and '&smc' games
//alt Added &5 cores; more alternatives available for '&smc' games
//alt Added core '&bsnes-accuracy-092'; can now handle '&smc', '&smc' and '&smc' games
//alt Added core '&bsnes-accuracy-092'; more alternatives available for '&smc' games
	
//alt if badload STBAR:Multiple ROMs chosen. Pick one.
	if (badcore)
	{
		set_status_bar("Couldn't load %s", badcore);
	}
	else if (load && badload)
	{
		set_status_bar("Multiple ROMs chosen. Pick one at the time.");
	}
	else if (numnewcores>1)
	{
		set_status_bar("Added %i cores; can now handle %s games", numnewcores, "x");
	}
	else
	{
		//FIXME
	}
	
	free(badcore);
	if (load && !badload)
	{
		if (load_is_core) load_core_as_rom(load);
		else load_rom(load);
		free(load);
		return true;
	}
	free(load);
	
	return false;
}

void drop_handler(const char * const * filenames)
{
	if (handle_cli_args(filenames, false)) wndw->focus(wndw);
}

bool closethis(struct window * subject, void* userdata)
{
	exit_called=true;
	return true;
}



void initialize(int argc, char * argv[])
{
	memset(&config, 0, sizeof(config));
	
	create_self_path(argv[0]);
	
	strcpy(selfpathend, selfname);
	strcat(selfpathend, ".cfg");
	configmgr=config_create(selfpath);
	configmgr->data_load(configmgr, &config, false, NULL, NULL);
	
	draw=widget_create_viewport(640, 480);
	wndw=window_create(draw);
	
	const int align[]={0,2};
	const int divider=180;
	wndw->statusbar_create(wndw, 2, align, &divider);
	
	if (argc==1)
	{
		const char * defautoload=configmgr->get_autoload(configmgr);
		if (defautoload) load_rom(defautoload);
	}
	else handle_cli_args((const char * const *)argv+1, false);
	
	//reset_config();
	
	unsigned int videowidth=320;
	unsigned int videoheight=240;
	videoformat videodepth=fmt_rgb565;
	double videofps=60.0;
	
	if (core) core->get_video_settings(core, &videowidth, &videoheight, &videodepth, &videofps);
	
	//draw->resize(videowidth*config.video_scale, videoheight*config.video_scale);
	wndw->set_onclose(wndw, closethis, NULL);
	wndw->set_title(wndw, "minir");
	set_window_title();
	
	draw->set_support_drop(bind(drop_handler));
	
	if (statusbar_to_load)
	{
		wndw->statusbar_set(wndw, 0, statusbar_to_load);
		free(statusbar_to_load);
	}
	
	retroinp=libretroinput_create(NULL);
	inp=inputmapper_create();
	rewind=rewindstack_create(0, 0);
	
	cheats=minircheats_create();
	cheats->set_parent(cheats, wndw);
	
	update_menu();
	reset_config();
	wndw->set_visible(wndw, true);
}



const char * get_state_path(int index)
{
	create_byrom_path();
	
	byrompathend[0]='.';
	byrompathend[1]=tolower(config.corename[0]);
	if (index<=9) byrompathend[2]='s';
	else          byrompathend[2]='0'+(index/10);
	if (index<=0) byrompathend[3]="at"[index+1];
	else          byrompathend[3]='0'+(index%10);
	byrompathend[4]='\0';
	return byrompath;
}

bool save_state(int index)
{
	const char * path=get_state_path(index);
	
	bool ret=true;
	if (ret) ret=core->state_save(core, state_buf, state_size);
	if (ret) ret=file_write(path, state_buf, state_size);
	
	if (ret) set_status_bar("State %i saved", index+1);
	else set_status_bar("Couldn't save state %i", index+1);
	
	return ret;
}

bool load_state(int index)
{
	const char * path=get_state_path(index);
	
	bool ret=true;
	if (ret) ret=file_read_to(path, state_buf, state_size);
	if (ret) ret=core->state_load(core, state_buf, state_size);
	
	if (wndw)
	{
		if (ret) set_status_bar("State %i loaded", index+1);
		else set_status_bar("Couldn't load state %i", index+1);
	}
	
	return ret;
}



const char * get_screenshot_path()
{
	create_byrom_path();
	
	time_t now=time(NULL);
	if (lastscreenshot==now) numscreenshotsthissecond++;
	else numscreenshotsthissecond=0;
	lastscreenshot=now;
	
	if (numscreenshotsthissecond>99) return NULL;
	
	struct tm * tm=localtime(&now);
	strftime(byrompathend, 1+8+1+6+1+2+4+1, "-%Y%m%d-%H%M%S", tm);
	if (numscreenshotsthissecond) sprintf(byrompathend+1+8+1+6, "-%i.png", numscreenshotsthissecond);
	else strcpy(byrompathend+1+8+1+6, ".png");
	return byrompath;
}

bool create_screenshot()
{
	void* data=NULL;
	int scret=0;
{
	struct image img;
	scret=vid->get_screenshot(&img.width, &img.height, &img.pitch, &img.bpp, &img.pixels, 0);
	data=img.pixels;
	if (!scret) goto bad;
	
	void* pngdata;
	unsigned int pnglen;
	
	const char * comments[]={
		"Software", NULL,
		NULL
	};
	asprintf((char**)&comments[1],
	         "minir v"VERSION"\ncore: %s (%s)\ngame: %s (%s)",
	         config.corename, coreloaded, config.gamename, romloaded);
	
	bool ok;
	ok=png_encode(&img, comments, &pngdata, &pnglen);
	free((char*)comments[1]);
	if (!ok) goto bad_clean;
	
	vid->release_screenshot(scret, data);
	
	ok=file_write(get_screenshot_path(), pngdata, pnglen);
	free(pngdata);
	if (!ok) goto bad;
	
	set_status_bar("Screenshot saved");
	return true;
}
bad_clean:
	vid->release_screenshot(scret, data);
bad:
	set_status_bar("Couldn't save screenshot");
	return false;
}



void create_state_manager()
{
/*
	struct dialogtemplate dialog_preview[]={
		{ dlg_spacing },
		{ dlg_bitmap, .id=1, .width=80, .height=72 },
		{ dlg_spacing },
		{ dlg_end }
	};
	struct dialogtemplate dialog_date[]={
		{ dlg_spacing },
		{ dlg_label, "date", .id=2 },
		{ dlg_spacing },
		{ dlg_end }
	};
	struct dialogtemplate dialog_buttons[]={
		{ dlg_spacing },
		{ dlg_button, "Load" },
		{ dlg_button, "Save" },
		{ dlg_button, "Cancel" },
		{ dlg_end }
	};
	struct dialogtemplate dialog[]={
		{ dlg_layout_horz, .contents=dialog_preview },
		{ dlg_spacing, .height=5 },
		{ dlg_layout_horz, .contents=dialog_date },
		{ dlg_spacing, .height=12 },
		{ dlg_bitmap, .id=3, .width=160, .height=144, .action=test },
		{ dlg_spacing, .height=12 },
		{ dlg_layout_horz, .contents=dialog_buttons },
		{ dlg_end }
	};
	struct dialogtemplate dialog_shell[]={
		{ dlg_layout_vert, .contents=dialog },
		{ dlg_end }
	};
	statemgr=window_create_dialog(dialog_shell);
	statemgr_shell=statemgr->get_parent(statemgr);
	statemgr_shell->show(statemgr_shell);
	statemgr_shell->set_modal(statemgr_shell, true);
*/
}



void update_rewind_stats()
{
	unsigned int entries;
	size_t bytes;
	bool full;
	rewind->capacity(rewind, &entries, &bytes, &full);
	if (new_second)
	{
//printf("rw: rawsize=%zu byte-per-slot=%f cmpr-ratio=%f\n",state_size,(float)bytes/entries,(float)bytes/entries/state_size);
//printf("rw: num-slot-used=%u num-byte-used=%zu frac-used=%f full=%i\n",entries,bytes,(float)bytes/config.rewind_mem/1024/1024,full);
		int now=entries*config.rewind_granularity/60;
		
		const char * nowstr="sec";
		if (now>=120)
		{
			now/=60;
			nowstr="min";
		}
		if (now>=120)
		{
			now/=60;
			nowstr="hr";
		}
		
		if (full) set_status_bar("Rewind: %u%s used (full)", now, nowstr);
		else if (entries<600) set_status_bar("Rewind: %u%s used", now, nowstr);
		else
		{
			//we need floating point here, because otherwise we could cross 64 bits in the multiplication
			//we don't need the full precision, anyways; 2-3 digits is enough, and float gives a little above 7
			unsigned int max=(float)config.rewind_mem*1024*1024*entries*config.rewind_granularity/bytes/60;
			
			const char * maxstr="sec";
			if (max>=120)
			{
				max/=60;
				maxstr="min";
			}
			if (max>=120)
			{
				max/=60;
				maxstr="hr";
			}
			
			set_status_bar("Rewind: %u%s used, ~%u%s max", now, nowstr, max, maxstr);
		}
	}
}

void handle_rewind(bool * skip_frame, bool * count_skipped_frame)
{
	if (!config.rewind_enable) return;
	if (*skip_frame) return;
	bool rewind_held_new=inp->button(inp, input_rewind, false);
	if (rewind_held_new && !rewind_held) rewind_timer=1;
	rewind_held=rewind_held_new;
	int rewind_decrement_steps=(rewind_held ? config.rewind_speedup : 1);
	
	while (rewind_decrement_steps--)
	{
		if (!--rewind_timer)
		{
			if (!rewind_held)
			{
				if (turbo) rewind_timer=config.rewind_granularity_turbo;
				else rewind_timer=config.rewind_granularity;
				
				char* state=(char*)rewind->push_begin(rewind);
				memcpy(state+0, &rewind_timer, sizeof(rewind_timer));
				if (core->state_save(core, state+sizeof(rewind_timer), state_size))
				{
					rewind->push_end(rewind);
				}
				else rewind->push_cancel(rewind);
			}
			else
			{
				const char* state=(const char*)rewind->pull(rewind);
				if (state)
				{
					memcpy(&rewind_timer, state+0, sizeof(rewind_timer));
					core->state_load(core, state+sizeof(rewind_timer), state_size);
				}
				else
				{
					rewind_timer=1;
					aud->clear(aud);
					*skip_frame=true;
					set_status_bar("Cannot rewind further");
					set_status_bar_duration(20);
				}
			}
			if (config.rewind_stats) update_rewind_stats();
		}
		else if (rewind_held)
		{
			aud->clear(aud);
			*skip_frame=true;
			*count_skipped_frame=true;
		}
	}
}

void do_hotkeys(bool * skip_frame, bool * count_skipped_frame)
{
	if (!romloaded) return;
	
	if (!config.savestate_disable)
	{
		for (int i=0;i<10;i++)
		{
			if (inp->button(inp, input_savestate_save+i, true)) save_state(i);
			if (inp->button(inp, input_savestate_load+i, true)) load_state(i);
		}
		
		if (inp->button(inp, input_savestate_slot_save, true)) save_state(savestate_slot);
		if (inp->button(inp, input_savestate_slot_load, true)) load_state(savestate_slot);
		if (inp->button(inp, input_savestate_slot_next, true))
		{
			savestate_slot=(savestate_slot+1)%100;
			set_status_bar("Savestate slot %i now active", savestate_slot+1);
		}
		if (inp->button(inp, input_savestate_slot_prev, true))
		{
			savestate_slot=(savestate_slot+100-1)%100;
			set_status_bar("Savestate slot %i now active", savestate_slot+1);
		}
		if (inp->button(inp, input_savestate_slot_next_10, true))
		{
			savestate_slot=(savestate_slot+10)%100;
			set_status_bar("Savestate slot %i now active", savestate_slot+1);
		}
		if (inp->button(inp, input_savestate_slot_prev_10, true))
		{
			savestate_slot=(savestate_slot+100-10)%100;
			set_status_bar("Savestate slot %i now active", savestate_slot+1);
		}
	}
	else
	{
		bool tried=false;
		for (int i=0;i<10;i++)
		{
			if (inp->button(inp, input_savestate_save+i, true)) tried=true;
			if (inp->button(inp, input_savestate_load+i, true)) tried=true;
		}
		
		if (inp->button(inp, input_savestate_slot_save, true)) tried=true;
		if (inp->button(inp, input_savestate_slot_load, true)) tried=true;
		if (inp->button(inp, input_savestate_slot_next, true)) tried=true;
		if (inp->button(inp, input_savestate_slot_prev, true)) tried=true;
		if (inp->button(inp, input_savestate_slot_next_10, true)) tried=true;
		if (inp->button(inp, input_savestate_slot_prev_10, true)) tried=true;
		
		if (tried) set_status_bar("Savestates are disabled");
	}
	
	
	if (inp->button(inp, input_screenshot, true)) create_screenshot();
	
	
	if (inp->button(inp, input_pause, true))
	{
		pause=!pause;
		pause_next_fwd=now+config.input_frame_adv_hold_delay;
	}
	if (pause)
	{
		*skip_frame=true;
		if (inp->button(inp, input_frame_adv_hold, false))
		{
			if (now>pause_next_fwd)
			{
				*skip_frame=false;
				pause_next_fwd+=config.input_frame_adv_hold_delay;
				if (pause_next_fwd<now) pause_next_fwd=now+config.input_frame_adv_hold_delay;
			}
		}
		else pause_next_fwd=now+config.input_frame_adv_hold_delay;
		if (inp->button(inp, input_frame_adv, true)) *skip_frame=false;
	}
	
	
	bool lastturbo=turbo;
	turbo_toggle^=(inp->button(inp, input_turbo_toggle, true));
	turbo=(inp->button(inp, input_turbo, false));
	if (turbo) turbo_toggle=false;
	else turbo=turbo_toggle;
	
	if (turbo!=lastturbo)
	{
		aud->set_sync(aud, config.audio_sync && !turbo);
		vid->set_vsync(config.video_sync && !turbo);
		
		if (turbo) rewind_timer=config.rewind_granularity_turbo;
		else rewind_timer=config.rewind_granularity;
	}
	
	
	if (rewind_timer) handle_rewind(skip_frame, count_skipped_frame);
	else if (inp->button(inp, input_rewind, true)) set_status_bar("Rewind is disabled");
	
	
	bool speed_changed=false;
	if (inp->button(inp, input_slowdown, true))
	{
		speed_change--;
		speed_changed=true;
	}
	if (inp->button(inp, input_speedup, true))
	{
		speed_change++;
		speed_changed=(!speed_changed);
	}
	if (speed_changed)
	{
		double rate=core->get_sample_rate(core);
		if (speed_change<0) rate/=-(speed_change-1);
		if (speed_change>0) rate*=(speed_change+1);
		aud->set_samplerate(aud, rate);
	}
	if (speed_change<0 && !*skip_frame)
	{
		speed_change_num_blanks=(speed_change_num_blanks+1)%((-speed_change)+1);
		*skip_frame=(speed_change_num_blanks!=0);
	}
	if (speed_change>0 && !*skip_frame)
	{
		speed_change_num_blanks=(speed_change_num_blanks+1)%(speed_change+1);
		if (!turbo) vid->set_vsync((speed_change_num_blanks==0) && config.video_sync);
	}
	
	if (inp->button(inp, input_savestate_manager, true)) create_state_manager();
}



void mainloop()
{
//unsigned char* gameram;
//core->get_memory(core,libretromem_wram,NULL,(void**)&gameram);
uint64_t lastchtupd=0;
	while (!exit_called)
	{
//printf("94:%.2X 7B:%.2X 7A:%.2X\n",gameram[0x94],gameram[0x7B],gameram[0x7A]);
		uint64_t next=window_get_time();
		new_second=(next/1000000 != now/1000000);
		now=next;
		
static int i=0;i+=1;
if (new_second)
{
char gg[16];
sprintf(gg,"%i fps",i);
wndw->statusbar_set(wndw, 1, gg);
i=0;
}

//static int gg=0;gg++;if(gg==60000)exit_called=1;config.defocus_pause=0;printf("%i/60000\r",gg);fflush(stdout);

		
		if (statusbar_expiry && now>statusbar_expiry)
		{
			wndw->statusbar_set(wndw, 0, "");
			statusbar_expiry=0;
		}
		
		if (wndw->is_active(wndw)) inp->poll(inp);
		//else inp->clear(inp);

char*b=inp->last(inp);
if(b)printf("%s\n",b),free(b);
		
		bool skip_frame=false;
		bool count_skipped_frame=false;
		do_hotkeys(&skip_frame, &count_skipped_frame);
if(skip_frame&&!count_skipped_frame)i--;
		if (romloaded)
		{
			if (!skip_frame)
			{
				core->run(core);
				if (now - lastchtupd >= 100*1000)
				{
					cheats->update(cheats, true);
					lastchtupd=now;
				}
				else cheats->update(cheats, false);
			}
			else
			{
				vid->draw_repeat();
			}
		}
		
		if (!romloaded ||
		    (config.defocus_pause && !wndw->is_active(wndw)) ||
		    (!config.defocus_pause && wndw->menu_active(wndw)))
		{
			aud->clear(aud);
			//inp->clear(inp);
			cheats->update(cheats, true);
			wndw->statusbar_set(wndw, 1, "Paused");
			draw->set_hide_cursor(false);
			while (!exit_called && wndw->is_visible(wndw) &&
			      (!romloaded || !wndw->is_active(wndw)))
			{
				window_run_wait();
			}
			if (exit_called || !wndw->is_visible(wndw)) break;
			draw->set_hide_cursor(config.cursor_hide);
			
			//get rid of last(), if someone is holding something on focus (they shouldn't, but let's let them anyways.)
			inp->poll(inp);
			inp->poll(inp);
			free(inp->last(inp));
		}
		
		window_run_iter();
	}
}



void deinit()
{
	unload_rom();
	
	if (!config.readonly)
	{
		configmgr->data_save(configmgr, &config);
		strcpy(selfpathend, selfname);
		strcat(selfpathend, ".cfg");
		configmgr->write(configmgr, selfpath);
	}
	configmgr->free(configmgr);
	
	free(state_buf);
	
	delete vid; vid=NULL;
	aud->free(aud); aud=NULL;
	inp->free(inp); inp=NULL;
	retroinp->free(retroinp); retroinp=NULL;
	wndw->free(wndw); wndw=NULL;
	draw=NULL;//window contents are freed when the window is
	if (core) core->free(core); core=NULL;
	if (cheats) cheats->free(cheats); cheats=NULL;
}



#ifndef HAVE_ASPRINTF
void asprintf(char * * ptr, const char * fmt, ...)
{
	va_list args;
	
	char * data=malloc(64);
	
	va_start(args, fmt);
	int neededlen=vsnprintf(data, 64, fmt, args);
	va_end(args);
	
	if (neededlen>=64)
	{
		free(data);
		data=malloc(neededlen+1);
		va_start(args, fmt);
		vsnprintf(data, neededlen+1, fmt, args);
		va_end(args);
	}
	
	*ptr=data;
}
#endif

void set_status_bar(const char * fmt, ...)
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
	
	if (wndw)
	{
		wndw->statusbar_set(wndw, 0, msgdat);
		free(msgdat);
	}
	else
	{
		free(statusbar_to_load);
		statusbar_to_load=msgdat;
	}
	
	set_status_bar_duration(3000);
}

void set_status_bar_duration(unsigned int ms)
{
	statusbar_expiry=window_get_time()+ms*1000;
}



void configpanel_show()
{
	
}



bool is_yesno(const char * yes, const char * no, bool * yesfirst)
{
	if (yesfirst) *yesfirst=true;
	if (!strcasecmp(yes, "yes") && !strcasecmp(no, "no")) return true;
	if (!strcasecmp(yes, "enabled") && !strcasecmp(no, "disabled")) return true;
	if (!strcasecmp(yes, "enable") && !strcasecmp(no, "disable")) return true;
	if (!strcasecmp(yes, "on") && !strcasecmp(no, "off")) return true;
	if (yesfirst)
	{
		*yesfirst=false;
		return is_yesno(no, yes, NULL);
	}
	return false;
}

void set_core_opt_normal(struct windowmenu * subject, unsigned int state, void* userdata)
{
	core->set_core_option(core, (uintptr_t)userdata, state);
}

void set_core_opt_bool(struct windowmenu * subject, bool checked, void* userdata)
{
	core->set_core_option(core, (uintptr_t)userdata, checked);
}

void set_core_opt_bool_invert(struct windowmenu * subject, bool checked, void* userdata)
{
	set_core_opt_bool(subject, !checked, userdata);
}

void update_coreopt_menu(struct windowmenu * parent, unsigned int pos)
{
	static struct windowmenu * menu=NULL;
	if (menu && core && !core->get_core_options_changed(core)) return;
	if (menu) parent->remove_child(parent, menu);
	menu=windowmenu_create_submenu("_Core _Options", NULL);
	
	unsigned int numopts;
	const struct libretro_core_option * opts=NULL;
	if (core) opts=core->get_core_options(core, &numopts);
	if (!opts)
	{
		struct windowmenu * item=windowmenu_create_item("(no core options)", NULL, NULL);
		menu->insert_child(menu, 0, item);
		item->set_enabled(item, false);
		menu->set_enabled(menu, false);
		parent->insert_child(parent, pos, menu);
		return;
	}
	
	for (unsigned int i=0;i<numopts;i++)
	{
		unsigned int numvalues=opts[i].numvalues;
		
		bool yesfirst;
		if (numvalues==2 && is_yesno(opts[i].values[0], opts[i].values[1], &yesfirst))
		{
			struct windowmenu * item;
			item=windowmenu_create_check(opts[i].name_display, yesfirst ? set_core_opt_bool_invert : set_core_opt_bool, (void*)(uintptr_t)i);
			menu->insert_child(menu, i, item);
			item->set_state(item, (bool)core->get_core_option(core, i) ^ yesfirst);
		}
		else
		{
			struct windowmenu * radioitem=windowmenu_create_radio_l(numvalues, opts[i].values, set_core_opt_normal, (void*)(uintptr_t)i);
			struct windowmenu * menuitem=windowmenu_create_submenu(opts[i].name_display, radioitem, NULL);
			menu->insert_child(menu, i, menuitem);
			radioitem->set_state(radioitem, core->get_core_option(core, i));
		}
	}
	parent->insert_child(parent, pos, menu);
}

void menu_system_core_any(struct windowmenu * subject, unsigned int state, void* userdata)
{
	struct configcorelist * cores_for_this=(struct configcorelist*)userdata;
	load_core(cores_for_this[state].path, true);
}

void menu_system_core_more(struct windowmenu * subject, void* userdata)
{
	select_cores(NULL);
}

struct windowmenu * update_corepicker_menu(struct windowmenu * parent)
{
	struct windowmenu * menu=windowmenu_create_submenu("__Core", NULL);
	
	unsigned int numchildren=0;
	
	struct configcorelist * cores_for_this=NULL;
	if (romloaded)
	{
		unsigned int current_core_id=0;
		
		int fixstate=0;
		
	again:
		free(cores_for_this);
		unsigned int numcores;
		cores_for_this=configmgr->get_core_for(configmgr, romloaded, &numcores);
		
		const char * * names=malloc(sizeof(const char*)*numcores);
		for (unsigned int i=0;i<numcores;i++)
		{
			if (!cores_for_this[i].name)
			{
				if (!study_core(cores_for_this[i].path, NULL)) configmgr->data_destroy(configmgr, cores_for_this[i].path);
				if (fixstate==0) fixstate=1;
			}
			
			names[i]=cores_for_this[i].name;
			
			if (!strcmp(coreloaded, cores_for_this[i].path)) current_core_id=i;
		}
		
		if (fixstate==1)
		{
			fixstate=2;
			goto again;
		}
		
		struct windowmenu * items;
		if (romloaded!=coreloaded)
		{
			items=windowmenu_create_radio_l(numcores, names, menu_system_core_any, cores_for_this);
		}
		else
		{
			//for gameless cores, claim the core itself is the only one who can do this
			//also ignore change requests because there is nothing changable in a single-item radio item.
			const char * name=core->name(core);
			items=windowmenu_create_radio_l(1, &name, NULL, NULL);
		}
		menu->insert_child(menu, numchildren++, items);
		items->set_state(items, current_core_id);
		free(names);
	}
	else
	{
		struct windowmenu * norom=windowmenu_create_item("(no ROM loaded)", NULL, NULL);
		menu->insert_child(menu, numchildren++, norom);
		norom->set_enabled(norom, false);
	}
	menu->insert_child(menu, numchildren++, windowmenu_create_separator());
	menu->insert_child(menu, numchildren++, windowmenu_create_item("_Add _more cores", menu_system_core_more, NULL));
	return menu;
}

void menu_system_rom(struct windowmenu * subject, void* userdata)
{
	select_rom();
}

void menu_system_reset(struct windowmenu * subject, void* userdata)
{
	core->reset(core);
}

void menu_system_settings(struct windowmenu * subject, void* userdata)
{
	configpanel_show();
}

void menu_system_exit(struct windowmenu * subject, void* userdata)
{
	exit_called=true;
}

void menu_cheat_list(struct windowmenu * subject, void* userdata)
{
	cheats->show_list(cheats);
}

void menu_cheat_search(struct windowmenu * subject, void* userdata)
{
	cheats->show_search(cheats);
}

void menu_cheat_enable_f(struct windowmenu * subject, bool state, void* userdata)
{
	cheats->set_enabled(cheats, state);
}

struct windowmenu * * romonlyitems=NULL;
unsigned int numromonlyitems=0;
struct windowmenu * menu_with_rom_only(struct windowmenu * item)
{
	romonlyitems=realloc(romonlyitems, sizeof(struct windowmenu*)*(numromonlyitems+1));
	romonlyitems[numromonlyitems++]=item;
	return item;
}

void update_menu()
{
	static struct windowmenu * menu_system;
	static struct windowmenu * menu_system_core=NULL;
	static struct windowmenu * menu_cheat_enable=NULL;
	
	if (menu_system_core) menu_system->remove_child(menu_system, menu_system_core);
	menu_system_core=update_corepicker_menu(menu_system);
	
	if (!menu)
	{
		menu=windowmenu_create_topmenu(
			menu_system=windowmenu_create_submenu("__System", 
				windowmenu_create_item("__Load ROM", menu_system_rom, NULL),
				menu_system_core,
				windowmenu_create_separator(),
				menu_with_rom_only(windowmenu_create_item("__Reset", menu_system_reset, NULL)),
				windowmenu_create_separator(),
				windowmenu_create_item("__Settings", menu_system_settings, NULL),
				windowmenu_create_separator(),
				windowmenu_create_item("_E_xit", menu_system_exit, NULL),
				NULL),
			//menu_coreopt,
			menu_with_rom_only(windowmenu_create_submenu("__Cheats",
				menu_with_rom_only(windowmenu_create_item("_Cheat _List", menu_cheat_list, NULL)),
				menu_with_rom_only(windowmenu_create_item("_Cheat _Search", menu_cheat_search, NULL)),
				menu_cheat_enable=menu_with_rom_only(windowmenu_create_check("__Enable Cheats", menu_cheat_enable_f, NULL)),
				NULL)),
			NULL);
		update_coreopt_menu(menu, 1);
		wndw->set_menu(wndw, menu);
	}
	else
	{
		update_coreopt_menu(menu, 1);
		menu_system->insert_child(menu_system, 1, menu_system_core);
	}
	
	menu_cheat_enable->set_state(menu_cheat_enable, cheats->get_enabled(cheats));
	
	for (unsigned int i=0;i<numromonlyitems;i++)
	{
		romonlyitems[i]->set_enabled(romonlyitems[i], (romloaded));
	}
}

int main(int argc, char * argv[])
{
	window_init(&argc, &argv);
	initialize(argc, argv);
//video::shader_parse("g.cg");exit(0);
if
(config.firstrun)
window_message_box(
"This piece of software is far from finished. There is no configuration panel, and some components are bad at emitting error messages.\r\n"
"All valid settings will show up in minir.cfg, which will appear beside the executable once it's closed.\r\n",
"minir", mb_warn, mb_ok);
config.firstrun
=false;
#ifdef DEBUG
//cheats->show_search(cheats);
//cheats->show_list(cheats);
#endif
	mainloop();

	deinit();
}
