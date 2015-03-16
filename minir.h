#pragma once
#include "global.h"
#include "os.h" // lock_incr, decr
#include "string.h" // stringlist

class video;//io.h   (libretro::enable_3d)
class file; //file.h (libretro::load_rom)
struct retro_memory_descriptor; //libretro.h (libretro::get_memory_info, minircheats_model::set_memory)
struct retro_hw_render_callback;//libretro.h (libretro::enable_3d)
struct windowmenu;//window.h (devmgr::device::get_menu)


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




class devmgr : nocopy {
	class impl;
public:
	
	class device;
	
	struct devinfo {
		enum {
			f_primary = 0x0001,//This will become a primary device.
			f_mandatory=0x0002,//The device must be enabled. Used only for the most fundamental devices, like mapper and inputkb.
		};
		
		const char * name;
		
		uint32_t features;
		//char padding[4];
	};
	static const devinfo* const devices[];
	
	enum {
		e_frame   = 0x0001,
		e_savestate=0x0002,
		
		e_video   = 0x0004,
		e_audio   = 0x0008,
		
		e_keyboard= 0x0010,
		e_mouse   = 0x0020,
		e_gamepad = 0x0040,
	};
	
	class event : nocopy {
		friend class devmgr;
		friend class devmgr::impl;
		event* next;
		device* source;
		uint8_t holdcount;
		
		enum {
			bs_normal,
			bs_blocked,//do not dispatch to secondaries/core
			bs_resent, //do not dispatch to primary
		};
		uint8_t blockstate;
		
		//char padding[1];
		
		event();//not implemented
		
	public:
		bool secondary;
		
		enum {
			ty_null,
			
			ty_frame,
			ty_state_save,
			ty_state_load,
			
			ty_video,
			ty_audio,
			
			ty_keyboard,
			ty_mousemove,
			ty_mousebutton,
			ty_gamepad,
			
			ty_button,
		};
		uint8_t type;
		
		class null;
		
		class frame;
		class state_save;
		class state_load;
		
		class video;
		class audio;
		
		class keyboard;
		class mousemove;
		class mousebutton;
		class gamepad;
		
		class button;
		
		//The primary device is allowed to save an event for later, and dispatch it at some point in the future.
		//To do this, call hold(). The event will remain open for dispatching until release().
		//Alternatively, a device may want to keep an event and process it later. For example, video dumping is expensive.
		//Savestate and frame events may not be held.
		void hold() { this->holdcount++; }
		void release() { this->holdcount--; if (!this->holdcount) delete this; }
		
		//TODO: figure out how this interacts with threading
		
		event(uint8_t type) : next(NULL), source(NULL), holdcount(1), blockstate(bs_normal), type(type) {}
		
		virtual ~event() {}
	};
	
	class device : nocopy {
		friend class devmgr::impl;
	public:
		enum devtype {
			//The secondary device is the default type. It doesn't have any special privileges nor obligations.
			//It should be used for everything that doesn't need exclusive control over the core.
			t_secondary,
			
			//The primary device sees all events, and can choose to discard them.
			//It can't see button events, but button events are always generated from another primary event, and that one can be blocked.
			//There can only be one primary device.
			t_primary,
			
			//The core gets all events last. Like the primary device, there can only be one.
			//Exception: It gets savestate events after the primary device does, but before the secondaries.
			t_core
		};
		virtual devtype type() { return t_secondary; }
		
		//This returns a menu which can control the device. Its lifetime is strictly bound to the device; it must be destructed before the device is.
		//TODO: some items just want a single button, not an entire submenu (e.g. screenshot); account for this
		virtual struct windowmenu * get_menu() { return NULL; }
		
		//Returns the configuration options used by a device. It will be added to the tab panel.
		virtual class widget_base * get_cfgpanel() { return NULL; }
		virtual const char * get_cfgname() { return NULL; }
		
	private:
		devmgr* parent;
		
