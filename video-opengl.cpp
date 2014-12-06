#define e printf("%i:%i\n",__LINE__,gl.GetError());

//#define GL_GLEXT_PROTOTYPES
#include "io.h"
#ifdef VIDEO_OPENGL

#undef bind
#ifdef _MSC_VER
//MSVC's gl.h doesn't seem to include the stuff it should. Copying these five lines from mingw's gl.h...
# if !(defined(WINGDIAPI) && defined(APIENTRY))
#  include <windows.h>
# else
#  include <stddef.h>
# endif
//Also disable a block of code that defines int32_t to something not identical to my msvc-compatible stdint.h.
# define GLEXT_64_TYPES_DEFINED
#endif

#include <GL/gl.h>
#include <GL/glext.h>
#ifdef WNDPROT_WINDOWS
# include <GL/wglext.h>
#endif
#ifdef WNDPROT_X11
# include <dlfcn.h>
# include <GL/glx.h>
#endif
#define bind BIND_CB

#include <stdio.h>
#include "libretro.h"

#define ONLY_WINDOWS(x)
#define ONLY_X11(x)

#ifdef WNDPROT_WINDOWS
#undef ONLY_WINDOWS
#define ONLY_WINDOWS(x) x
#endif
#ifdef WNDPROT_X11
#undef ONLY_X11
#define ONLY_X11(x) x
#endif

//TODO: pixel buffer objects seem fun

#define ENABLE_DEBUG 2//0 = no, 2 = yes, 1 = if the core asks for it
#if ENABLE_DEBUG==0
#define DO_DEBUG false
#elif ENABLE_DEBUG==2
#define DO_DEBUG true
#else
#define DO_DEBUG (this->is3d && this->in3.debug_context)
#endif

