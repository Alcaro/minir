#pragma once
#include "global.h"

class video;//io.h
class file; //file.h
struct retro_memory_descriptor; //libretro.h
struct retro_hw_render_callback;//libretro.h


#ifdef NEED_ICON_PNG
extern const unsigned char icon_minir_64x64_png[1300];
#endif



//A compressing, lossy stack. Optimized for large, mostly similar, blocks of data; optimized for
// writing, less so for reading.
//"Lossy" means that it will discard old data if its capacity is exhausted. It will not give out any
// memory block it wasn't given.
struct rewindstack {
	//This is equivalent to deleting and recreating the structure, but may be faster.
	//It is safe to set the capacity to 0, though this will make the structure rather useless.
	//The structure may hand out bigger blocks of data than requested. This is not detectable; just
	// ignore the extra bytes.
	//The structure may allocate a reasonable multiple of blocksize, in addition to capacity.
	//It is not possible to accurately predict how many blocks will fit in the structure; it varies
	// depending on how much the data changes. Emulator savestates are usually compressed to about
	// 0.5-2% of their original size, but this varies depending on various factors. For exact numbers,
	// stick in some data and use capacity().
	void (*reset)(struct rewindstack * This, size_t blocksize, size_t capacity);
	
	//Asks where to put a new block. Size is same as blocksize. Don't read from it; contents are undefined.
	//push_end or push_cancel must be the first function called on the structure after this; not even free() is allowed.
	//This function cannot fail, though a pull() directly afterwards may fail.
	void * (*push_begin)(struct rewindstack * This);
	//Tells that the savestate has been written. Don't use the pointer from push_begin after this point.
	void (*push_end)(struct rewindstack * This);
	//Tells that nothing usable was written to the pointer from push_begin. Equivalent to push_end+pull,
	// but faster, and may avoid discarding something. The user is allowed to have written to the pointer.
	void (*push_cancel)(struct rewindstack * This);
	
	//Pulls off a block. Don't change it; it will be used to generate the next one. The returned pointer is only
	// guaranteed valid until the first call to any function in this structure, with the exception that capacity()
	// will not invalidate anything. If the requested block has been discarded, or was never pushed, it returns NULL.
	const void * (*pull)(struct rewindstack * This);
	
	//Tells how many entries are in the structure, how many bytes are used, and whether the structure
	// is likely to discard something if a new item is appended. The full flag is guaranteed true if
	// it has discarded anything since the last pull() or reset(); however, it may be set even before
	// discarding, if the implementation believes that will simplify the implementation.
	void (*capacity)(struct rewindstack * This, unsigned int * entries, size_t * bytes, bool * full);
	
	void (*free)(struct rewindstack * This);
};
struct rewindstack * rewindstack_create(size_t blocksize, size_t capacity);




class minirdevice : nocopy {
	//- input-handling devices will be sorted into multiple groups
	// - source group (keyboard/mouse/etc)
	//  - can create input events, but can't do anything else
	//  - can execute spontaneously - events are buffered until 
	// - filter group (keyjoy, mousejoy, hotkeys?)
	//  - can send more input, but can not discard existing input
	//  - how do I thread them? mousejoy wants to see keyjoy output
	//  - do all filters see requested events from all others?
	//  - do I put hotkeys here?
	//  - can send requests for all kinds of stuff
	//   - pause
	//   - save savestate
	//   - load savestate
	//   - do I put them all in the same function and switch() them?
	// - exclusive filter (netplay, video playback (it discards all other input), mousejoy during the setup step)
	//  - can only have one
	//  - can consume input events and send out new ones
	//  - can also reject savestates
	//  - can execute additional frames
	//   - can specify whether a frame is final
	// - sink group (video recording, core itself)
	//  - all see the same input
	//  - only core can emit events
	//  - can be threaded - some of them are expensive
	//  - video record will want to modify savestates to append an input log
	//- groups can be empty, except sink group because a core is mandatory
	//- each device, including core, needs to tell what it wants and produces (can libretro do that, or do I hardcode something?)
	//- there will be a device manager to keep track of them all
	//- no function<>, this is too complex for that
	// - virtual objects will be used
	// - the exclusive filter must pass the events on to next device
	// - others don't need to do that
	
