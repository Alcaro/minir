//#define _XOPEN_SOURCE 500 //strdup and realpath demands this
#ifndef _GNU_SOURCE
#define _GNU_SOURCE //strdup, realpath, asprintf
#endif
#define _strdup strdup //and windows is being windows as usual
#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <stddef.h>
#include "host.h"

#include "function.h"

#ifdef DEBUG
//Having to include this one all the time annoys me.
#include <stdio.h>
#endif

//#ifndef NO_ANON_UNION_STRUCT//For crappy compilers.
//Mandatory because there's a pile of unions in struct config.
#define UNION_BEGIN union {
#define UNION_END };
#define STRUCT_BEGIN struct {
#define STRUCT_END };
//#else
//#define UNION_BEGIN
//#define UNION_END
//#define STRUCT_BEGIN
//#define STRUCT_END
//#endif

#ifdef __cplusplus
 template<bool cond> class static_assertion { private: enum { val=0 }; };
 template<> class static_assertion<true> { public: enum { val=1 }; };
 #define STATIC_ASSERT(cond, name) (void)(static_assertion<(cond)>::val)
 #define STATIC_ASSERT_GSCOPE(cond, name) extern static_assertion<static_assertion<(cond)>::val> name
#else
 #define STATIC_ASSERT(cond, name) (void)(sizeof(struct { int:-!(cond); }))
 #define STATIC_ASSERT_GSCOPE(cond, name) extern char name[(cond)?1:-1]
#endif
#define STATIC_ASSERT_CAN_EVALUATE(cond, name) STATIC_ASSERT(sizeof(cond), name)
#define STATIC_ASSERT_GSCOPE_CAN_EVALUATE(cond, name) STATIC_ASSERT_GSCOPE(sizeof(cond), name)


#ifdef __cplusplus
class anyptr {
void* data;
public:
template<typename T> anyptr(T* data_) { data=(void*)data_; }
template<typename T> operator T*() { return (T*)data; }
template<typename T> operator const T*() const { return (const T*)data; }
};
#else
typedef void* anyptr;
#endif


#include <stdlib.h> // needed because otherwise I get errors from malloc being redeclared.
anyptr malloc_check(size_t size);
anyptr try_malloc(size_t size);
#define malloc malloc_check
anyptr realloc_check(anyptr ptr, size_t size);
anyptr try_realloc(anyptr ptr, size_t size);
#define realloc realloc_check
anyptr calloc_check(size_t size, size_t count);
anyptr try_calloc(size_t size, size_t count);
#define calloc calloc_check


class nocopy {
protected:
	nocopy() {}
	~nocopy() {}
private:
	nocopy(const nocopy&);
	const nocopy& operator=(const nocopy&);
};


#include <string.h> // strdup
class string {
private:
	char* ptr;
	void set(const char * newstr) { if (newstr) ptr=strdup(newstr); else ptr=NULL; }
public:
	string() : ptr(NULL) {}
	string(const char * newstr) { set(newstr); }
	string(const string& newstr) { set(newstr.ptr); }
	~string() { free(ptr); }
	string& operator=(const char * newstr) { char* prev=ptr; set(newstr); free(prev); return *this; }
	string& operator=(string newstr) { char* tmp=newstr.ptr; newstr.ptr=ptr; ptr=tmp; return *this; } // my sources tell me this can sometimes avoid copying entirely
	operator const char * () { return ptr; }
};


struct driverlist {
	const char * name;
};


#include "window.h"