namespace {
#ifdef WNDPROT_WINDOWS
#define WGL_SYM(ret, name, args) WGL_SYM_N("wgl"#name, ret, name, args)
#define WGL_SYM_ANON(ret, name, args) WGL_SYM_N(#name, ret, name, args)
#define WGL_SYMS() \
	WGL_SYM(HGLRC, CreateContext, (HDC hdc)) \
	WGL_SYM(BOOL, DeleteContext, (HGLRC hglrc)) \
	WGL_SYM(HGLRC, GetCurrentContext, ()) \
	WGL_SYM(PROC, GetProcAddress, (LPCSTR lpszProc)) \
	WGL_SYM(BOOL, MakeCurrent, (HDC hdc, HGLRC hglrc)) \
	/*WGL_SYM(BOOL, SwapBuffers, (HDC hdc))*/ \
	
//WINGDIAPI BOOL  WINAPI wglCopyContext(HGLRC, HGLRC, UINT);
//WINGDIAPI HGLRC WINAPI wglCreateContext(HDC);
//WINGDIAPI HGLRC WINAPI wglCreateLayerContext(HDC, int);
//WINGDIAPI BOOL  WINAPI wglDeleteContext(HGLRC);
//WINGDIAPI HGLRC WINAPI wglGetCurrentContext(VOID);
//WINGDIAPI HDC   WINAPI wglGetCurrentDC(VOID);
//WINGDIAPI PROC  WINAPI wglGetProcAddress(LPCSTR);
//WINGDIAPI BOOL  WINAPI wglMakeCurrent(HDC, HGLRC);
//WINGDIAPI BOOL  WINAPI wglShareLists(HGLRC, HGLRC);
//WINGDIAPI BOOL  WINAPI wglUseFontBitmapsA(HDC, DWORD, DWORD, DWORD);
//WINGDIAPI BOOL  WINAPI wglUseFontBitmapsW(HDC, DWORD, DWORD, DWORD);
//#ifdef UNICODE
//#define wglUseFontBitmaps  wglUseFontBitmapsW
//#else
//#define wglUseFontBitmaps  wglUseFontBitmapsA
//#endif // !UNICODE
//WINGDIAPI BOOL  WINAPI SwapBuffers(HDC);

#define WGL_SYM_N(str, ret, name, args) ret (WINAPI * name) args;
struct { WGL_SYMS() HMODULE lib; } static wgl;
#undef WGL_SYM_N
#define WGL_SYM_N(str, ret, name, args) str,
const char * const wgl_names[]={ WGL_SYMS() };
#undef WGL_SYM_N

bool InitGlobalGLFunctions()
{
	//this can yield multiple unsynchronized writers to global variables
	//however, this is safe, because they all write the same values in the same order.
	wgl.lib=LoadLibrary("opengl32.dll");
	if (!wgl.lib) return false;
	
	//HMODULE gdilib=GetModuleHandle("gdi32.dll");
	
	funcptr* functions=(funcptr*)&wgl;
	for (unsigned int i=0;i<sizeof(wgl_names)/sizeof(*wgl_names);i++)
	{
		functions[i]=(funcptr)GetProcAddress(wgl.lib, wgl_names[i]);
		//if (!functions[i]) functions[i]=(funcptr)GetProcAddress(gdilib, wgl_names[i]);
		if (!functions[i]) return false;
	}
	return true;
}
#endif


#ifdef WNDPROT_X11
#define GLX_SYM(ret, name, args) GLX_SYM_N("glX"#name, ret, name, args)
#define GLX_SYM_OPT(ret, name, args) GLX_SYM_N_OPT("glX"#name, ret, name, args)
#define GLX_SYM_ARB(ret, name, args) GLX_SYM_N("glX"#name"ARB", ret, name, args)
#define GLX_SYM_ARB_OPT(ret, name, args) GLX_SYM_N_OPT("glX"#name"ARB", ret, name, args)
#define GLX_SYMS() \
	/* GLX 1.0 */ \
	GLX_SYM(funcptr, GetProcAddress, (const GLubyte * procName)) \
	GLX_SYM(void, SwapBuffers, (Display* dpy, GLXDrawable drawable)) \
	GLX_SYM(Bool, MakeCurrent, (Display* dpy, GLXDrawable drawable, GLXContext ctx)) \
	GLX_SYM(Bool, QueryVersion, (Display* dpy, int* major, int* minor)) \
	GLX_SYM(XVisualInfo*, ChooseVisual, (Display* dpy, int screen, int * attribList)) \
	GLX_SYM(GLXContext, CreateContext, (Display* dpy, XVisualInfo* vis, GLXContext shareList, Bool direct)) \
	/* GLX 1.3 */ \
	GLX_SYM_OPT(GLXFBConfig*, ChooseFBConfig, (Display* dpy, int screen, const int * attrib_list, int * nelements)) \
	GLX_SYM_OPT(XVisualInfo*, GetVisualFromFBConfig, (Display* dpy, GLXFBConfig config)) \
	GLX_SYM_OPT(GLXWindow, CreateWindow, (Display* dpy, GLXFBConfig config, Window win, const int * attrib_list)) \
	GLX_SYM_OPT(GLXContext, CreateNewContext, (Display* dpy, GLXFBConfig config, int render_type, GLXContext share_list, Bool direct)) \
	GLX_SYM_OPT(void, DestroyWindow, (Display* dpy, GLXWindow win)) \

#define GLX_SYM_N_OPT GLX_SYM_N
#define GLX_SYM_N(str, ret, name, args) ret (*name) args;
struct { GLX_SYMS() void* lib; } static glx;
#undef GLX_SYM_N
#define GLX_SYM_N(str, ret, name, args) str,
const char * const glx_names[]={ GLX_SYMS() };
#undef GLX_SYM_N
#undef GLX_SYM_N_OPT

#define GLX_SYM_N(str, ret, name, args) 0,
#define GLX_SYM_N_OPT(str, ret, name, args) 1,
const uint8_t glx_opts[]={ GLX_SYMS() };
#undef GLX_SYM_N
#undef GLX_SYM_N_OPT

bool InitGlobalGLFunctions()
{
	glx.lib=dlopen("libGL.so", RTLD_LAZY);
	if (!glx.lib) return false;
	
	funcptr* functions=(funcptr*)&glx;
	for (unsigned int i=0;i<sizeof(glx_names)/sizeof(*glx_names);i++)
	{
		functions[i]=(funcptr)dlsym(glx.lib, glx_names[i]);
		if (!glx_opts[i] && !functions[i]) return false;
	}
	return true;
}
#endif

void DeinitGlobalGLFunctions()
{
#ifdef WNDPROT_WINDOWS
	FreeLibrary(wgl.lib);
#endif
#ifdef WNDPROT_X11
	dlclose(glx.lib);
#endif
}

//valid transistions:
//2d -> memory
//memory -> texture
//3d -> fbo
//fbo -> texture
//texture -> fbo [takes shader]
//texture -> memory
//fbo -> out
//memory -> chain

//2d -> out
 //2d -> memory -> texture -> [default shader] -> fbo -> out
//3d -> shaders -> chain
 //3d -> fbo -> texture -> [shaders] -> fbo -> texture -> memory
//2d -> chain
 //2d -> memory -> texture -> [default shader] -> fbo -> texture -> memory

#define GL_SYM(ret, name, args) GL_SYM_N("gl"#name, ret, name, args)
#define GL_SYM_OPT(ret, name, args) GL_SYM_N_OPT("gl"#name, ret, name, args)
#define GL_SYM_ARB(ret, name, args) GL_SYM_N("gl"#name"ARB", ret, name, args)
#define GL_SYM_OPT_ARB(ret, name, args) GL_SYM_N_OPT("gl"#name"ARB", ret, name, args)
#define GL_SYMS() \
	GL_SYM(void, BindTexture, (GLenum target, GLuint texture)) \
	GL_SYM(void, Clear, (GLbitfield mask)) \
	GL_SYM(void, ClearColor, (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)) \
	GL_SYM(void, GenTextures, (GLsizei n, GLuint* textures)) \
	GL_SYM(GLenum, GetError, ()) \
	GL_SYM(void, Finish, ()) \
	GL_SYM(void, PixelStorei, (GLenum pname, GLint param)) \
	GL_SYM(void, TexImage2D, (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, \
	                          GLint border, GLenum format, GLenum type, const GLvoid* pixels)) \
	GL_SYM(void, TexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, \
	                             GLenum format, GLenum type, const GLvoid* pixels)) \
	GL_SYM(void, ReadPixels, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid* pixels)) \
	ONLY_WINDOWS(GL_SYM_N_OPT("wglSwapIntervalEXT", BOOL, SwapInterval, (int interval))) \
	ONLY_X11(GL_SYM_N_OPT("glXSwapIntervalMESA", int, SwapIntervalMESA, (unsigned int interval))) \
	ONLY_X11(GL_SYM_N_OPT("glXSwapIntervalSGI", int, SwapIntervalSGI, (int interval))) \
	ONLY_X11(GL_SYM_N_OPT("glXSwapIntervalEXT", void, SwapIntervalEXT, (Display* dpy, GLXDrawable drawable, int interval))) \
	\
