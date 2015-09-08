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

//Which input/video/audio drivers to enable. Multiple of each kind may be enabled.
#define INPUT_UDEV
#define INPUT_GDK
//#define INPUT_RAWINPUT
#define INPUT_XINPUT2
#define INPUT_X11
//#define INPUT_DIRECTINPUT

#define INPUTCUR_XRECORD
//#define INPUTCUR_RAWINPUT
#define INPUTCUR_X11
//#define INPUTCUR_WIN32

//#define VIDEO_D3D9
//#define VIDEO_DDRAW
#define VIDEO_OPENGL
//#define VIDEO_GDI
#define VIDEO_XSHM

//#define AUDIO_PULSEAUDIO//defined by the configure script
//#define AUDIO_DIRECTSOUND
//#define AUDIO_WASAPI
#endif


#ifdef WINDOW_WIN32
//Which window protocol is in use. Only select one. Some IO drivers work only on some protocols.
//#define WNDPROT_X11
#define WNDPROT_WINDOWS

//Which window toolkit to use. Pick exactly one.
//#define WINDOW_GTK3
//#define WINDOW_WIN32

//Which input/video/audio drivers to enable. Multiple of each kind may be enabled.
//#define INPUT_UDEV
//#define INPUT_GDK
#define INPUT_RAWINPUT
//#define INPUT_XINPUT2
//#define INPUT_X11
#define INPUT_DIRECTINPUT

//#define INPUTCUR_XRECORD
#define INPUTCUR_RAWINPUT
//#define INPUTCUR_X11
#define INPUTCUR_WIN32

#define VIDEO_D3D9
#define VIDEO_DDRAW
#define VIDEO_OPENGL
#define VIDEO_GDI
//#define VIDEO_XSHM

//#define AUDIO_PULSEAUDIO
#define AUDIO_DIRECTSOUND
#define AUDIO_WASAPI
#endif


#ifdef __APPLE__
#define __unix__ 1 // you're a Unix, OSX, stop pretending not to be. Are you not proud of your heritage?
#error not implemented.
#endif


#ifdef __GNUC__
#define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)
#define LIKELY(expr)    __builtin_expect(!!(expr), true)
#define UNLIKELY(expr)  __builtin_expect(!!(expr), false)
#else
#define GCC_VERSION 0
#define LIKELY(expr)    (expr)
#define UNLIKELY(expr)  (expr)
#endif

#if __cplusplus >= 201103L
#define HAVE_MOVE
#endif

#if defined(__linux__) || GCC_VERSION >= 40900
#define HAVE_ASPRINTF
#endif

#ifdef WINDOW_GTK3
#define NEED_ICON_PNG
#endif

#ifdef WINDOW_WIN32
#define NEED_MANUAL_LAYOUT
#endif