//If an interface specifies cases where only a subset of the functions may be called, free() is
// always allowed, unless it specifically mentions free().
//However, if any interface defines a callback, free() is banned while inside this callback, while
// other functions are allowed. Other instances of the same interface may be used and freed, and
// other interfaces may be called.
//If an interface defines a function to set some state, and a callback for when this state changes,
// calling that function will not trigger the state callback.
//Unless otherwise specified, an interface may only be used from its owner thread (the creator).
// However, it is safe for any thread to create an interface, including having different threads
// use multiple instances of the same interface.
//Don't depend on any pointer being unique; for example, the None interfaces are static. However,
// even if they are (potentially) non-unique, following the instructed method to free them is safe;
// either they're owned by the one one giving them to you, or their free() handlers are empty, or
// they could even be refcounted.
//If a pointer is valid until anything from a set of functions is called (including if the set
// contains only one listed function), free() will also invalidate that pointer.
//"Implementation" means the implementation of the interfaces; the blocks of code that are called
// when a function is called on an interface.
//"User" means the one using the interfaces. Some interface implementations are users of other
// interfaces; for example, an implementation of libretro is also the user of a a dylib.
//An implementation may, at its sole discretion, choose to define any implementation of undefined
// behaviour. After all, any result, including something well defined, is a valid interpretation of
// undefined behaviour. The user may, of course, not rely on that.
//Any function that starts with an underscore may only be called by the module that implements that
// function. ("Module" is defined as "anything whose compilation is controlled by the same #ifdef,
// or the file implementing an interface, whichever makes sense"; for example, window-win32-* is the
// same module.) The arguments and return values of these private functions may change meaning
// between modules, and the functions are not guaranteed to exist at all. (For example, one of the
// three _window_init_* is redundant.)

//This file, and many other parts of minir, uses a weird mix between Windows- and Linux-style
// filenames and paths. This is intentional; the author prefers Linux-style paths and directory
// structures, but Windows file extensions. .exe is less ambigous than no extension, and Windows'
// insistence on overloading the escape character is irritating. The rest is personal preference.



struct video;
struct audio;
struct inputkb;
struct inputmouse;
struct inputjoy;
struct inputmapper;
struct dylib;
struct mutex;
struct rewindstack;
struct libretro;
struct libretroinput;
typedef void(*funcptr)();



//needed features:
//                    2d input
//       3d input   thread move?          2d input
//             shaders                  thread move?
//   thread move?    direct output      direct output
//separate backend
//thread moves are optional

class video;
struct driver_video {
	const char * name;
	video* create(uintptr_t windowhandle);
	uint32_t features;
};

//The owner thread of this one is the one calling set_input_*, which may be another than the creator.
//Additionally, set_output may be called by any thread, but only before the set_input_*.
struct retro_hw_render_callback;
struct video_shader_param;
class video : nocopy {
public:
	//Returns the features this driver supports. Since video drivers can be chained, the flags are in no particular order.
	enum {
		f_output= 0x4000,//set_output can be called with a uintptr_t. The threading and null drivers don't know what that does.
		f_chain = 0x2000,//set_output can be called with a video*. Not all have anything to offer in a chained configuration.
		f_vsync = 0x1000,//This flag only has effect if the output is a window handle.
		f_shaders=0x0F00,//Each of these bits correspond to 256<<shadertype.
		f_3d    = 0x00FF,//Each of these bits correspond to 1<<retro_hw_context_type.
	};
	virtual uint32_t features() = 0;
	
	//The video chain must be fully constructed (set_output()) before this is done, including the final one with the window handle.
	//set_input must be called only on the first one in the chain; it will call the others.
	//Only one of set_input_2d and set_input_3d can be called, and it must be called only once.
	//The corresponding draw_* must be used.
	virtual void set_input_2d(unsigned int depth, double fps) = 0;
	//Asks where to put the video data for best performance. Returning data=NULL means 'I have no opinion, give me whatever'.
	//If called, the next call to this object must be draw_2d, with the same arguments as draw_2d_where.
	//However, it is allowed to call draw_2d without draw_2d_where.
	virtual void draw_2d_where(unsigned int width, unsigned int height, void * * data, unsigned int * pitch) { *data=NULL; *pitch=0; }
	virtual void draw_2d(unsigned int width, unsigned int height, const void * data, unsigned int pitch) = 0;
	