GL_SYM(void, BindBuffer, (GLenum target, GLuint buffer)) \
GL_SYM(void, GenBuffers, (GLsizei n, GLuint * buffers)) \
GL_SYM(void, BufferData, (GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage)) \
\
GL_SYM(void, DrawArrays, (GLenum mode, GLint first, GLsizei count)) \
GL_SYM(void, VertexAttribPointer, (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer)) \
\
GL_SYM(void, EnableVertexAttribArray, (GLuint index)) \
GL_SYM(void, DisableVertexAttribArray, (GLuint index)) \
\
GL_SYM(void, Viewport, (GLint x,GLint y,GLsizei width,GLsizei height)) \
\
GL_SYM(GLuint, CreateProgram, ()) \
GL_SYM(GLuint, CreateShader, (GLenum type)) \
GL_SYM(void, ShaderSource, (GLuint shader, GLsizei count, const GLchar * const * string, const GLint * length)) \
GL_SYM(void, CompileShader, (GLuint shader)) \
GL_SYM(void, AttachShader, (GLuint program, GLuint shader)) \
GL_SYM(void, LinkProgram, (GLuint program)) \
GL_SYM(void, GetProgramiv, (GLuint program, GLenum pname, GLint* params)) \
GL_SYM(void, GetProgramInfoLog, (GLuint program, GLsizei bufSize, GLsizei* length, GLchar * infoLog)) \
GL_SYM(void, GetShaderiv, (GLuint shader, GLenum pname, GLint* params)) \
GL_SYM(void, GetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar * infoLog)) \
GL_SYM(void, UseProgram, (GLuint program)) \
GL_SYM(GLint, GetAttribLocation, (GLuint program, const GLchar * name)) \
GL_SYM(GLint, GetUniformLocation, (GLuint program, const GLchar * name)) \
GL_SYM(void, DeleteProgram, (GLuint program)) \
GL_SYM(void, DeleteShader, (GLuint shader)) \
\
GL_SYM(void, ActiveTexture, (GLenum texture)) \
GL_SYM(void, Uniform1i, (GLint location, GLint v0)) \
GL_SYM(void, Enable, (GLenum cap)) \
GL_SYM(void, TexParameteri, (GLenum target, GLenum pname, GLint param)) \
\
GL_SYM(void, GenRenderbuffers, (GLsizei n, GLuint * renderbuffers)) \
GL_SYM(void, BindRenderbuffer, (GLenum target, GLuint renderbuffer)) \
GL_SYM(void, RenderbufferStorage, (GLenum target, GLenum internalformat, GLsizei width, GLsizei height)) \
GL_SYM(void, FramebufferRenderbuffer, (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)) \
GL_SYM(void, DeleteRenderbuffers, (GLsizei n, const GLuint * renderbuffers)) \
\
GL_SYM(void, GenFramebuffers, (GLsizei n, const GLuint * framebuffers)) \
GL_SYM(void, DeleteFramebuffers, (GLsizei n, const GLuint * framebuffers)) \
GL_SYM(void, DeleteTextures, (GLsizei n, const GLuint * textures)) \
GL_SYM(void, FramebufferTexture2D, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)) \
\
GL_SYM_OPT_ARB(void, DebugMessageCallback, (GLDEBUGPROC callback, const void * userParam)) \
GL_SYM_OPT_ARB(void, DebugMessageControl, (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint * ids, GLboolean enabled)) \
GL_SYM(void, BindFramebuffer, (GLenum target, GLuint framebuffer)) \
\
GL_SYM(void, Uniform4f, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)) \
GL_SYM(void, UniformMatrix4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat * value)) \




#define GL_SYM_N_OPT GL_SYM_N
#define GL_SYM_N(str, ret, name, args) ret (APIENTRY * name) args;
struct glsyms { GL_SYMS() };
#undef GL_SYM_N
#define GL_SYM_N(str, ret, name, args) str,
static const char * const gl_names[] = { GL_SYMS() };
#undef GL_SYM_N
#undef GL_SYM_N_OPT
#define GL_SYM_N(str, ret, name, args) 0,
#define GL_SYM_N_OPT(str, ret, name, args) 1,
static uint8_t gl_opts[] = { GL_SYMS() };
#undef GL_SYM_N
#undef GL_SYM_N_OPT

class video_opengl : public video {
public:
	static const uint32_t max_features =
		f_sshot|f_chain|
#ifndef _WIN32
		f_vsync|
#endif
		(f_3d_base<<RETRO_HW_CONTEXT_OPENGL)|
		(f_3d_base<<RETRO_HW_CONTEXT_OPENGLES2)|
		(f_3d_base<<RETRO_HW_CONTEXT_OPENGL_CORE)|
		(f_3d_base<<RETRO_HW_CONTEXT_OPENGLES3)|
		(f_3d_base<<RETRO_HW_CONTEXT_OPENGLES_VERSION)|
		(f_sh_base<<shader::la_glsl);
	uint32_t features()
	{
		static uint32_t features=0;
		if (features!=0) return features;
		features|=f_sshot|f_chain;
#ifndef _WIN32
		//we could poke DwmIsCompositionEnabled (http://msdn.microsoft.com/en-us/library/windows/desktop/aa969518%28v=vs.85%29.aspx),
		//but it can be assumed always on in Windows 7 and I don't care about the others.
		features|=f_vsync;
#endif
		return features;
	}
	
#ifdef WNDPROT_WINDOWS
	HWND hwnd;
	HDC hdc;
	HGLRC hglrc;
#endif
	
#ifdef WNDPROT_X11
	Display* display;
	
	Window xwindow;
	GLXWindow glxwindow;
	GLXDrawable glxsurface;
	Colormap colormap;
	
	GLXContext context;
#endif
	
	glsyms gl;
	
	bool is3d;
	
	union {
		struct {
			uint8_t in2_bytepp;
			GLenum in2_fmt;
			GLenum in2_type;
		};
		struct {
			retro_hw_render_callback in3;
			GLuint in3_renderbuffer;
		};
	};
	
	unsigned int in_lastwidth;
	unsigned int in_lastheight;
	unsigned int in_texwidth;
	unsigned int in_texheight;
	
	GLuint sh_vertexbuf;
	GLuint sh_vertexbuf_first;
	GLuint sh_texcoordbuf;
	
	//sh_fbo[N] is attached to sh_tex[N]
	//sh_prog[N] renders sh_tex[N] to sh_fbo[N+1]
	//sh_fbo[sh_passes] is the back buffer
	//N can be in the range 0 .. sh_passes-1
	//for 2d input, sh_fbo[0] exists but is unused
	unsigned int sh_passes;
	GLuint * sh_prog;
	GLuint * sh_tex;
	GLuint * sh_fbo;
	GLint sh_vercoordloc;
	GLint sh_texcoordloc;
	
	shader* sh_shader;
	
	video* out_chain;
	void* out_buffer;
	size_t out_bufsize;
	unsigned int out_width;
	unsigned int out_height;
	
	/*private*/ bool load_gl_functions(unsigned int version)
	{
		funcptr* functions=(funcptr*)&gl;
		for (unsigned int i=0;i<sizeof(gl_names)/sizeof(*gl_names);i++)
		{
			functions[i]=this->draw_3d_get_proc_address(gl_names[i]);
			if (!gl_opts[i] && !functions[i]) return false;
		}
		return true;
	}
	