	//- output handling must do different things
	// - the core is the only source
	// - audio resampler must be somewhere
	// - video3d flattener must be somewhere
	// - filter: real-time rewind must reverse the audio
	// - multiple sinks
	//  - thread them
	//  - can specify max frames to buffer
	//   - 0 - a/vsync, next frame must not start before this is done
	//   - 1 or more - handler will buffer it
	//    - must specify a max lag or we run out of ram
	//  - frame number?
	//   - netplay replay
	//   - netplay preliminary
	//   - savestate
};




class libretro : nocopy {
public:
	//Any returned pointer is, unless otherwise specified, valid only until the next call to a function here, and freed by this object.
	//Input pointers are, unless otherwise specified, not expected valid after the function returns.
	
	//The name of the core.
	virtual const char * name() = 0;
	
	//Return value format is { "smc", "sfc", NULL }.
	virtual const char * const * supported_extensions(unsigned int * count) = 0;
	
	//This one is also without the dot.
	virtual bool supports_extension(const char * extension) = 0;
	
	enum {
		f_load_filename= 0x0001,//Can load a ROM from a filename.
		f_load_mem     = 0x0002,//Can load a ROM from a memory block.
		f_load_virt_file=0x0004,//Can load a ROM from a file stream.
		f_load_none    = 0x0008,//Doesn't require a ROM.
	};
	virtual uint32_t features() = 0;
	
	//The interface pointers must be valid during every call to run().
	//It is safe to attach new interfaces without recreating the structure.
	//It is safe to attach new interfaces if the previous ones are destroyed.
	virtual void attach_interfaces(video* v, struct audio * a, struct libretroinput * i) = 0;
	
	//The callee will own the returned object and shall treat it as if it is the attached video driver.
	virtual void enable_3d(function<video*(struct retro_hw_render_callback * desc)> creator) = 0;
	
	//The object will take ownership of the given argument. If you too want it, use clone().
	virtual bool load_rom(file* data) = 0;
	
	//The following are only valid after a game is loaded.
	
	virtual void get_video_settings(unsigned int * width, unsigned int * height, videoformat * depth, double * fps) = 0;
	virtual double get_sample_rate() = 0;
	
	struct coreoption {
		const char * name_internal;
		const char * name_display;
		
		//This one is hackishly calculated by checking whether it's checked if during retro_run.
		bool reset_only;
		
		unsigned int numvalues;
		const char * const * values;
	};
	//The core options will be reported as having changed on a freshly created core,
	// even if there are no options. The flag is cleared by calling this function.
	virtual bool get_core_options_changed() = 0;
	//The list is terminated by a { NULL, NULL, false, 0, NULL }.
	//The return value is invalidated by run() or free(), whichever comes first.
	virtual const struct coreoption * get_core_options(unsigned int * numopts) = 0;
	//It is undefined behaviour to set a nonexistent option, or to set an option to a nonexistent value.
	virtual void set_core_option(unsigned int option, unsigned int value) = 0;
	virtual unsigned int get_core_option(unsigned int option) = 0;
	
	//You can write to the returned pointer.
	//Will return 0:NULL if the core doesn't know what the given memory type is.
	//(If that happens, you can still read and write the indicated amount to the pointer.)
	enum memtype { // These IDs are the same as RETRO_MEMORY_*.
		mem_sram,
		mem_unused1,
		mem_wram,
		mem_vram
	};
	virtual void get_memory(memtype which, size_t * size, void* * ptr) = 0;
	
	virtual const struct retro_memory_descriptor * get_memory_info(unsigned int * nummemdesc) = 0;
	
	virtual void reset() = 0;
	