	//The caller will fill in get_current_framebuffer and get_proc_address with something that calls the below two.
	virtual bool set_input_3d(struct retro_hw_render_callback* input3d) { return false; }
	virtual uintptr_t input_3d_get_current_framebuffer() { return 0; }
	virtual funcptr input_3d_get_proc_address(const char *sym) { return NULL; }
	virtual void draw_3d(unsigned int width, unsigned int height) {}
	
	virtual void draw_repeat() = 0;//This can be called whether this one is configured as 2d or 3d.
	
	enum shadertype {
		sh_glsl,
		sh_cg,
		sh_hlsl,
	};
	//TODO: maybe set each pass separately?
	virtual bool set_shader(shadertype type, const char * filename) { return false; }
	virtual video_shader_param* get_shader_params() { return NULL; }
	virtual void set_shader_param(unsigned int index, double value) {}
	
	//Returns the last input to this object.
	//TODO: Fill in arguments
	virtual bool get_screenshot() { return false; }
	//Returns the last output from this object. If shaders aren't supported or configured, it's same as input.
	virtual bool get_screenshot_after() { return get_screenshot(); }
	
	virtual void set_output(unsigned int screen_width, unsigned int screen_height) {}//Draws to the window it was created with.
	virtual void set_output(video* backend) {}//Chains the video drivers. Chaining passes bitmaps around.
	
	virtual ~video() = 0;
};
inline video::~video(){}
void video_copy_2d(void* dst, size_t dstpitch, void* src, size_t srcpitch, size_t bytes_per_line, uint32_t height);

//This returns everything that's compiled in, but some may have runtime requirements that are not
// met. Try them in order until one works. It is guaranteed that at least one of them can
// successfully be created, but this one may not necessarily be useful.
const char * const * video_supported_backends(uint32_t minfeatures);
video* video_create(const char * backend, uintptr_t windowhandle);

//TODO: D3D11?
//TODO: D2D? Probably not.
#ifdef VIDEO_D3D9
video* video_create_d3d9(uintptr_t windowhandle);
#endif
#ifdef VIDEO_DDRAW
video* video_create_ddraw(uintptr_t windowhandle);
#endif
#ifdef VIDEO_OPENGL
video* video_create_opengl(uintptr_t windowhandle);
#endif
#ifdef VIDEO_GDI
video* video_create_gdi(uintptr_t windowhandle);
#endif
#ifdef VIDEO_XSHM
video* video_create_xshm(uintptr_t windowhandle);
#endif
video* video_create_none(uintptr_t windowhandle);
video* video_create_thread(uintptr_t windowhandle);



struct audio {
	//Plays the given samples. They're interleaved left then right then left then right; one pair is
	// one frame.
	void (*render)(struct audio * This, unsigned int numframes, const int16_t * samples);
	
	//Clears out the sound buffer, silencing the buffer until the next call to render(). It is
	// implementation defined whether what's already in the buffer is played.
	void (*clear)(struct audio * This);
	
	//It is implementation defined whether doing this will drop whatever is currently in the sound buffer.
	void (*set_samplerate)(struct audio * This, double samplerate);
	void (*set_latency)(struct audio * This, double latency);
	
	//Toggles synchronization, that is, whether render() should wait or drop some samples if there's
	// insufficient freespace in the internal sound buffer. Defaults to wait.
	void (*set_sync)(struct audio * This, bool sync);
	
	bool (*has_sync)(struct audio * This);
	
	//Deletes the structure.
	void (*free)(struct audio * This);
};

const char * const * audio_supported_backends();
struct audio * audio_create(const char * backend, uintptr_t windowhandle, double samplerate, double latency);