	protected:
		//Most primary events come from the I/O drivers. They're direct instructions from the user.
		//Secondary events come from other devices, like the keyboard->joypad translator. They are dispatched in response to primary events.
		//Any event may be dispatched as a response to a primary event, with the exception that frame events may not be dispatched as response to this.
		//Secondary events may not emit response events, with the exception that the primary device and the core may react to the secondary frame event.
		//This is to guarantee that there can be devices who see the same events as the core.
		void register_events(uint32_t primary, uint32_t secondary) { parent->dev_register_events(this, primary, secondary); }
		
		//The descriptor looks like KB1::Z. The given ID will be given to the event handler. Each device has its own ID namespace.
		//This is built on top of ev_keyboard/etc, but is far easier to use.
		//If 'hold' is true, the event will be dispatched between the two frame events if the button is held, or ignored otherwise.
		//If false, it will be called every time the state changes.
		//Any ID is allowed; each device has its own ID namespace.
		bool register_button(const char * desc, unsigned int id, bool hold) { return parent->dev_register_button(this, desc, id, hold); }
		
		//Asks whether a button is held.
		bool query_button(unsigned int id) { return parent->dev_test_button(this, id); }
		
		//An event is not dispatched to its sender. Events are guaranteed to be processed in the order they're dispatched.
		//While few devices would listen to the same events they emit, the primary device is likely to
		// both listen to and emit secondary frame events, so the same guarantees are given to all devices.
		void dispatch(event* ev) { parent->ev_set_source(ev, this); parent->ev_append(ev); }
		
		//This allows events to be dispatched from a foreign thread.
		//After this returns, the next entry to frame() is guaranteed to process the event.
		//Events are processed between the primary and secondary frame events. Anything else is unspecified.
		//The event must be a primary input event. It should be called from an event dispatched by window_run(),
		// but is allowed to be dispatched in response to something else (for example, a device is allowed to
		// listen to both window_run and primary frame and dispatch the same events from both).
		void dispatch_async(event* ev) { parent->ev_set_source(ev, this); parent->ev_append_async(ev); }
		
		//Blocks the current event. The event may be held and dispatch()ed later.
		//May only be called during an event handler, and only on that event.
		//May only be called if you're the primary device.
		//If you block an input event, button events connected to that input will not fire, even if you're the primary device.
		void reject(event* ev) { ev->blockstate = event::bs_blocked; }
		
	private:
		virtual void attach() = 0;
		virtual void detach() {}
		
		virtual void ev_frame(event::frame* ev) {}
		virtual void ev_state_save(event::state_save* ev) {}
		virtual void ev_state_load(event::state_load* ev) {}
		
		virtual void ev_video(event::video* ev) {}
		virtual void ev_audio(event::audio* ev) {}
		
		virtual void ev_keyboard(event::keyboard* ev) {}
		virtual void ev_mousemove(event::mousemove* ev) {}
		virtual void ev_mousebutton(event::mousebutton* ev) {}
		virtual void ev_gamepad(event::gamepad* ev) {}
		
		virtual void ev_button(event::button* ev) {}
		
	protected:
		virtual ~device() { if (parent) parent->dev_unregister(this); }
	};
	
	//See class device for more information about these.
	class event::null : public event { public: null() : event(ty_null) {} }; // Used internally by the device manager. Devices never see those.
	class event::frame : public event { public: frame() : event(ty_frame) {} };
	class event::state_save : public event {
	public:
		state_save() : event(ty_state_save) {}
		//Any device may declare that it wants an extra piece of data inserted into savestates. This is done through this function.
		//The data is copied and may be deleted once this returns.
		//The name should match the name of the device. The core will use a blank string as name.
		void insert(const char * name, const uint8_t * data, size_t len);
	};
	class event::state_load : public event {
	public:
		state_load() : event(ty_state_load) {}
		//The returned data is owned by the event object and is deleted once that happens.
		//TODO: The core must be able to reject an offered savestate.
		//Does the primary get/dispatch state events, then core dispatches secondary if it accepts the primary?
		const uint8_t* query(const char * name, size_t* len);
	};
	class event::video : public event {
	public:
		//TODO: find a way to avoid
		//- copying this from core-owned memory to front-owned (requires libretro change, but easy for me)
		//- copying this from front-owned to device-owned video memory (requires changing stuff around here)
		//video data can be 90KB even for tiny stuff like GBC, and later consoles only go bigger; avoiding that copy is desirable
		video() : event(ty_video) {}
		unsigned int width;
		unsigned int height;
		const void* data;
		size_t pitch;
		