	virtual size_t state_size() = 0;
	virtual bool state_save(void* state, size_t size) = 0;
	virtual bool state_load(const void* state, size_t size) = 0;
	
	virtual void run() = 0;
	
	virtual ~libretro() = 0;
	
	//The message notification may be called before libretro_create returns. It may even be called if the
	// function returns NULL afterwards. It can be NULL, in which case the messages will be discarded.
	//It is safe to free this item without loading a ROM.
	//Since a libretro core is a singleton, only one libretro structure may exist for each core. For the purpose of the
	// previous sentence, loading the dylib through other ways than this function counts as creating a libretro structure.
	//If one existed already, 'existed' will be set to true. For success, and for other failures, it's false.
	//TODO: userdata in message_cb
	static libretro* create(const char * corepath, void (*message_cb)(int severity, const char * message), bool * existed);
	
	//Returns whatever libretro cores the system can find. The following locations are to be searched for all dylibs:
	//- The directory of the executable
	//- All subdirectories of the directory the executable is in (but not subdirectories of those)
	//- Any other directory the system feels like including, including system directories
	//If the scanned directories can be expected to contain large amounts of non-libretro dylibs, a
	// filename-based filter will be applied to reject anything that does not look like a libretro
	// core. A low number of false positives is fine, but returning the entire /usr/lib/ is not appropriate.
	//The return value may contain duplicates, may contain non-libretro dylibs, and may even contain non-dylibs.
	//The return value is invalidated by the next call to this function, or libretro_nearby_cores.
	//
	//For example, the following directory structure would work out of the box:
	// emulators/
	//   minir.exe
	//   cores/
	//     snes9x_libretro.dll
	//     gambatte_libretro.dll
	//   roms/
	//     zeldaseasons.gbc
	//     kirby3.smc
	static const char * const * default_cores();
	
	//Equivalent to libretro_default_cores, but looks near the given path instead.
	static const char * const * nearby_cores(const char * rompath);
};
inline libretro::~libretro(){}



//Turns old SNES/etc games into something mouse-controlled.
class libretro_mousejoy : nocopy {
public:
	libretro_mousejoy* create();
	
	//Resources used: mem_wram, video_settings, video output, joypad[WRITE]
	//TODO: rewrite libretro input, needs to be push based
	virtual void attach(libretro* obj) = 0;
	
	//Disabling this makes it release all input, allowing other input sources to do their job. Starts in the disabled state.
	virtual void enable(bool enable) = 0;
	
	//x and y are in pixels from the top left corner. Negative values are fine.
	//Buttons are a bitfield, input.h::inputmouse::button.
	virtual void update(int x, int y, uint32_t buttons) = 0;
	
	//TODO: save state
	//TODO: configure which joy button each mouse button points to
	
	virtual ~libretro_mousejoy() = 0;
};
inline libretro_mousejoy::~libretro_mousejoy(){}



//An input mapper that converts the interface of an inputmapper to whatever a libretro core
// understands. It's roughly a joypad emulator. The input mapper is assumed polled elsewhere.
struct libretroinput {
	//Polls input. Same interface as libretro.
	int16_t (*query)(struct libretroinput * This, unsigned port, unsigned device, unsigned index, unsigned id);
	
	//Sets the input handler. It is still usable for other things while attached to a libretroinput.
	// It is not deleted once this structure is deleted.
	void (*set_input)(struct libretroinput * This, struct inputmapper * in);
	
	//Tells where the libretroinput should ask the inputmapper for the first used key. Order is rather
	// illogical; B, Y, Select, Start, Up, Down, Left, Right, A, X, L, R, L2, R2, L3, R3.
	//len is how many input slots should be used, including the first one. They must be consecutive,
	// and should be a multiple of 16.
	void (*joypad_set_inputs)(struct libretroinput * This, unsigned port, unsigned int inputstart, unsigned int len);
	
	//Whether to blocks left+right and up+down. Defaults to allowed.
	void (*joypad_set_block_opposing)(struct libretroinput * This, bool block);
	