	/*private*/ void begin()
	{
#ifdef WNDPROT_WINDOWS
		if (wgl.GetCurrentContext()) abort();//cannot use two of these from the same thread
		wgl.MakeCurrent(this->hdc, this->hglrc);
#endif
#ifdef WNDPROT_X11
		glx.MakeCurrent(this->display, this->glxsurface, this->context);
#endif
	}
	
	/*private*/ void end()
	{
#ifdef WNDPROT_WINDOWS
		wgl.MakeCurrent(this->hdc, NULL);
#endif
#ifdef WNDPROT_X11
		glx.MakeCurrent(this->display, this->glxsurface, NULL);
#endif
	}
	
#ifdef WNDPROT_WINDOWS
	/*private*/ bool create_context(uintptr_t window, bool gles, unsigned int major, unsigned int minor, bool debug)
	{
		if (gles) return false;//rejected for now
		if (major<2) return false;//reject these (cannot hoist to construct3d because InitGlobalGLFunctions must be called)
		//TODO: clone the hwnd, so I won't set pixel format twice
		//TODO: study if the above is necessary - it returns success twice
		//TODO: also study creating an OpenGL driver then Direct3D on the same window (restore pixel format on destruct?)
		//TODO: we need to handle multiple drivers on the same window - in case of chaining, maybe create a window and never show it?
		
		PIXELFORMATDESCRIPTOR pfd;
		memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
		pfd.nSize=sizeof(PIXELFORMATDESCRIPTOR);
		pfd.nVersion=1;
		pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
		pfd.iPixelType=PFD_TYPE_RGBA;
		pfd.cColorBits=24;
		pfd.cAlphaBits=0;
		pfd.cAccumBits=0;
		pfd.cDepthBits=0;
		pfd.cStencilBits=0;
		pfd.cAuxBuffers=0;
		pfd.iLayerType=PFD_MAIN_PLANE;
		SetPixelFormat(this->hdc, ChoosePixelFormat(this->hdc, &pfd), &pfd);
		this->hglrc=wgl.CreateContext(this->hdc);
		wgl.MakeCurrent(this->hdc, this->hglrc);
		
		if (major*10+minor >= 31)
		{
			HGLRC hglrc_v2=this->hglrc;
			PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribs=
				(PFNWGLCREATECONTEXTATTRIBSARBPROC)wgl.GetProcAddress("wglCreateContextAttribsARB");
			const int attribs[] = {
				WGL_CONTEXT_MAJOR_VERSION_ARB, (int)major,
				WGL_CONTEXT_MINOR_VERSION_ARB, (int)minor,
				//WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
				//https://www.opengl.org/wiki/Core_And_Compatibility_in_Contexts says do not use
			0 };
			const int attribs_debug[] = {
				WGL_CONTEXT_MAJOR_VERSION_ARB, (int)major,
				WGL_CONTEXT_MINOR_VERSION_ARB, (int)minor,
				WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
			0 };
			this->hglrc=wglCreateContextAttribs(this->hdc, /*share*/NULL, debug ? attribs_debug : attribs);
			
			wgl.MakeCurrent(this->hdc, this->hglrc);
			wgl.DeleteContext(hglrc_v2);
		}
		
		return true;
	}
#endif
	
#ifdef WNDPROT_X11
	/*private*/ static Bool XWaitForCreate(Display* d, XEvent* ev, char* arg)
	{
		return (ev->type==MapNotify && ev->xmap.window==(Window)arg);
	}
	
	/*private*/ Window create_x11_window(Display* display, int screen, Window parent, XVisualInfo* vis, Colormap* colormap)
	{
		XSetWindowAttributes attr;
		memset(&attr, 0, sizeof(attr));
		attr.colormap=XCreateColormap(display, (Window)parent/*why is this not a screen*/, vis->visual, AllocNone);
		*colormap=attr.colormap;
		attr.event_mask=StructureNotifyMask;//for MapNotify
		
		Window window=XCreateWindow(display, (Window)parent, 0, 0, 16, 16, 0,
		                            vis->depth, InputOutput, vis->visual, CWColormap|CWEventMask, &attr);
		
		XMapWindow(this->display, window);
		XEvent ignore;
		XPeekIfEvent(this->display, &ignore, this->XWaitForCreate, (char*)window);
		return window;
	}
	
	/*private*/ bool create_context(uintptr_t windowhandle, bool gles, unsigned int major, unsigned int minor, bool debug)
	{
		if (gles) return false;//rejected for now
		
		//this doesn't really belong here, but I don't want to promote it to a class member and I can't add another parameter.
		int screen = window_x11_get_display()->screen;
		
		int glxmajor=0;
		int glxminor=0;
		glx.QueryVersion(this->display, &glxmajor, &glxminor);
		if (glxmajor*10+glxminor < 11) return false;
		
		bool doublebuffer;//TODO: use
		
		if (glxmajor*10+glxminor >= 13)
		{
			static const int attributes[]={
				GLX_DOUBLEBUFFER, True,
				GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
				None };
			
			int numconfig;
			GLXFBConfig* configs=glx.ChooseFBConfig(this->display, screen, attributes, &numconfig);
			doublebuffer=(configs);
			if (!configs) configs=glx.ChooseFBConfig(this->display, screen, attributes+2, &numconfig);
			if (!configs) return false;
			
			this->xwindow=None;
			this->glxwindow=glx.CreateWindow(this->display, configs[0], (Window)windowhandle, NULL);
			this->glxsurface=this->glxwindow;
			
			PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribs =
				(PFNGLXCREATECONTEXTATTRIBSARBPROC)glx.GetProcAddress((const GLubyte *)"glXCreateContextAttribsARB");
			if (glXCreateContextAttribs)
			{
				const int attribs[] = {
					GLX_CONTEXT_MAJOR_VERSION_ARB, (int)major,
					GLX_CONTEXT_MINOR_VERSION_ARB, (int)minor,
					//GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
					//https://www.opengl.org/wiki/Core_And_Compatibility_in_Contexts says do not use
					None };
				const int attribs_debug[] = {
					GLX_CONTEXT_MAJOR_VERSION_ARB, (int)major,
					GLX_CONTEXT_MINOR_VERSION_ARB, (int)minor,
					GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
					None };
				this->context=glXCreateContextAttribs(this->display, configs[0], NULL, True, debug ? attribs_debug : attribs);
			}
			else
			{
				this->context=glx.CreateNewContext(this->display, configs[0], GLX_RGBA_TYPE, NULL, True);
			}
		}
		else
		{
			//if (major*10+minor >= 30) return false;
			
			static const int attributes[] = { GLX_DOUBLEBUFFER, GLX_RGBA, None };
			
			XVisualInfo* vis=glx.ChooseVisual(this->display, screen, (int*)attributes);
			doublebuffer=(vis);//TODO: use
			if (!vis) vis=glx.ChooseVisual(this->display, screen, (int*)attributes+1);
			if (!vis) return false;
			
			this->context=glx.CreateContext(this->display, vis, NULL, True);
			if (!this->context) return false;
			
			this->xwindow = this->create_x11_window(this->display, screen, (Window)windowhandle, vis, &this->colormap);
			this->glxwindow = None;
			this->glxsurface = this->xwindow;
		}
		
		glx.MakeCurrent(this->display, this->glxsurface, this->context);
	}
#endif
	