		~video() { free((void*)data); }
	};
	class event::audio : public event {
	public:
		audio() : event(ty_audio) {}
		//TODO: libretro v2 will require that this gets changed (likely rewritten from scrath), but for now, this is sufficient.
		//There is no remotely plausible change that could force me to change the overlying architecture.
		const int16_t * data;
		size_t frames;
		
		~audio() { free((int16_t*)data); }
	};
	class event::keyboard : public event {
	public:
		keyboard() : event(ty_keyboard) {}
		unsigned int deviceid;//usually 1
		unsigned int scancode;//0..1023
		unsigned int libretrocode;//0..RETROK_LAST
		bool down;
	};
	//TODO: Fill in the mouse events.
	class event::mousemove : public event {};
	class event::mousebutton : public event {};
	class event::gamepad : public event {
	public:
		gamepad() : event(ty_gamepad) {}
		//TODO: libretro v2 will change this too. If I'm lucky, it'll change to exactly this.
		unsigned int device;
		unsigned int button;//RETRO_DEVICE_ID_JOYPAD_*
		bool down;
	};
	class event::button : public event {
	public:
		button() : event(ty_button) {}
		//You can't dispatch a button event manually, nor can you hold it.
		unsigned int id;
		bool down;
	};
	
protected:
	friend class event;
	virtual void ev_append(event* ev) = 0;
	virtual void ev_append_async(event* ev) = 0;
	//virtual void ev_dispatch_sec(event* ev) = 0;
	
	void ev_set_source(event* ev, device* source) { ev->source = source; } // this one is here so it can be inlined
	
	virtual void dev_register_events(device* target, uint32_t primary, uint32_t secondary) = 0;
	virtual bool dev_register_button(device* target, const char * desc, unsigned int id, bool hold) = 0;
	virtual bool dev_test_button(device* target, unsigned int id) = 0;
	virtual void dev_unregister(device* dev) = 0;
	
public:
	//The created savestate is owned by the caller. Send it to free().
	virtual uint8_t * state_save(size_t* size) = 0;
	virtual bool state_load(const uint8_t * data, size_t size) = 0;
	
	virtual void frame() = 0;
	
	virtual bool add_device(device* dev) = 0;
	
	static devmgr* create();
	virtual ~devmgr() {}
	
	class inputmapper {
		class impl;
	protected:
		function<void(unsigned int id, bool down)> callback;
	public:
		void set_cb(function<void(unsigned int id, bool down)> callback) { this->callback=callback; }
		
		//void request_next(function<void(const char * desc)> callback) { this->req_callback=callback; }
		
		//Each input descriptor has an unique ID, known as its slot. 0 is valid.
		//It's fine to set a descriptor slot that's already used; this will remove the old one.
		//It's also fine to set a descriptor to NULL. This is the default for any slot which is not set, and will never fire.
		//If the descriptor is invalid, the slot will be set to NULL, and false will be returned.
		
		//The implementation may limit the maximum number of modifiers on any descriptor. At least 15 modifiers
		// must be supported, but more is allowed. If it goes above that, the descriptor is rejected.
		virtual bool register_button(unsigned int id, const char * desc) = 0;
		//Returns the lowest slot ID where the given number of descriptors can be sequentially added.
		//If called for len=4 and it returns 2, it means that slots 2, 3, 4 and 5 are currently unused.
		//It doesn't actually reserve anything, or otherwise change the state of the object; it just tells the current state.
		