	void (*free)(struct libretroinput * This);
};

struct libretroinput * libretroinput_create(struct inputmapper * in);



struct configcorelist {
	const char * path;
	const char * name;
};

enum configscope {
	cfgsc_default,
	cfgsc_default_globonly,
	cfgsc_global,
	cfgsc_global_globonly,
	cfgsc_core,
	cfgsc_invalid1,//this would be by-core global-only, which doesn't make sense
	cfgsc_game
};

enum configverbosity {
	cfgv_minimal,
	cfgv_nocomments,
	cfgv_default,
	cfgv_maximum
};

struct configdata {
	//Only config.c may access the items starting with underscores.
	
	char * corename;
	char * _corepath;
	char * gamename;
	char * _gamepath;
	
	UNION_BEGIN
		//cores
		STRUCT_BEGIN
			char* * support;
			char* * primary;
		STRUCT_END
		
		//used for games, only inside config.c - marking as unioned with support/primary since they're not applicable for games
		STRUCT_BEGIN
			char * _forcecore;
		STRUCT_END
	UNION_END
	
#define CONFIG_HEADER
//this also defines CONFIG_ENUM_INPUT
#include "obj/generated.cpp"
#undef CONFIG_HEADER
	
	//these are at the end for packing reasons
	bool firstrun;
	
	bool _autoload;
};

enum input { CONFIG_ENUM_INPUT };

struct minirconfig {
	//Tells which game to autoload. Can be NULL if none. Don't free it.
	const char * (*get_autoload)(struct minirconfig * This);
	//Returns { "smc", "sfc", NULL } for the extensions supported by any core.
	//Free it when you're done, but don't free the pointers inside.
	struct configcorelist * (*get_core_for)(struct minirconfig * This, const char * gamepath, unsigned int * count);
	//This one should also be freed. Its contents should not.
	const char * * (*get_supported_extensions)(struct minirconfig * This);
	
	//This one loads config for the given core and game.
	//NULL is valid for either or both of them. It is not an error if a given entry doesn't exist; it will be created.
	//If the given game demands a specific core, the given core will be ignored. The game will always be honored unless it's NULL.
	//The caller gets ownership of everything in 'config'. When you're done, use data_free().
	void (*data_load)(struct minirconfig * This, struct configdata * config,
	                  bool free_old, const char * corepath, const char * gamepath);
	//To change anything permanently, free() the old value if needed and hand in the new one.
	//NULL is treated identically to an empty item.
	//If anything is written to 'support' and no core has an entry for that in 'primary', it will be created.
	//If anything is written to 'primary', it will be deleted from the entries for all other cores.
	//Nothing is freed; the caller is responsible for cleaning out the structure.
	void (*data_save)(struct minirconfig * This, struct configdata * config);
	//Frees all pointers in 'config' and sets them to NULL. 'config' itself is not freed.
	void (*data_free)(struct minirconfig * This, struct configdata * config);
	
	//Removes all data for a core or game.
	void (*data_destroy)(struct minirconfig * This, const char * item);
	
	//This one writes the configuration back to disk, if changed.
	void (*write)(struct minirconfig * This, const char * path);
	
	void (*free)(struct minirconfig * This);
};
struct minirconfig * config_create(const char * path);



struct minir {
	
};

//All functions on this object yield undefined behaviour if datsize is not within 1..4.
//For easier transferability to other projects, this object does not call any other part of minir,
// so you'll have to do a fair bit of stuff by yourself.
enum cheat_compfunc { cht_lt, cht_gt, cht_lte, cht_gte, cht_eq, cht_neq };
enum cheat_chngtype { cht_const, cht_inconly, cht_deconly, cht_once };
struct retro_memory_descriptor;
struct cheat {
	char * addr;//For cheat_set and code_create, this is only read, and may be a casted const char *.
	            //For cheat_get and code_parse, this is a caller-allocated buffer of size 32 or larger, and will be written.
	uint32_t val;
	unsigned char changetype;//this is an enum cheat_chngtype, but I want sizeof==1, so I have to do this.
	unsigned char datsize;
	bool enabled;
	bool issigned;
	const char * desc;//Do not free if you get this from cheat_get; it's owned by the cheat model.
};
struct minircheats_model {
	void (*set_memory)(struct minircheats_model * This, const struct retro_memory_descriptor * memory, unsigned int nummemory);
	