	/*private*/ bool construct(uintptr_t windowhandle, bool gles, unsigned int major, unsigned int minor, bool debug)
	{
		this->out_buffer=NULL;
		this->out_bufsize=0;
		
		this->sh_passes=0;
		this->sh_prog=NULL;
		this->sh_tex=NULL;
		this->sh_fbo=NULL;
		
#ifdef WNDPROT_X11
		this->display=window_x11_get_display()->display;
		this->xwindow=None;
		this->glxwindow=None;
		this->glxsurface=None;
		this->colormap=None;
#endif
#ifdef WNDPROT_WINDOWS
		this->hwnd=(HWND)windowhandle;
		this->hdc=GetDC(this->hwnd);
		this->hglrc=NULL;
#endif
		
		if (!InitGlobalGLFunctions()) return false;
		if (!create_context(windowhandle, gles, major, minor, debug)) return false;
		if (!load_gl_functions(major*10+minor)) return false;
		
		if (debug && gl.DebugMessageCallback)
		{
			gl.DebugMessageCallback((GLDEBUGPROC)this->debug_cb_s, this);//some headers lack 'const' on the userdata, which throws errors
			gl.DebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
			gl.Enable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
		}
		
		gl.GenBuffers(1, &this->sh_vertexbuf);
		gl.BindBuffer(GL_ARRAY_BUFFER, this->sh_vertexbuf);
		static const GLfloat vertexcoord[] = {
			-1.0f,  1.0f,
			 1.0f,  1.0f,
			-1.0f, -1.0f,
			 1.0f, -1.0f,
		};
		gl.BufferData(GL_ARRAY_BUFFER, sizeof(vertexcoord), vertexcoord, GL_STATIC_DRAW);
		
		gl.GenBuffers(1, &this->sh_vertexbuf_first);
		gl.BindBuffer(GL_ARRAY_BUFFER, this->sh_vertexbuf_first);
		static const GLfloat vertexcoord_flip[] = {
			-1.0f, -1.0f,
			 1.0f, -1.0f,
			-1.0f,  1.0f,
			 1.0f,  1.0f,
		};
		gl.BufferData(GL_ARRAY_BUFFER, sizeof(vertexcoord), (this->is3d && this->in3.bottom_left_origin) ? vertexcoord_flip : vertexcoord, GL_STATIC_DRAW);
		
		gl.GenBuffers(1, &this->sh_texcoordbuf);
		
		this->out_chain=NULL;
		
		return true;
	}
	
	/*private*/ bool construct2d(uintptr_t windowhandle)
	{
		this->is3d=false;
		if (!construct(windowhandle, false, 2,0, (ENABLE_DEBUG==2))) return false;
		
		end();
		return true;
	}
	
	void set_chain(video* next)
	{
		this->out_chain=next;
	}
	
	void initialize()
	{
		begin();
		if (this->is3d) this->in3.context_reset();
		
		this->set_shader(NULL);
	}
	
	/*private*/ unsigned int bitround(unsigned int in)
	{
		in--;
		in|=in>>1;
		in|=in>>2;
		in|=in>>4;
		in|=in>>16;
		in++;
		return in;
	}
	
	void set_source(unsigned int max_width, unsigned int max_height, videoformat depth)
	{
		if (this->is3d) this->set_source_3d(max_width, max_height, depth);
		else this->set_source_2d(max_width, max_height, depth);
	}
	
