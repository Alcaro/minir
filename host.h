#ifdef WANT_PHOENIX
#undef HAVE_ASPRINTF
#undef WNDPROT_X11
#undef WNDPROT_WINDOWS
#undef WINDOW_GTK3
#undef WINDOW_WIN32
#error Check FILEPATH.
#error Check LINEBREAK.
#undef DYLIB_POSIX
#undef DYLIB_WIN32
//Phoenix doesn't offer threads.
#define WNDPROT_PHOENIX
#define WINDOW_PHOENIX
#define DYLIB_PHOENIX
#define VIDEO_RUBY
#define AUDIO_RUBY
#define INPUT_RUBY
#endif

#ifdef __linux__
//Which window protocol is in use. Only select one. Some IO drivers work only on some protocols.
//This refers to the deepest underlying API; for example X11, Wayland or Win32.
#define WNDPROT_X11
//#define WNDPROT_WINDOWS

//Which window toolkit to use. Pick exactly one.
//This refers to the top-level API that draws the widgets. Can be Qt, GTK+, Win32, or similar.
#define WINDOW_GTK3
//#define WINDOW_WIN32

//Which system to use for file paths.
#define FILEPATH_POSIX
//#define FILEPATH_WINDOWS

//#define LINEBREAK_CRLF

//How to load dynamic libraries. Pick exactly one.
#define DYLIB_POSIX
//#define DYLIB_WIN32

#define THREAD_PTHREAD
//#define THREAD_WIN32

//Which input/video/audio drivers to enable. Multiple of each kind may be enabled.
#define INPUT_X11_XINPUT2
//#define INPUT_RAWINPUT
#define INPUT_X11
//#define INPUT_DIRECTINPUT

//#define VIDEO_D3D9
#define VIDEO_OPENGL
//#define VIDEO_GDI
#define VIDEO_XSHM

#define AUDIO_PULSEAUDIO
//#define AUDIO_DIRECTSOUND

//#define NO_ANON_UNION_STRUCT
#define HAVE_ASPRINTF
//#define NO_UNALIGNED_MEM
#endif


#ifdef _WIN32
//Which window protocol is in use. Only select one. Some IO drivers work only on some protocols.
//#define WNDPROT_X11
#define WNDPROT_WINDOWS

//Which system to use for file paths.
//#define FILEPATH_POSIX
#define FILEPATH_WINDOWS

#define LINEBREAK_CRLF

//How to load dynamic libraries. Pick exactly one.
//#define DYLIB_POSIX
#define DYLIB_WIN32

//#define THREAD_PTHREAD
#define THREAD_WIN32

//Which window toolkit to use. Pick exactly one.
//#define WINDOW_GTK3
#define WINDOW_WIN32

//Which input/video/audio drivers to enable. Multiple of each kind may be enabled.
//#define INPUT_X11_XINPUT2
#define INPUT_RAWINPUT
//#define INPUT_X11
#define INPUT_DIRECTINPUT

#define VIDEO_D3D9
#define VIDEO_OPENGL
#define VIDEO_GDI
//#define VIDEO_XSHM

//#define AUDIO_PULSEAUDIO
#define AUDIO_DIRECTSOUND

//#define NO_ANON_UNION_STRUCT
//#define HAVE_ASPRINTF
//#define NO_UNALIGNED_MEM
#endif

#ifdef WINDOW_GTK3
#define NEED_ICON_PNG
#endif

#ifdef WINDOW_WIN32
#define NEED_MANUAL_LAYOUT
#endif