//Should be used only within audio_create.
#ifdef AUDIO_PULSEAUDIO
struct audio * audio_create_pulseaudio(uintptr_t windowhandle, double samplerate, double latency);
#endif
#ifdef AUDIO_DIRECTSOUND
struct audio * audio_create_directsound(uintptr_t windowhandle, double samplerate, double latency);
#endif
struct audio * audio_create_none(uintptr_t windowhandle, double samplerate, double latency);



class inputkb;
struct driver_inputkb {
	const char * name;
	inputkb* (*create)(uintptr_t windowhandle);
	uint32_t features;
};

//inputkb is a quite low-level structure. You'll have to keep the state yourself.
class inputkb : nocopy {
protected:
	function<void(unsigned int keyboard, int scancode, unsigned int libretrocode, bool down)> key_cb;
	
public:
	//It is safe to set this callback multiple times; the latest one applies. It is also safe to not set it at all, though that makes the structure quite useless.
	//scancode is in the range -1..1023, and libretrocode is in the range 0..RETROK_LAST-1. keyboard is in 0..31.
	//If scancode is -1 or libretrocode is 0, it means that the key does not have any assigned value. (Undefined scancodes are extremely rare, though.)
	//It may repeat the current state.
	void set_kb_cb(function<void(unsigned int keyboard, int scancode, unsigned int libretrocode, bool down)> key_cb) { this->key_cb = key_cb; }
	
	//Returns the features this driver supports. Numerically higher is better. (Some flags contradict each other.)
	enum {
		f_multi    = 0x0080,//Can differ between multiple keyboards.
		f_delta    = 0x0040,//Does not call the callback for unchanged state, except for key repeat events. Improves processing time.
		f_auto     = 0x0020,//poll() is empty, and the callback is called by window_run_*(). Implies f_delta.
		f_direct   = 0x0010,//Does not go through a separate process. Improves latency.
		f_background=0x0008,//Can view input events while the window is not focused. Implies f_auto.
		f_pollable = 0x0004,//refresh() is implemented.
		f_remote   = 0x0002,//Compatible with X11 remoting, or equivalent. Implies !f_direct.
		f_public   = 0x0001,//Does not require elevated privileges to use.
	};
	//virtual uint32_t features() = 0;
	
	//Returns the number of keyboards.
	//virtual unsigned int numkb() { return 1; }
	
	//If f_pollable is set, this calls the callback for all pressed keys.
	//The implementation is allowed to call it for non-pressed keys.
	virtual void refresh() {}
	
	//If f_auto is not set, this calls the callback for all key states that changed since the last poll().
	//The implementation is allowed to call it for unchanged keys.
	virtual void poll() {}
	
	virtual ~inputkb() = 0;
};
inline inputkb::~inputkb(){}

extern const driver_inputkb list_inputkb[];

/*
const char * const * inputkb_supported_backends();
inputkb* inputkb_create(const char * backend, uintptr_t windowhandle);

//It's better to use inputkb_create() and let it pick the right one. Otherwise, you're surrendering platform independence.
//Linux drivers:
#ifdef INPUT_X11
struct inputkb* inputkb_create_x11(uintptr_t windowhandle);
#endif
#ifdef INPUT_X11_XINPUT2
struct inputkb* inputkb_create_xinput2(uintptr_t windowhandle);
#endif
#ifdef INPUT_GDK
struct inputkb* inputkb_create_gdk(uintptr_t windowhandle);
#endif
#ifdef INPUT_UDEV
struct inputkb* inputkb_create_udev(uintptr_t windowhandle);
#endif

//Windows drivers:
#ifdef INPUT_RAWINPUT
struct inputkb* inputkb_create_rawinput(uintptr_t windowhandle);
#endif
#ifdef INPUT_DIRECTINPUT
struct inputkb* inputkb_create_directinput(uintptr_t windowhandle);
#endif

inputkb* inputkb_create_none(uintptr_t windowhandle);
*/

