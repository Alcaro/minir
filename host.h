#if (!defined(WINDOW_WIN32) && !defined(WINDOW_GTK3) && !defined(WINDOW_MINIMAL))
#ifdef __linux__
#define WINDOW_GTK3
#else
#define WINDOW_WIN32
#endif
#endif

#ifdef WINDOW_GTK3
//Which window protocol is in use. Only select one. Some IO drivers work only on some protocols.
//This refers to the deepest underlying API; for example X11, Wayland or Win32.
#define WNDPROT_X11
//#define WNDPROT_WINDOWS

//Which window toolkit to use. Pick exactly one.
//This refers to the top-level API that draws the widgets. Can be Qt, GTK+, Win32, or similar.
//#define WINDOW_GTK3
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
#define INPUT_UDEV
#define INPUT_GDK
//#define INPUT_RAWINPUT
#define INPUT_XINPUT2
#define INPUT_X11
//#define INPUT_DIRECTINPUT

#define INPUTCUR_XRECORD
//#define INPUTCUR_RAWINPUT
//#define INPUTCUR_W32HOOK
#define INPUTCUR_X11
//#define INPUTCUR_WIN32

//#define VIDEO_D3D9
//#define VIDEO_DDRAW
#define VIDEO_OPENGL
//#define VIDEO_GDI
#define VIDEO_XSHM

//#define AUDIO_PULSEAUDIO//defined by the configure script
//#define AUDIO_DIRECTSOUND

//#define NO_ANON_UNION_STRUCT
#define HAVE_ASPRINTF
//#define NO_UNALIGNED_MEM
//#define HAVE_CG_SHADERS
#endif


#ifdef WINDOW_WIN32
//Which window protocol is in use. Only select one. Some IO drivers work only on some protocols.
//#define WNDPROT_X11
#define WNDPROT_WINDOWS

//Which window toolkit to use. Pick exactly one.
//#define WINDOW_GTK3
//#define WINDOW_WIN32

//Which system to use for file paths.
//#define FILEPATH_POSIX
#define FILEPATH_WINDOWS

#define LINEBREAK_CRLF

//How to load dynamic libraries. Pick exactly one.
//#define DYLIB_POSIX
#define DYLIB_WIN32

//#define THREAD_PTHREAD
#define THREAD_WIN32

//Which input/video/audio drivers to enable. Multiple of each kind may be enabled.
//#define INPUT_UDEV
//#define INPUT_GDK
#define INPUT_RAWINPUT
//#define INPUT_XINPUT2
//#define INPUT_X11
#define INPUT_DIRECTINPUT

//#define INPUTCUR_XRECORD
#define INPUTCUR_RAWINPUT
#define INPUTCUR_W32HOOK
//#define INPUTCUR_X11
#define INPUTCUR_WIN32

#define VIDEO_D3D9
#define VIDEO_DDRAW
#define VIDEO_OPENGL
#define VIDEO_GDI
//#define VIDEO_XSHM

//#define AUDIO_PULSEAUDIO
#define AUDIO_DIRECTSOUND

//#define NO_ANON_UNION_STRUCT
#if (__GNUC__*10000 + __GNUC_MINOR__*100 + __GNUC_PATCHLEVEL__)>=40900
#define HAVE_ASPRINTF
#endif
//#define NO_UNALIGNED_MEM
//#define HAVE_CG_SHADERS
#endif

#if __cplusplus >= 201103L
#define HAVE_MOVE_SEMANTICS
#endif

#ifdef __linux__
#define HAVE_ASPRINTF
#endif

#ifdef WINDOW_GTK3
#define NEED_ICON_PNG
#endif

#ifdef WINDOW_WIN32
#define NEED_MANUAL_LAYOUT
#endif