	//The relevant size is how much memory it would take to create the 'prev' arrays.
	//It doesn't change depending on whether they exist, and doesn't account for malloc overhead.
	//Defaults to disabled, and switches to that on every set_memory.
	size_t (*prev_get_size)(struct minircheats_model * This);
	void (*prev_set_enabled)(struct minircheats_model * This, bool enable);
	bool (*prev_get_enabled)(struct minircheats_model * This);
	
	void (*search_reset)(struct minircheats_model * This);
	void (*search_set_datsize)(struct minircheats_model * This, unsigned int datsize);//Default 1.
	void (*search_set_signed)(struct minircheats_model * This, bool issigned);//Default unsigned.
	void (*search_do_search)(struct minircheats_model * This,
	                         enum cheat_compfunc compfunc, bool comptoprev, uint32_t compto);
	
	size_t (*search_get_num_rows)(struct minircheats_model * This);
	//Returns all information about a currently visible row. The data size is set by the functions above.
	//The address will be written to, and must be at least 32 bytes long (31 plus NUL).
	//If 'prev' is disabled, 'prevval' will remain unchanged if queried.
	void (*search_get_row)(struct minircheats_model * This, size_t row,
	                       char * addr, uint32_t * val, uint32_t * prevval);
	
	//Returns the ID of the visible row starting with the given prefix closest to 'start', in the given direction.
	//Returns (size_t)-1 if there is no such row.
	size_t (*search_find_row)(struct minircheats_model * This, const char * prefix, size_t start, bool up);
	
	//Threading works like this:
	//First, call thread_enable and tell how many threads are available on this CPU.
	//Then, after each search_do_search, call thread_do_work for each threadid from 0 to numthreads-1,
	// from one thread each. The work may not necessarily be evenly split, especially not if the total
	// workload is small. Some threads may return without doing anything at all.
	//Once all thread_do_work have returned, call thread_finish_work.
	//Only thread_do_work may be called from any thread other than the creating thread, and no other
	// function may be called while any thread is inside thread_do_work. It is safe to call
	// thread_do_work from other threads than the creator, including for threadid==0.
	//Nothing except thread_do_work and thread_get_count may be called between search_do_search and thread_finish_work.
	//If the structure sees that it has only use for a finite number of threads, thread_get_count may
	// return a smaller value than given to thread_enable. However, it is safe to call thread_do_work
	// with excess threads; they will return without doing anything.
	//Note that while performance theoretically should be linear with the number of CPU cores, it is
	// not in practice. It is linear for some workloads, but for the expected workloads, it's
	// sublinear (though it does speed up a little). Reasons aren't known for sure, but memory
	// bandwidth limitations are likely.
	void (*thread_enable)(struct minircheats_model * This, unsigned int numthreads);
	unsigned int (*thread_get_count)(struct minircheats_model * This);
	void (*thread_do_work)(struct minircheats_model * This, unsigned int threadid);
	void (*thread_finish_work)(struct minircheats_model * This);
	
	//Cheat code structure:
	//disable address value signspec direction SP desc
	//disable is '-' if the cheat is currently disabled, otherwise blank.
	//address is a namespace identifier and an address inside this prefix, in hex. There is no
	// separator from the value; all addresses in a namespace have the same length.
	//value is what to set it to, also in hex. It's either two, four, six or eight digits.
	//signspec is 'S' if the cheat code is signed, or empty otherwise. For addresses
	// not allowed to change, the sign makes no difference, and should be empty.
	//direction is '+' if the value at this address is allowed to increase, '-' if it's allowed to
	// decrease, '=' for single-use cheats, or empty if the given value should always be there.
	//SP is a simple space character. Optional if the description is blank.
	//desc is a human readable description of the cheat code. May not contain ASCII control characters
	// (0..31 and 127), but is otherwise freeform.
	//The format is designed so that a SNES Gameshark code is valid.
	