//These translate hardware scancodes or virtual keycodes to libretro cores. Can return RETROK_UNKNOWN.
//Note that "virtual keycode" is platform dependent, and because they're huge on X11, they don't exist at all there.
//void inputkb_translate_init();
unsigned inputkb_translate_scan(unsigned int scancode);
unsigned inputkb_translate_vkey(unsigned int vkey);




struct inputmouse;
struct inputjoy;



struct inputmapper {
	//Asks whether a button is pressed. If oneshot is set, the button is considered released after
	// being held for one frame. If the button is not mapped to anything, it's considered unheld.
	//It is safe to query the same button for both oneshot and non-oneshot.
	bool (*button)(struct inputmapper * This, unsigned int id, bool oneshot);
	
	//Tells which key was pressed (actually released) last. You're responsible for setting it free()
	// when you're done. Includes all relevant shift states.
	//It is undefined whether you'll miss anything if you call poll() twice without calling this. It
	// is undefined what you'll get if you call it twice without a poll() in between. It is undefined
	// which of them is returned if two keys are released at the same time.
	char * (*last)(struct inputmapper * This);
	
	//Sets where to get keyboard input. The inputmapper takes ownership of this item; it can not be used
	// anymore after calling this, not even to free it (the inputmapper takes care of that).
	void (*set_keyboard)(struct inputmapper * This, struct inputkb * in);
	
	//Maps an input descriptor (a string) to an input ID.
	//IDs should be assigned to low numbers (0 is fine); leaving big unused holes is wasteful, though
	// small ones don't matter too much.
	//Input descriptors are human readable and human editable text strings, for example KB1::ShiftR+F1,
	// if the input driver maps them to RetroKeyboard properly; if the driver behaves poorly, it ends
	// up as a mess (for example KB1::x4B+x54).
	//However, no matter how they look, they should not be parsed; they can be shown to the user,
	// stored in a text file, or whatever, but don't chop them apart.
	//Note that while A+B+C is the same as B+A+C, A+C+B is not the same. The last listed key is the
	// actual trigger; the others are treated as 'shift states'. If something is mapped to A+B+C, and
	// the keys are pressed in order A C B, A+B+C will be recorded as held, but it will never actually
	// fire if queried as oneshot.
	//Also note that mapping one input can affect the others; if H is mapped somewhere, L+H will
	// trigger it, but if L+H is mapped to something else, the plain H is no longer triggered by L+H.
	//Since all keys can be shift states in that way, Ctrl, Shift and Alt are not given special treatment.
	//Non-keyboard inputs are not supported right now.
	//The return value is whether it worked; if it didn't, the input slot will be unmapped.
	//A NULL or an empty string will return success and unmap the key.
	bool (*map_key)(struct inputmapper * This, const char * descriptor, unsigned int id);
	
	//Tells the structure to ask the inputraw for updates. Clears all prior oneshot flags.
	void (*poll)(struct inputmapper * This);
	
	void (*free)(struct inputmapper * This);
};
struct inputmapper * inputmapper_create();

//Takes an input descriptor and verifies that it's understandable; if it is, it's normalized into
// the mapper's favourite format, otherwise you get a NULL.
//Free it once you're done.
char * inputmapper_normalize(const char * descriptor);



struct dylib {
	bool (*owned)(struct dylib * This);
	
	void* (*sym_ptr)(struct dylib * This, const char * name);
	funcptr (*sym_func)(struct dylib * This, const char * name);
	
	void (*free)(struct dylib * This);
};
struct dylib * dylib_create(const char * filename);

//Returns ".dll", ".so", ".dylib", or whatever is standard on this OS. The return value is lowercase.
const char * dylib_ext();



//Any data associated with this thread is freed once the thread procedure returns, with the possible exception of the userdata.
//It is safe to malloc() something in one thread and free() it in another.
//It is not safe to call window_run_*() from within another thread than the one entering main().
//A thread is rather heavy; for short-running jobs, use thread_create_short or thread_split.
void thread_create(void(*startpos)(void* userdata), void* userdata);