		//The implementation may set an upper bound on the maximum valid slot. All values up to 4095 must work,
		// but going up to SIZE_MAX is not guaranteed. If this is hit, behaviour is undefined.
		virtual unsigned int register_group(unsigned int len) = 0;
		//If you don't want to decide which slot to use, this one will pick an unused slot and tell which it used.
		//If the descriptor is invalid, -1 will be returned, and no slot will change.
		int register_button(const char * desc)
		{
			int slot=register_group(1);
			if (register_button(slot, desc)) return slot;
			else return -1;
		}
		
		enum dev_t {
			dev_unknown,
			dev_kb,
			dev_mouse,
			dev_gamepad,
		};
		enum { mb_left, mb_right, mb_middle, mb_x4, mb_x5 };
		//type is an entry in the dev_ enum.
		//device is which item of this type is relevant. If you have two keyboards, pressing A on both
		// should give different values here. If they're distinguishable, use 1 and higher; if not, use 0.
		//button is the 'common' ID for that device.
		// For keyboard, it's a RETROK_* (not present here, include libretro.h). For mouse, it's the mb_* enum. For gamepads, [TODO]
		//scancode is a hardware-dependent unique ID for that key. If a keyboard has two As, they will
		// have different scancodes. If a key that doesn't map to any RETROK (Mute, for example), the
		// common ID will be 0, and scancode will be something valid. Scancodes are still present for non-keyboards.
		//down is the new state of the button. Duplicate events are fine and will be ignored.
		virtual void event(dev_t type, unsigned int device, unsigned int button, unsigned int scancode, bool down) = 0;
		
		//Returns the state of a button.
		virtual bool query(dev_t type, unsigned int device, unsigned int button, unsigned int scancode) = 0;
		
		//Releases all buttons held on the indicated device type. Can be dev_unknown to reset everything. The callback is not called.
		//This is likely paired with a refresh() on the relevant inputkb/etc, which will call event() and thereby the callback; set it to NULL.
		virtual void reset(dev_t type) = 0;
		
		static inputmapper* create();
		virtual ~inputmapper(){}
	};
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
		f_3d           = 0x0010,//Wants to use a 3D drawing interface. Use get_3d() for more information.
	};
	virtual uint32_t features() = 0;
	
protected:
	function<void(unsigned int width, unsigned int height, void* * data, size_t* pitch)> vid2d_where;
	function<void(unsigned int width, unsigned int height, const void* data, size_t pitch)> vid2d;
	
	function<void(const int16_t* data, size_t frames)> audio;
	
public:
	void set_video(function<void(unsigned int width, unsigned int height, void* * data, size_t* pitch)> vid2d_where,
	               function<void(unsigned int width, unsigned int height, const void* data, size_t pitch)> vid2d)
	{
		this->vid2d_where=vid2d_where;
		this->vid2d=vid2d;
	}
	
	void set_audio(function<void(const int16_t* data, size_t frames)> audio)
	{
		this->audio=audio;
	}
	
	//device is generally 0, sometimes other low integers
	//button is RETRO_DEVICE_ID_JOYPAD_*
	virtual void input_gamepad(unsigned int device, unsigned int button, bool down) = 0;
	
	//TODO
	//virtual void input_kb() = 0;
	//TODO: mouse/etc
	
	//TODO: Rewrite
	//The caller will own the returned object and shall treat it as if it is the attached video driver.
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



/*
 * Turns old SNES/etc games into something mouse-controlled.
 * This is C89, because it's intended to be shared with RetroArch.
 */
//TODO: do this
//needs two different structures - one for control, one for autodetection
//control takes core WRAM, a binary blob of instructions, and instructions on where to move; it tells which buttons to press
//autodetect takes core WRAM, video output, and some savestates; it creates a blob which the controller can use



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



//All functions on this object yield undefined behaviour if datsize is not within 1..4.
//For easier transferability to other projects, this object does not call any other part of minir,
// so you'll have to do a fair bit of stuff by yourself.
enum cheat_compfunc { cht_lt, cht_gt, cht_lte, cht_gte, cht_eq, cht_neq };
enum cheat_chngtype { cht_const, cht_inconly, cht_deconly, cht_once };
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
