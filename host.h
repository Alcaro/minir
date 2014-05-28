//This is configured for Windows; the configure script will configure it for Linux.

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

//#define NEED_ICON_PNG

#define NEED_MANUAL_LAYOUT