//Returns the number of threads to create to utilize the system resources optimally.
unsigned int thread_ideal_count();

//This is a simple tool that ensures only one thread is doing a certain action at a given moment.
//It may be used from multiple threads simultaneously.
//Memory barrier are inserted as appropriate. Any memory access done while holding a lock is finished while holding this lock.
//This means that if all access to an object is done exclusively while holding the lock, no further synchronization is needed.
//It is not allowed for a thread to call lock() or try_lock() while holding the lock already. It is not allowed
// for a thread to release the lock unless it holds it. It is not allowed to free() the lock while it's held.
//However, it it allowed to hold multiple locks simultaneously.
//lock() is not guaranteed to yield the CPU if it can't grab the lock. It may be implemented as a busy loop.
//Remember to create all relevant mutexes before creating a thread.
struct mutex {
	void (*lock)(struct mutex * This);
	bool (*try_lock)(struct mutex * This);
	void (*unlock)(struct mutex * This);
	
	void (*free)(struct mutex * This);
};
struct mutex * mutex_create();

//This one lets one thread wake another.
//The conceptual difference between this and a mutex is that while a mutex is intended to protect a
// shared resource from being accessed simultaneously, an event is intended to wait until another
// thread is done with something. A mutex is unlocked on the same thread as it's locked; an event is
// unlocked on a different thread.
//An example would be a producer-consumer scenario; if one thread is producing 200 items per second,
// and another thread processes them at 100 items per second, then there will soon be a lot of
// waiting items. An event allows the consumer to ask the producer to get to work, so it'll spend
// half of its time sleeping, instead of filling the system memory.
//If the consumer is the faster one, the excess signals will be piled up. If you prefer dropping them,
// you can check count() and only signal() if the count is <= 0.
//If two threads wait() on the same event, a random one will return for each call to signal().
//count() is guaranteed to remain within [-128, 127]. If anything would put it outside that, it's undefined behaviour.
//Signalling or waiting multiple times may make count() return intermediate values, if applicable.
struct event {
	void (*signal)(struct event * This);
	void (*wait)(struct event * This);
	//The multi functions are equivalent to calling their associated function 'count' times.
	void (*multisignal)(struct event * This, unsigned int count);
	void (*multiwait)(struct event * This, unsigned int count);
	//This is how many signals are waiting to be wait()ed for. Can be below zero if something is currently waiting for this event.
	//Alternate explaination: Increased for each entry to signal() and decreased for each entry to wait().
	int (*count)(struct event * This);
	
	void (*free)(struct event * This);
};
struct event * event_create();

//Increments or decrements a variable, while guaranteeing atomicity relative to other threads. lock_read() just reads the value.
//Returns the value before changing it.
unsigned int lock_incr(unsigned int * val);
unsigned int lock_decr(unsigned int * val);
unsigned int lock_read(unsigned int * val);

//This one creates 'count' threads, calls startpos() in each of them with 'id' from 0 to 'count'-1, and
// returns once each thread has returned.
//Unlike thread_create, thread_split is expected to be called often, for short-running tasks. The threads may be reused.
//It is safe to use the values 0 and 1. However, you should avoid going above thread_ideal_count().
void thread_split(unsigned int count, void(*work)(unsigned int id, void* userdata), void* userdata);



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




struct libretro_core_option {
	const char * name_internal;
	const char * name_display;
	
	//This one is hackishly calculated by checking whether it's checked if during retro_run.
	bool reset_only;
	
	unsigned int numvalues;
	const char * const * values;
};

enum libretro_memtype { // These IDs are the same as RETRO_MEMORY_*.
	libretromem_sram,
	libretromem_unused1,
	libretromem_wram,
	libretromem_vram
};
struct retro_memory_descriptor;//If you want to use this, include libretro.h.
struct libretro {
	//Any returned pointer is, unless otherwise specified, valid only until the next call to a function here, and freed by this object.
	//Input pointers are, unless otherwise specified, not expected valid after the function returns.
	