	//Returns the longest possible size for a valid address.
	unsigned int (*cheat_get_max_addr_len)(struct minircheats_model * This);
	
	//This one tells the current value of an address.
	//Fails if:
	//- There is no such namespace
	//- That address is not mapped in that namespace
	//- The relevant memory block ends too soon
	//- Alignment says you can't use that address
	bool (*cheat_read)(struct minircheats_model * This, const char * addr, unsigned int datsize, uint32_t * val);
	
	//Don't cache cheat IDs; use the address as unique key.
	//If the address is invalid, or not targeted by any cheats, it returns -1.
	int (*cheat_find_for_addr)(struct minircheats_model * This, unsigned int datsize, const char * addr);
	
	unsigned int (*cheat_get_count)(struct minircheats_model * This);
	//To add a new cheat, use pos==count. Note that if changetype==cht_once, it will be used, but not added to the list.
	//To check if a cheat is valid without actually adding it, use pos==-1.
	//The possible errors are the same as cheat_read. In all such cases, it's fair to blame the address.
	//It is not guaranteed that cheat_get returns the same values as given to cheat_set; for example, mirroring may be undone.
	//However, it is guaranteed that setting a cheat to itself will do nothing.
	bool (*cheat_set)(struct minircheats_model * This, int pos, const struct cheat * newcheat);
	void (*cheat_get)(struct minircheats_model * This, unsigned int pos, struct cheat * thecheat);
	void (*cheat_remove)(struct minircheats_model * This, unsigned int pos);
	
	//This one sorts the cheats in the same order as the addresses show up in the cheat search.
	void (*cheat_sort)(struct minircheats_model * This);
	
	//This one disables all cheat codes.
	//void (*cheat_set_enabled)(struct minircheats_model * This, bool enabled);
	//This one makes all cheat codes take effect. Call it each frame.
	void (*cheat_apply)(struct minircheats_model * This);
	
	//The returned code is invalidated on the next call to code_create.
	const char * (*code_create)(struct minircheats_model * This, struct cheat * thecheat);
	//On failure, the contents of thecheat is undefined.
	bool (*code_parse)(struct minircheats_model * This, const char * code, struct cheat * thecheat);
	
	void (*free)(struct minircheats_model * This);
};
struct minircheats_model * minircheats_create_model();

//This is a very high-level object; not counting the libretro core, it takes more input directly from the user than from this object.
struct minircheats {
	void (*set_parent)(struct minircheats * This, struct window * parent);
	//It is allowed to set the core to NULL, and all operations are safe if the core is NULL.
	//However, if the core has been deleted, set_core or delete must be called (possibly with NULL) before any other function, and before window_run_*.
	void (*set_core)(struct minircheats * This, struct libretro * core, size_t prev_limit);
	
	void (*show_search)(struct minircheats * This);
	void (*show_list)(struct minircheats * This);
	
	//This one disables all cheat codes. Does, however, not disable RAM watch.
	void (*set_enabled)(struct minircheats * This, bool enable);
	bool (*get_enabled)(struct minircheats * This);
	
	//This one should be called every frame, as soon as possible after libretro->run(). This applies even if disabled.
	//ramwatch tells whether RAM watch should be updated (both cheat search and ram watch).
	void (*update)(struct minircheats * This, bool ramwatch);
	
	unsigned int (*get_cheat_count)(struct minircheats * This);
	//The returned pointer is valid until the next get_cheat() or free().
	const char * (*get_cheat)(struct minircheats * This, unsigned int id);
	void (*set_cheat)(struct minircheats * This, unsigned int id, const char * code);
	
	void (*free)(struct minircheats * This);
};
struct minircheats * minircheats_create();
