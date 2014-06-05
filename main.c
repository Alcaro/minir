#include "minir.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#define VERSION "0.90"

//yes, this file is a mess; the plan is to rewrite it from scratch.

extern void unalign_lock();
extern void unalign_unlock();
#if __x86_64__
asm(
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
asm(
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

struct video * vid;
struct audio * aud;
struct inputraw * inpraw;
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
//<0: draw NULL N times per frame, divide audio rate by (N+1)
int speed_change;
int speed_change_num_blanks;

bool pause;
uint64_t pause_next_fwd;

bool handle_cli_args(const char * const * filenames, bool coresonly);



bool try_create_interface_video(const char * interface, unsigned int videowidth, unsigned int videoheight,
                                                        unsigned int videodepth, double videofps)
{
	vid=(config.video_thread ? video_create_thread : video_create)(interface, draw->get_window_handle(draw),
										videowidth*config.video_scale, videoheight*config.video_scale, videodepth, videofps);
//printf("create %s = %p\n",interface,vid);
	if (vid)
	{
		if (interface!=config.driver_video)
		{
			free(config.driver_video);
			config.driver_video=strdup(interface);
		}
		return true;
	}
	return false;
}

bool try_create_interface_audio(const char * interface)
{
	aud=audio_create(interface, draw->get_window_handle(draw),
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

bool try_create_interface_input(const char * interface)
{
	inpraw=inputraw_create(interface, draw->get_window_handle(draw));
	if (inpraw)
	{
		if (interface!=config.driver_input)
		{
			free(config.driver_input);
			config.driver_input=strdup(interface);
		}
		return true;
	}
	return false;
}

void create_interfaces(unsigned int videowidth, unsigned int videoheight, unsigned int videodepth, double videofps)
{
	if (vid) vid->free(vid); vid=NULL;
	if (aud) aud->free(aud); aud=NULL;
	if (inpraw) inpraw->free(inpraw); inpraw=NULL;
	
	if (!config.driver_video || !try_create_interface_video(config.driver_video, videowidth, videoheight, videodepth, videofps))
	{
		const char * const * drivers=video_supported_backends();
		while (true)
		{
			if (try_create_interface_video(*drivers, videowidth, videoheight, videodepth, videofps)) break;
			drivers++;
		}
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
	
	if (!config.driver_input || !try_create_interface_input(config.driver_input))
	{
		const char * const * drivers=inputraw_supported_backends();
		while (true)
		{
			if (try_create_interface_input(*drivers)) break;
			drivers++;
		}
	}
}

void reset_config()
{
	//calling this before creating the window is so config.video_scale can get loaded properly, which helps initial placement
	//and also for savestate_auto, which is also used before creating the window; it could be delayed, but this method is easier.
	if (romloaded==coreloaded) config_load(NULL, romloaded);
	else config_load(coreloaded, romloaded);
	if (!wndw) return;
	
	unsigned int videowidth=320;
	unsigned int videoheight=240;
	unsigned int videodepth=16;
	double videofps=60.0;
	
	if (core) core->get_video_settings(core, &videowidth, &videoheight, &videodepth, &videofps);
	
	draw->resize(draw, videowidth*config.video_scale, videoheight*config.video_scale);
	draw->set_hide_cursor(draw, config.cursor_hide);
	
	create_interfaces(videowidth, videoheight, videodepth, videofps);
	
	aud->set_sync(aud, config.audio_sync);
	vid->set_sync(vid, config.video_sync);
	
	inp->set_input(inp, inpraw);
	
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

bool study_core(const char * path)
{
	struct libretro * thiscore=libretro_create(path, NULL, NULL);
	if (!thiscore) return false;
	config_create_core(path, true, thiscore->name(thiscore), thiscore->supported_extensions(thiscore));
	thiscore->free(thiscore);
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
	
	config_create_core(path, true, core->name(core), core->supported_extensions(core));//why not
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
	const char * const * cores=window_file_picker(wndw, "Select Libretro cores", extension, "Libretro cores", true, true);
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
	if (extension && !strcmp(extension, dylib_ext()))
	{
		if (load_core_as_rom(rom)) return true;
		//I doubt there is any core that can load a dll, but why not try? Worst case, it just fails, as it would have anyways.
	}
	if (!core ||
			(extension && !core->supports_extension(core, extension+1)))
	{
		struct minircorelist * newcores;
		newcores=config_get_core_for(rom, NULL);
		if (config.auto_locate_cores)
		{
			if (!newcores[0].path)
			{
				free(newcores);
				const char * const * cores=libretro_nearby_cores(rom);
				for (int i=0;cores[i];i++) study_core(cores[i]);
				newcores=config_get_core_for(rom, NULL);
			}
			if (!newcores[0].path)
			{
				free(newcores);
				const char * const * cores=libretro_default_cores();
				for (int i=0;cores[i];i++) study_core(cores[i]);
				newcores=config_get_core_for(rom, NULL);
			}
		}
		if (!newcores[0].path)
		{
			//MBOX:No Libretro core found for this file type. Do you wish to look for one?
			free(newcores);
			if (!select_cores(rom)) return false;
			newcores=config_get_core_for(rom, NULL);
		}
		unload_rom();
		if (!load_core(newcores[0].path, false))
		{
			config_delete_core(newcores[0].path);
			if (wndw) wndw->set_title(wndw, "minir");
			//MBOX: "Couldn't load core at %s", newcores[0].path
			return false;
		}
		free(newcores);
	}
	unload_rom();
	if (!core->load_rom(core, rom))
	{
		if (wndw) wndw->set_title(wndw, "minir");
		//MBOX: "Couldn't load %s with %s", romloaded, core->name(core)
		return false;
	}
	
	romloaded=strdup(rom);
	char * basenamestart=strrchr(romloaded, '/');
	if (basenamestart) basenamestart++;
	else basenamestart=romloaded;
	char * basenameend=strrchr(basenamestart, '.');
	if (basenameend) *basenameend='\0';
	config_create_game(rom, false, basenamestart);
	if (basenameend) *basenameend='.';
	
	load_rom_finish();
	
	return true;
}

bool load_core_as_rom(const char * rom)
{
	if (!load_core(rom, false) || !core->load_rom(core, NULL))
	{
		wndw->set_title(wndw, "minir");
		return false;
	}
	romloaded=coreloaded;
	
	config_create_game(rom, true, core->name(core));
	load_rom_finish();
	
	return true;
}

void load_rom_finish()
{
	free(state_buf);
	state_size=core->state_size(core);
	state_buf=malloc(state_size);
	
	reset_config();
	
	size_t sramsize;
	void* sramptr;
	core->get_memory(core, libretromem_sram, &sramsize, &sramptr);
	file_read_to(sram_path(), sramptr, sramsize);
	
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
	const char * * extensions=config_get_supported_extensions();
	if (!*extensions && config.auto_locate_cores)
	{
		free(extensions);
		const char * const * cores=libretro_default_cores();
		for (int i=0;cores[i];i++) study_core(cores[i]);
		extensions=config_get_supported_extensions();
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
	
	const char * load=NULL;
	bool load_is_core=false;
	const char * ext=dylib_ext();
	for (int i=0;filenames[i];i++)
	{
		const char * path=filenames[i];
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
					if (!load) load=path;
					else badload=true;
				}
				else
				{
					config_create_core(path, true, thiscore->name(thiscore), thiscore->supported_extensions(thiscore));
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
			if (!load) load=path;
			else badload=true;
		}
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
		return true;
	}
	
	return false;
}

void drop_handler(struct widget_viewport * subject, const char * const * filenames, void* userdata)
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
	window_init(&argc, &argv);
	
	create_self_path(argv[0]);
	
	strcpy(selfpathend, selfname);
	strcat(selfpathend, ".cfg");
	config_read(selfpath);
	
	if (!handle_cli_args((const char * const *)argv+1, false))
	{
		const char * defautoload=config_get_autoload();
		if (defautoload) load_rom(defautoload);
	}
	
	reset_config();
	
	unsigned int videowidth=320;
	unsigned int videoheight=240;
	unsigned int videodepth=16;
	double videofps=60.0;
	
	if (core) core->get_video_settings(core, &videowidth, &videoheight, &videodepth, &videofps);
	
	draw=widget_create_viewport(videowidth*config.video_scale, videoheight*config.video_scale);
	wndw=window_create(draw);
	wndw->set_title(wndw, "minir");//in case the previous one didn't work
	wndw->onclose(wndw, closethis, NULL);
	set_window_title();
	
	draw->set_support_drop(draw, drop_handler, NULL);
	
	const int align[]={0,2};
	const int divider=180;
	wndw->statusbar_create(wndw, 2, align, &divider);
	
	if (statusbar_to_load)
	{
		wndw->statusbar_set(wndw, 0, statusbar_to_load);
		free(statusbar_to_load);
	}
	
	retroinp=libretroinput_create(NULL);
	inp=inputmapper_create(NULL);
	rewind=rewindstack_create(0, 0);
	
	cheats=minircheats_create();
	cheats->set_parent(cheats, wndw);
	
	update_menu();
	wndw->set_visible(wndw, true);
	reset_config();
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
	struct image img;
	vid->repeat_frame(vid, &img.width, &img.height, (const void**)&img.pixels, &img.pitch, &img.bpp);
	if (!img.pixels) goto bad;
	void* pngdata;
	unsigned int pnglen;
	
	const char * comments[]={
		"Software", NULL,
		NULL
	};
	asprintf((char**)&comments[1],
	         "minir v"VERSION"\ncore: %s (%s)\ngame: %s (%s)",
	         config.corename, coreloaded, config.gamename, romloaded);
	
	if (!png_encode(&img, comments, &pngdata, &pnglen))
	{
		free((char*)comments[1]);
		goto bad;
	}
	free((char*)comments[1]);
	if (!file_write(get_screenshot_path(), pngdata, pnglen))
	{
		free(pngdata);
		goto bad;
	}
	free(pngdata);
	
	set_status_bar("Screenshot saved");
	return true;
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
				
				char* state=rewind->push_begin(rewind);
				memcpy(state+0, &rewind_timer, sizeof(rewind_timer));
				if (core->state_save(core, state+sizeof(rewind_timer), state_size))
				{
					rewind->push_end(rewind);
				}
				else rewind->push_cancel(rewind);
			}
			else
			{
				const char* state=rewind->pull(rewind);
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
		vid->set_sync(vid, config.video_sync && !turbo);
		
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
		if (!turbo) vid->set_sync(vid, (speed_change_num_blanks==0) && config.video_sync);
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

		
		if (statusbar_expiry && now>statusbar_expiry)
		{
			wndw->statusbar_set(wndw, 0, "");
			statusbar_expiry=0;
		}
		
		if (wndw->is_active(wndw)) inp->poll(inp);
		else inp->clear(inp);

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
				unsigned int width;
				unsigned int height;
				unsigned int pitch;
				vid->repeat_frame(vid, &width, &height, NULL, &pitch, NULL);
				vid->draw(vid, width, height, NULL, pitch);//skip reuploading the texture; why would we?
			}
		}
		
		if (!romloaded ||
		    (config.defocus_pause && !wndw->is_active(wndw)) ||
		    (!config.defocus_pause && wndw->menu_active(wndw)))
		{
			aud->clear(aud);
			inp->clear(inp);
			cheats->update(cheats, true);
			wndw->statusbar_set(wndw, 1, "Paused");
			draw->set_hide_cursor(draw, false);
			while (!exit_called && wndw->is_visible(wndw) &&
			      (!romloaded || !wndw->is_active(wndw)))
			{
				window_run_wait();
			}
			if (exit_called || !wndw->is_visible(wndw)) break;
			draw->set_hide_cursor(draw, config.cursor_hide);
			
			//get rid of last(), if someone is holding something on focus (they shouldn't, but let's let them anyways.)
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
		strcpy(selfpathend, selfname);
		strcat(selfpathend, ".cfg");
		config_write(selfpath);
	}
	
	free(state_buf);
	
	vid->free(vid); vid=NULL; // this gets angry on GTK+. I think I'm trying to destroy an already-dead child window.
	aud->free(aud); aud=NULL;
	inp->free(inp); inp=NULL;
	inpraw->free(inpraw); inpraw=NULL;
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

struct windowmenu * update_coreopt_menu(struct windowmenu * parent, bool * enable)
{
	static struct windowmenu * menu=NULL;
	if (menu && core && !core->get_core_options_changed(core)) return menu;
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
		*enable=false;
		return menu;
	}
	*enable=true;
	
	for (unsigned int i=0;i<numopts;i++)
	{
		unsigned int numvalues=opts[i].numvalues;
		
		bool yesfirst;
		if (numvalues==numvalues && is_yesno(opts[i].values[0], opts[i].values[1], &yesfirst))
		{
			struct windowmenu * item;
			item=windowmenu_create_check(opts[i].name_display, yesfirst ? set_core_opt_bool_invert : set_core_opt_bool, (void*)(uintptr_t)i);
			menu->insert_child(menu, i, item);
			item->set_state(item, core->get_core_option(core, i) ^ yesfirst);
		}
		else
		{
			struct windowmenu * radioitem=windowmenu_create_radio_l(numvalues, opts[i].values, set_core_opt_normal, (void*)(uintptr_t)i);
			struct windowmenu * menuitem=windowmenu_create_submenu(opts[i].name_display, radioitem, NULL);
			menu->insert_child(menu, i, menuitem);
			radioitem->set_state(radioitem, core->get_core_option(core, i));
		}
	}
	return menu;
}

void menu_system_rom(struct windowmenu * subject, void* userdata)
{
	select_rom();
}

void menu_system_reset_f(struct windowmenu * subject, void* userdata)
{
	core->reset(core);
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

void update_menu()
{
	static struct windowmenu * menu_system;
	static struct windowmenu * menu_system_core=NULL;
	static struct windowmenu * menu_system_reset;
	static struct windowmenu * menu_cheat_enable=NULL;
	
	bool menu_coreopt_enable;
	struct windowmenu * menu_coreopt=update_coreopt_menu(menu, &menu_coreopt_enable);
	
	if (menu_system_core) menu_system->remove_child(menu_system, menu_system_core);
	menu_system_core=windowmenu_create_submenu("__Core", NULL);
	menu_system_core->insert_child(menu_system_core, 0, windowmenu_create_item("(TODO: Implement this)", NULL, NULL));
	
	if (!menu)
	{
		menu=windowmenu_create_topmenu(
			menu_system=windowmenu_create_submenu("__System", 
				windowmenu_create_item("__Load ROM", menu_system_rom, NULL),
				menu_system_core,
				windowmenu_create_separator(),
				menu_system_reset=windowmenu_create_item("__Reset", menu_system_reset_f, NULL),
				windowmenu_create_separator(),
				windowmenu_create_item("_E_xit", menu_system_exit, NULL),
				NULL),
			menu_coreopt,
			windowmenu_create_submenu("__Cheats",
				windowmenu_create_item("_Cheat _List", menu_cheat_list, NULL),
				windowmenu_create_item("_Cheat _Search", menu_cheat_search, NULL),
				menu_cheat_enable=windowmenu_create_check("__Enable Cheats", menu_cheat_enable_f, NULL),
				NULL),
			NULL);
		wndw->set_menu(wndw, menu);
	}
	else
	{
		if (menu_coreopt) menu->insert_child(menu, 1, menu_coreopt);
		menu_system->insert_child(menu_system, 1, menu_system_core);
		menu_coreopt->set_enabled(menu_coreopt, menu_coreopt_enable);
	}
	
	menu_cheat_enable->set_state(menu_cheat_enable, cheats->get_enabled(cheats));
}
/*
void menu_system_core_any_action(unsigned int id, unsigned int state, void* userdata)
{
	load_core(userdata, true);
}

void menu_system_core_more_action(unsigned int id, unsigned int state, void* userdata)
{
	select_cores(NULL);
}

void update_menu()
{
	struct menuitem * menu_system_core_inner=NULL;
	int current_core_id=-1;
	
	if (romloaded)
	{
		bool round2=false;
		bool anyfixed=false;
		do
		{
			if (anyfixed) round2=true;
			anyfixed=false;
			
			unsigned int numcores;
			struct minircorelist * cores_for_this=config_get_core_for(romloaded, &numcores);
			menu_system_core_inner=calloc(numcores+1, sizeof(struct menuitem));
			
			for (int i=0;i<numcores;i++)
			{
				menu_system_core_inner[i].type=menu_radio;
				menu_system_core_inner[i].text=cores_for_this[i].name;
				if (!cores_for_this[i].name)
				{
					if (!study_core(cores_for_this[i].path)) config_delete_core(cores_for_this[i].path);
					anyfixed=true;
				}
				menu_system_core_inner[i].action=menu_system_core_any_action;
				menu_system_core_inner[i].userdata=(void*)cores_for_this[i].path;
				menu_system_core_inner[i].id=menu_system_core_any_id;
				
				if (!strcmp(coreloaded, cores_for_this[i].path)) current_core_id=i;
			}
			menu_system_core_inner[numcores].type=menu_end;
			
			free(cores_for_this);
		} while (anyfixed && !round2);
	}
	else
	{
		menu_system_core_inner=calloc(2, sizeof(struct menuitem));
		menu_system_core_inner[0].type=menu_item;
		menu_system_core_inner[0].text="(no ROM loaded)";
		menu_system_core_inner[0].disabled=true;
		menu_system_core_inner[1].type=menu_end;
	}
*/


int main(int argc, char * argv[])
{
	initialize(argc, argv);
if
(config.firstrun)
window_firstrun
();
config.firstrun
=false;
cheats->show_search(cheats);
//cheats->show_list(cheats);
	mainloop();
	deinit();
}