	const char * (*name)(struct libretro * This);
	
	//Return value format is { "smc", "sfc", NULL }.
	const char * const * (*supported_extensions)(struct libretro * This, unsigned int * count);
	
	//This one is also without the dot.
	bool (*supports_extension)(struct libretro * This, const char * extension);
	
	//Whether the core supports load_rom(NULL).
	bool (*supports_no_game)(struct libretro * This);
	
	//The interface pointers must be valid during every call to run().
	//It is safe to attach new interfaces without recreating the structure.
	//It is safe to attach new interfaces if the previous ones are destroyed.
	void (*attach_interfaces)(struct libretro * This, struct cvideo * v, struct audio * a, struct libretroinput * i);
	
	//data/datalen or filename can be NULL, but not both unless supports_no_game is true. It is allowed for both to be non-NULL.
	//If load_rom_mem_supported is false, filename must be non-NULL, and data/datalen are unlikely to be used.
	bool (*load_rom)(struct libretro * This, const char * data, size_t datalen, const char * filename);
	bool (*load_rom_mem_supported)(struct libretro * This);
	
	//The following are only valid after a game is loaded.
	
	void (*get_video_settings)(struct libretro * This, unsigned int * width, unsigned int * height, unsigned int * depth, double * fps);
	double (*get_sample_rate)(struct libretro * This);
	
	//The core options will be reported as having changed on a freshly created core,
	// even if there are no options. The flag is cleared by calling this function.
	bool (*get_core_options_changed)(struct libretro * This);
	//The list is terminated by a { NULL, NULL, false, 0, NULL }.
	//The return value is invalidated by run() or free(), whichever comes first.
	const struct libretro_core_option * (*get_core_options)(struct libretro * This, unsigned int * numopts);
	//It is undefined behaviour to set a nonexistent option, or to set an option to a nonexistent value.
	void (*set_core_option)(struct libretro * This, unsigned int option, unsigned int value);
	unsigned int (*get_core_option)(struct libretro * This, unsigned int option);
	
	//You can write to the returned pointer.
	//Will return 0:NULL if the core doesn't know what the given memory type is.
	//(If that happens, you can still read and write the indicated amount to the pointer.)
	void (*get_memory)(struct libretro * This, enum libretro_memtype which, size_t * size, void* * ptr);
	
	const struct retro_memory_descriptor * (*get_memory_info)(struct libretro * This, unsigned int * nummemdesc);
	
	void (*reset)(struct libretro * This);
	
	size_t (*state_size)(struct libretro * This);
	bool (*state_save)(struct libretro * This, void* state, size_t size);
	bool (*state_load)(struct libretro * This, const void* state, size_t size);
	
	void (*run)(struct libretro * This);
	
	void (*free)(struct libretro * This);
};

//The message notification may be called before libretro_create returns. It may even be called if the
// function returns NULL afterwards. It can be NULL, in which case the messages will be discarded.
//It is safe to free this item without loading a ROM.
//Since a libretro core is a singleton, only one libretro structure may exist for each core. For the purpose of the
// previous sentence, loading the dylib through other ways than this function counts as creating a libretro structure.
//If one existed already, 'existed' will be set to true. For success, and for other failures, it's false.
//TODO: userdata in message_cb
struct libretro * libretro_create(const char * corepath, void (*message_cb)(int severity, const char * message), bool * existed);

//Returns whatever libretro cores the system can find. The following locations are to be searched for all dylibs:
//- The directory of the executable
//- All subdirectories of the directory the executable is in (but not subdirectories of those)
//- Any other directory the system feels like including, including system directories
//If the scanned directories can be expected to contain large amounts of non-libretro dylibs, all
// dylibs whose name does not contain "retro" or "core" should be filtered off. For example,
// returning the entire /usr/lib is not appropriate.
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
const char * const * libretro_default_cores();