	/*private*/ void set_source_2d(unsigned int max_width, unsigned int max_height, videoformat depth)
	{
		GLenum glfmt[]={ GL_BGRA, GL_BGRA, GL_RGB };
		this->in2_fmt=glfmt[depth];
		GLenum gltype[]={ GL_UNSIGNED_SHORT_1_5_5_5_REV, GL_UNSIGNED_INT_8_8_8_8_REV, GL_UNSIGNED_SHORT_5_6_5 };
		this->in2_type=gltype[depth];
		unsigned char bytepp[]={2,4,2};
		this->in2_bytepp=bytepp[depth];
		
		gl.BindTexture(GL_TEXTURE_2D, this->sh_tex[0]);
		this->in_texwidth=bitround(max_width);
		this->in_texheight=bitround(max_height);
		//why do I need to use rgba for internal format, it works fine with GL_RGB on the old opengl driver
		gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->in_texwidth, this->in_texheight, 0, this->in2_fmt, this->in2_type, NULL);
	}
	
	//void draw_2d_where(unsigned int width, unsigned int height, void * * data, unsigned int * pitch);
	//TODO: pixel buffer object
	
	void draw_2d(unsigned int width, unsigned int height, const void * data, unsigned int pitch)
	{
		gl.BindTexture(GL_TEXTURE_2D, this->sh_tex[0]);
		gl.PixelStorei(GL_UNPACK_ROW_LENGTH, pitch/this->in2_bytepp);
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, this->in2_fmt, this->in2_type, data);
		
		draw_shared(width, height);
	}
	
	/*private*/ bool construct3d(uintptr_t windowhandle, struct retro_hw_render_callback * desc)
	{
		this->is3d=true;
		this->in3=*desc;
		
		//used elements:
		//context_type - handled
		//context_reset - TODO
		//get_current_framebuffer - handled externally
		//get_proc_address - handled externally
		//depth - handled
		//stencil - handled
		//bottom_left_origin - TODO
		//version_major - handled
		//version_minor - handled
		//cache_context - ignored (treated as always true)
		//context_destroy - TODO
		//debug_context - handled
		bool gles;
		unsigned int major;
		unsigned int minor;
		switch (this->in3.context_type)
		{
			case RETRO_HW_CONTEXT_OPENGL:          gles=false; major=2; minor=0; break;
			case RETRO_HW_CONTEXT_OPENGLES2:        gles=true; major=2; minor=0; break;
			case RETRO_HW_CONTEXT_OPENGL_CORE:     gles=false; major=this->in3.version_major; minor=this->in3.version_minor; break;
			case RETRO_HW_CONTEXT_OPENGLES3:        gles=true; major=3; minor=0; break;
			case RETRO_HW_CONTEXT_OPENGLES_VERSION: gles=true; major=this->in3.version_major; minor=this->in3.version_minor; break;
			default: gles=false; major=0; minor=0;
		}
		bool debug;
		if (ENABLE_DEBUG==0) debug=false;
		if (ENABLE_DEBUG==1) debug=(desc->debug_context);
		if (ENABLE_DEBUG==2) debug=true;
		if (!construct(windowhandle, gles, major, minor, debug)) return false;
		
		this->in3_renderbuffer=0;
		if (this->in3.depth) gl.GenRenderbuffers(1, &this->in3_renderbuffer);
		else if (desc->stencil) return false;
		
		end();
		return true;
	}
	
	/*private*/ void set_source_3d(unsigned int max_width, unsigned int max_height, videoformat depth)
	{
		this->in_texwidth=bitround(max_width);
		this->in_texheight=bitround(max_height);
		
		gl.BindTexture(GL_TEXTURE_2D, this->sh_tex[0]);
		gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, this->in_texwidth, this->in_texheight, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, NULL);
		
		if (this->in3.depth)
		{
			gl.BindRenderbuffer(GL_RENDERBUFFER, this->in3_renderbuffer);
			gl.RenderbufferStorage(GL_RENDERBUFFER, this->in3.stencil ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT16, this->in_texwidth, this->in_texheight);
			gl.BindFramebuffer(GL_FRAMEBUFFER, this->sh_fbo[0]);
			gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, this->in3.stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, this->in3_renderbuffer);
		}
	}
	
	uintptr_t draw_3d_get_current_framebuffer()
	{
		if (!this->sh_fbo) return 0;
//static bool g=0;g=!g;return this->sh_fbo[g];
		return this->sh_fbo[0];
	}
	
	funcptr draw_3d_get_proc_address(const char * sym)
	{
#ifdef WNDPROT_WINDOWS
		funcptr ret=(funcptr)wgl.GetProcAddress(sym);
		if (!ret) ret=(funcptr)GetProcAddress(wgl.lib, sym);
		return ret;
#endif
#ifdef WNDPROT_X11
		return (funcptr)glx.GetProcAddress((GLubyte*)sym);
#endif
	}
	
	void draw_3d(unsigned int width, unsigned int height)
	{
		draw_shared(width, height);
	}
	
	void draw_repeat()
	{
		draw_shared(this->in_lastwidth, this->in_lastheight);
	}
	
	/*private*/ void draw_shared(unsigned int width, unsigned int height)
	{
		gl.BindFramebuffer(GL_FRAMEBUFFER, this->sh_fbo[1]);
		gl.UseProgram(this->sh_prog[0]);
		gl.Viewport(0, 0, this->out_width, this->out_height);
//gl.ClearColor(0.5,0.5,0.5,0.5);
		gl.Clear(GL_COLOR_BUFFER_BIT);
		
		if (width!=this->in_lastwidth || height!=this->in_lastheight)
		{
			//left  = 1/2 / out.width
			//right = width/texwidth - left
			//all the docs say I should aim for the centers of the pixels - but zeroes is what gives the right answers
			//likely cause: coordinate 0,0 is never rendered - the corner pixel is at coordinate 0.01,0.01
			//GLfloat left=0.5f/this->out_width;
			//GLfloat top=0.5f/this->out_height;
			GLfloat left=0;
			GLfloat top=0;
			GLfloat right=(float)width / this->in_texwidth - left;
			GLfloat bottom=(float)height / this->in_texheight - top;
			GLfloat texcoord[] = {
				left, top,
				right, top,
				left, bottom,
				right, bottom,
			};
//printf("coord=%f %f %f %f\n",left,top,right,bottom);
			
			gl.BindBuffer(GL_ARRAY_BUFFER, this->sh_texcoordbuf);
			gl.BufferData(GL_ARRAY_BUFFER, sizeof(texcoord), texcoord, GL_DYNAMIC_DRAW);
			
			this->in_lastwidth=width;
			this->in_lastheight=height;
		}
		
		gl.EnableVertexAttribArray(this->sh_texcoordloc);
		gl.BindBuffer(GL_ARRAY_BUFFER, this->sh_texcoordbuf);
		gl.VertexAttribPointer(this->sh_texcoordloc, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
		
		gl.EnableVertexAttribArray(this->sh_vercoordloc);
		gl.BindBuffer(GL_ARRAY_BUFFER, this->sh_vertexbuf_first);
		gl.VertexAttribPointer(this->sh_vercoordloc, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
		
		gl.ActiveTexture(GL_TEXTURE0);
		gl.BindTexture(GL_TEXTURE_2D, this->sh_tex[0]);
		
		gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		
		if (!this->out_chain)
		{
			gl.Finish();
			//TODO: check if this one improves anything
			//use the flicker test core
#ifdef WNDPROT_WINDOWS
			SwapBuffers(this->hdc);
#endif
#ifdef WNDPROT_X11
			glx.SwapBuffers(this->display, this->glxsurface);
#endif
		}
		else
		{
			//out_chain->draw_2d_where
			//if (null) this->out_buffer
			//gl.ReadPixels
			//out_chain->draw_2d
		}
	}
	
	
	void set_vsync(double fps)
	{
#ifdef WNDPROT_WINDOWS
		if (gl.SwapInterval) gl.SwapInterval(fps ? 1 : 0);
#endif
#ifdef WNDPROT_X11
		if (gl.SwapIntervalSGI) gl.SwapIntervalSGI(fps ? 1 : 0);
		if (gl.SwapIntervalMESA) gl.SwapIntervalMESA(fps ? 1 : 0);
		if (gl.SwapIntervalEXT) gl.SwapIntervalEXT(this->display, this->glxsurface, fps ? 1 : 0);
#endif
	}
	
	
	/*private*/ GLuint createShaderProg(unsigned int version, const char * data)
	{
		if (version==200) version=110;
		if (version==210) version=120;
		if (version==300) version=130;
		if (version==310) version=140;
		if (version==320) version=150;
		
		GLuint program=gl.CreateProgram();
		
		for (unsigned int i=0;i<2;i++)
		{
			GLenum type[]={ GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
			char version_s[sizeof("#version 123\n")];
			sprintf(version_s, "#version %i\n", version);
			const char * defines[]={ "#define VERTEX\n", "#define FRAGMENT\n" };
			const char * shaderdata[3]={ strstr(data, "#version") ? "" : version_s, defines[i], data };
			
			GLuint shader=gl.CreateShader(type[i]);
			gl.ShaderSource(shader, 3, shaderdata, NULL);
			gl.CompileShader(shader);
			
			//gl.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
			GLint errlength=0;
			gl.GetShaderiv(shader, GL_INFO_LOG_LENGTH, &errlength);
			if (errlength>1)
			{
				char errstr[1024];
				gl.GetShaderInfoLog(shader, 1023, NULL, errstr);
				errstr[1023]='\0';
				puts(errstr);
			}
			
			gl.AttachShader(program, shader);
			gl.DeleteShader(shader);
		}
		
		gl.LinkProgram(program);
		
		//gl.GetProgramiv(program, GL_LINK_STATUS, &ok);
		GLint errlength=0;
		gl.GetProgramiv(program, GL_INFO_LOG_LENGTH, &errlength);
		if (errlength>1)
		{
			char errstr[1024];
			gl.GetProgramInfoLog(program, 1023, NULL, errstr);
			errstr[1023]='\0';
			puts(errstr);
		}
		
		return program;
	}
	
	bool set_shader(const shader * sh)
	{
		static const char * defaultshader =
			"varying vec2 tex_coord;\n"
			"#if defined(VERTEX)\n"
				"attribute vec2 TexCoord;\n"
				"attribute vec2 VertexCoord;\n"
				"void main()\n"
				"{\n"
					"gl_Position = vec4(VertexCoord, 0.0, 1.0);\n"
					"tex_coord = TexCoord;\n"
				"}\n"
			"#elif defined(FRAGMENT)\n"
				"uniform sampler2D Texture;\n"
				"void main()\n"
				"{\n"
					"gl_FragColor = texture2D(Texture, tex_coord);\n"
				"}\n"
			"#endif\n";
		
		if (this->sh_prog)
		{
			for (unsigned int i=0;i<this->sh_passes;i++) gl.DeleteProgram(this->sh_prog[i]);
			free(this->sh_prog);
		}
		if (this->sh_tex)
		{
			gl.DeleteTextures(this->sh_passes, this->sh_tex);
			free(this->sh_tex);
		}
		if (this->sh_fbo)
		{
			gl.DeleteFramebuffers(this->sh_passes, this->sh_fbo);
			free(this->sh_fbo);
		}
		
		this->sh_passes=1;//TODO
		this->sh_prog=malloc(sizeof(GLuint)*this->sh_passes);
		
		this->sh_tex=malloc(sizeof(GLuint)*this->sh_passes);
		gl.GenTextures(this->sh_passes, this->sh_tex);
		this->sh_fbo=malloc(sizeof(GLuint)*(this->sh_passes+1));
		gl.GenFramebuffers(this->sh_passes, this->sh_fbo);
		this->sh_fbo[this->sh_passes]=0;
		
		for (unsigned int pass=0;pass<this->sh_passes;pass++)
		{
			GLuint prog=createShaderProg(210, defaultshader);
			gl.UseProgram(prog);
			this->sh_prog[pass]=prog;
			
			//TODO: this is unlikely to work with multipass, need to make those arrays
			this->sh_vercoordloc=gl.GetAttribLocation(prog, "VertexCoord");
			this->sh_texcoordloc=gl.GetAttribLocation(prog, "TexCoord");
			
			//TODO: analyze if these two do anything
			//gl.Uniform4f(gl.GetAttribLocation(prog, "Color"), 1,1,1,1);
			gl.Uniform4f(gl.GetAttribLocation(prog, "Color"), 1,0,0.5,1);
			const float identity4[16]={
				//1,0,0,0,
				//0,1,0,0,
				//0,0,1,0,
				//0,0,0,1,
				1,0.5,0,0,
				0.5,1,0,0,
				0,0,1,0,
				0,0,0,1,
			};
			gl.UniformMatrix4fv(gl.GetAttribLocation(prog, "MVPMatrix"), 1, GL_FALSE, identity4);
			
			gl.BindTexture(GL_TEXTURE_2D, this->sh_tex[pass]);
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			
			GLint texid=gl.GetUniformLocation(prog, "Texture");
			gl.Uniform1i(texid, 0);
			
			if (pass!=0 || this->is3d)
			{
				gl.BindFramebuffer(GL_FRAMEBUFFER, this->sh_fbo[pass]);
				gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->sh_tex[pass], 0);
			}
			if (this->is3d && pass==0)
			{
				gl.BindFramebuffer(GL_FRAMEBUFFER, this->sh_fbo[0]);
				GLenum attachment = (this->in3.stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT);
				gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, this->in3_renderbuffer);
				gl.GetError();//ignore this; if it fails, it's because set_source_3d hasn't been called, and we'll get a proper call to that later.
			}
		}
		
//vertex vec4 COLOR [ = (0,0.5,1,0.8) ] [to be changed to 1,1,1,1 if it works]
//global mat4 MVPMatrix [ = ((1,0,0,0),(0,1,0,0),(0,0,1,0),(0,0,0,1)) ]
		
		return true;
	}
	
	
	void set_dest_size(unsigned int width, unsigned int height)
	{
		this->in_lastwidth=0;
		this->in_lastheight=0;
		this->out_width=width;
		this->out_height=height;
		//gl.BindTexture(GL_TEXTURE_2D, this->sh_tex[0]);
		//if (this->is3d)
		//{
		//	//TODO: Use proper size
		//	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 1024, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, NULL);
		//}
		//else
		//{
		//	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->in_texwidth, this->in_texheight, 0, this->in2_fmt, this->in2_type, NULL);
		//}
#ifdef WNDPROT_X11
		if (this->xwindow) XResizeWindow(this->display, this->xwindow, width, height);
#endif
	}
	
	
	//int get_screenshot(unsigned int * width, unsigned int * height, unsigned int * pitch, unsigned int * depth, void* * data, size_t datasize);
	//int get_screenshot_out(unsigned int * width, unsigned int * height, unsigned int * pitch, unsigned int * depth, void* * data, size_t datasize);
	//void release_screenshot(int ret, void* data);
	
	
	~video_opengl()
	{
		free(this->out_buffer);
		//TODO: destroy various resources
		if (this->is3d && this->in3_renderbuffer) gl.DeleteRenderbuffers(1, &this->in3_renderbuffer);
#ifdef WNDPROT_X11
		if (glx.MakeCurrent) glx.MakeCurrent(this->display, 0, NULL);
		if (this->xwindow) XDestroyWindow(this->display, this->xwindow);
		if (this->glxwindow) glx.DestroyWindow(this->display, this->glxwindow);
		if (this->colormap) XFreeColormap(this->display, this->colormap);
#endif
#ifdef WNDPROT_WINDOWS
		if (wgl.MakeCurrent) wgl.MakeCurrent(this->hdc, NULL);
		if (this->hglrc) wgl.DeleteContext(this->hglrc);
		if (this->hdc) ReleaseDC(this->hwnd, this->hdc);
#endif
		DeinitGlobalGLFunctions();
	}
	
	
#if ENABLE_DEBUG > 0
	/*private*/ static void APIENTRY debug_cb_s(GLenum source, GLenum type, GLuint id, GLenum severity,
	                                GLsizei length, const char * message, const void* userParam)
	{
		((video_opengl*)userParam)->debug_cb(source, type, id, severity, length, message);
	}
	
	/*private*/ void debug_cb(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char * message)
	{
		const char * source_s;
		const char * type_s;
		const char * severity_s;
		
		switch (source)
		{
			case GL_DEBUG_SOURCE_API:             source_s="API"; break;
			case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   source_s="Window system"; break;
			case GL_DEBUG_SOURCE_SHADER_COMPILER: source_s="Shader compiler"; break;
			case GL_DEBUG_SOURCE_THIRD_PARTY:     source_s="3rd party"; break;
			case GL_DEBUG_SOURCE_APPLICATION:     source_s="Application"; break;
			case GL_DEBUG_SOURCE_OTHER:           source_s="Other"; break;
			default:                              source_s="Unknown"; break;
		}
		
		switch (type)
		{
			case GL_DEBUG_TYPE_ERROR:               type_s="Error"; break;
			case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_s="Deprecated behavior"; break;
			case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  type_s="Undefined behavior"; break;
			case GL_DEBUG_TYPE_PORTABILITY:         type_s="Portability"; break;
			case GL_DEBUG_TYPE_PERFORMANCE:         type_s="Performance"; break;
			case GL_DEBUG_TYPE_MARKER:              type_s="Marker"; break;
			case GL_DEBUG_TYPE_PUSH_GROUP:          type_s="Push group"; break;
			case GL_DEBUG_TYPE_POP_GROUP:           type_s="Pop group"; break;
			case GL_DEBUG_TYPE_OTHER:               type_s="Other"; break;
			default:                                type_s="Unknown"; break;
		}
		
		switch (severity)
		{
			case GL_DEBUG_SEVERITY_HIGH:   severity_s="Error"; break;
			case GL_DEBUG_SEVERITY_MEDIUM: severity_s="Warning"; break;
			case GL_DEBUG_SEVERITY_LOW:    severity_s="Notice"; break;
			default:                       severity_s="Unknown"; break;
		}
		
		//this could be sent to a better location, but the video driver isn't supposed to
		// generate messages in the first place, so there's nothing good to do.
		printf("[GL debug: %s from %s about %s: %s]\n", severity_s, source_s, type_s, message);
	}
#endif
};


video* video_create_opengl_2d(uintptr_t windowhandle)
{
	video_opengl* ret=new video_opengl;
	if (!ret->construct2d(windowhandle))
	{
		delete ret;
		return NULL;
	}
	return ret;
}

video* video_create_opengl_3d(uintptr_t windowhandle, struct retro_hw_render_callback * desc)
{
	video_opengl* ret=new video_opengl;
	if (!ret->construct3d(windowhandle, desc))
	{
		delete ret;
		return NULL;
	}
	return ret;
}

}

extern const driver_video video_opengl_desc = { "OpenGL", video_create_opengl_2d, video_create_opengl_3d, video_opengl::max_features };
#else
video* video_create_opengl(uintptr_t windowhandle) { return NULL; }
extern const driver_video video_opengl_desc = { "OpenGL", video_create_opengl, NULL, 0 };
#endif