//Equivalent to libretro_default_cores, but looks near the given path instead.
const char * const * libretro_nearby_cores(const char * rompath);



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



#include "image.h"





struct cvideo {
	//Initializes the video system. It will draw on the windowhandle given during creation, at the given bit depth.
	//The user guarantees that the window is size screen_width*screen_height when This is called, and at
	// every subsequent call to draw(). If the window is resized, reinit() (or free()) must be called again.
	//The bit depths may be 32 (XRGB8888), 16 (RGB565), or 15 (0RGB1555).
	void (*reinit)(struct cvideo * This, unsigned int screen_width, unsigned int screen_height, unsigned int depth, double fps);
	
	//Draws the given data. Size doesn't need to be same as above; if it isn't, nearest neighbor scaling will be used.
	//pitch is how many bytes to go forward to reach the next scanline.
	//If data is NULL, the last frame is redrawn, and other arguments are ignored. It will still wait for vsync.
	void (*draw)(struct cvideo * This, unsigned int width, unsigned int height, const void * data, unsigned int pitch);
	
	//Toggles vsync; that is, whether draw() should wait for vblank before doing its stuff and
	// returning. Defaults to on; does not change on reinit().
	//Returns the previous state, if syncing is possible; otherwise, returns an undefined value.
	bool (*set_sync)(struct cvideo * This, bool sync);
	
	//Whether vsync can be enabled on This item.
	bool (*has_sync)(struct cvideo * This);
	
	//Returns the last frame drawn.
	//If This video driver doesn't support This, or if there is no previous frame, returns 0,0,NULL,0,16.
	bool (*repeat_frame)(struct cvideo * This, unsigned int * width, unsigned int * height,
	                                          const void * * data, unsigned int * pitch, unsigned int * bpp);
	
	//Deletes the structure.
	void (*free)(struct cvideo * This);
};

//This returns everything that's compiled in, but some may have runtime requirements that are not
// met. Try them in order until one works. It is guaranteed that at least one of them can
// successfully be created, but this one may not necessarily be useful.
const char * const * cvideo_supported_backends();
struct cvideo * cvideo_create(const char * backend, uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                            unsigned int depth, double fps);

//TODO: D3D11?
//TODO: D2D? Probably not.
#ifdef VIDEO_D3D9
struct cvideo * cvideo_create_d3d9(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                   unsigned int depth, double fps);
#endif
#ifdef VIDEO_DDRAW
struct cvideo * cvideo_create_ddraw(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                  unsigned int depth, double fps);
#endif
#ifdef VIDEO_OPENGL
struct cvideo * cvideo_create_opengl(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                   unsigned int depth, double fps);
#endif
#ifdef VIDEO_GDI
struct cvideo * cvideo_create_gdi(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                unsigned int depth, double fps);
#endif
#ifdef VIDEO_XSHM
struct cvideo * cvideo_create_xshm(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                 unsigned int depth, double fps);
#endif
struct cvideo * cvideo_create_none(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                 unsigned int depth, double fps);

//It would be cleaner if this one wrapped a struct video, but that'd make it use the video structure
// from another thread than what created it, which isn't really feasible.
struct cvideo * cvideo_create_thread(const char * backend, uintptr_t windowhandle,
                                   unsigned int screen_width, unsigned int screen_height,
                                   unsigned int depth, double fps);



struct inputraw {
uint32_t feat;
	unsigned int (*keyboard_num_keyboards)(struct inputraw * This);
	bool (*keyboard_poll)(struct inputraw * This, unsigned int kb_id, unsigned char * keys);
	void (*free)(struct inputraw * This);
};
struct inputraw * _inputraw_create_xinput2(uintptr_t windowhandle);
void _inputraw_x11_keyboard_create_shared(struct inputraw * This);
unsigned int _inputraw_translate_key(unsigned int keycode);
