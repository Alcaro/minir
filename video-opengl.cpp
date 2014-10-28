#define e printf("%i:%i\n",__LINE__,gl.GetError());

#include "minir.h"
#ifdef VIDEO_OPENGL
#undef bind
#include <GL/gl.h>
#include <GL/glext.h>
#ifdef WNDPROT_WINDOWS
#include <GL/wglext.h>
#endif
#ifdef WNDPROT_X11
#include <dlfcn.h>
#include <GL/glx.h>
#endif
#define bind BIND_CB
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

namespace {
#ifdef WNDPROT_WINDOWS
#define WGL_SYM(ret, name, args) WGL_SYM_N("wgl"#name, ret, name, args)
#define WGL_SYMS() \
	WGL_SYM(HGLRC, CreateContext, (HDC hdc)) \
	WGL_SYM(WINBOOL, DeleteContext, (HGLRC hglrc)) \
	WGL_SYM(HGLRC, GetCurrentContext, ()) \
	WGL_SYM(PROC, GetProcAddress, (LPCSTR lpszProc)) \
	WGL_SYM(WINBOOL, MakeCurrent, (HDC hdc, HGLRC hglrc)) \
	WGL_SYM(WINBOOL, SwapBuffers, (HDC hdc)) \
  //WINGDIAPI WINBOOL WINAPI wglCopyContext(HGLRC,HGLRC,UINT);
  //WINGDIAPI HGLRC WINAPI wglCreateContext(HDC);
  //WINGDIAPI HGLRC WINAPI wglCreateLayerContext(HDC,int);
  //WINGDIAPI WINBOOL WINAPI wglDeleteContext(HGLRC);
  //WINGDIAPI HGLRC WINAPI wglGetCurrentContext(VOID);
  //WINGDIAPI HDC WINAPI wglGetCurrentDC(VOID);
  //WINGDIAPI PROC WINAPI wglGetProcAddress(LPCSTR);
  //WINGDIAPI WINBOOL WINAPI wglMakeCurrent(HDC,HGLRC);
  //WINGDIAPI WINBOOL WINAPI wglShareLists(HGLRC,HGLRC);
  //WINGDIAPI WINBOOL WINAPI wglUseFontBitmapsA(HDC,DWORD,DWORD,DWORD);
  //WINGDIAPI WINBOOL WINAPI wglUseFontBitmapsW(HDC,DWORD,DWORD,DWORD);
  //WINGDIAPI WINBOOL WINAPI SwapBuffers(HDC);

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
	
	funcptr* functions=(funcptr*)&wgl;
	for (unsigned int i=0;i<sizeof(wgl_names)/sizeof(*wgl_names);i++)
	{
		functions[i]=(funcptr)GetProcAddress(wgl.lib, wgl_names[i]);
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
	/* GLX 1.4 */ \
	GLX_SYM_ARB_OPT(GLXContext, CreateContextAttribs, \
	                (Display* dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int * attrib_list)) \

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
#ifdef DYLIB_WINDOWS
	FreeLibrary(wgl.lib);
#endif
#ifdef DYLIB_POSIX
	dlclose(glx.lib);
#endif
}

//shader variables, mandatory:
//vertex vec2 TexCoord [ = VertexCoord]
//vertex vec2 VertexCoord [ = (0,0), (0,1), (1,0), (1,1) ]
//vertex vec4 COLOR [ = (0,0.5,1,0.8) ] [to be changed to 1,1,1,1 if it works]
//global mat4 MVPMatrix [ = ((1,0,0,0),(0,1,0,0),(0,0,1,0),(0,0,0,1)) ]
//global sampler2D Texture
//there are more

//shader variables in action:
//https://github.com/libretro/RetroArch/blob/master/gfx/shader/shader_glsl.c

const char defaultShader[] =
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
	ONLY_X11(GL_SYM_OPT(void, SwapIntervalEXT, (Display* dpy, GLXDrawable drawable, int interval))) \
	ONLY_X11(GL_SYM_OPT(int, SwapIntervalMESA, (unsigned int interval))) \
	ONLY_X11(GL_SYM_OPT(int, SwapIntervalSGI, (int interval))) \
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
GL_SYM(void, TexParameteri, (GLenum target,GLenum pname,GLint param)) \


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
		(1<<RETRO_HW_CONTEXT_OPENGL)|
		(1<<RETRO_HW_CONTEXT_OPENGLES2)|
		(1<<RETRO_HW_CONTEXT_OPENGL_CORE)|
		(1<<RETRO_HW_CONTEXT_OPENGLES3)|
		(1<<RETRO_HW_CONTEXT_OPENGLES_VERSION)|
		(256<<sh_glsl);
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
	
	Window window;
	bool glxwindow;
	Colormap colormap;
	
	GLXContext context;
#endif
	
	glsyms gl;
	
	bool is3d;
	
	union {
		struct {
			//2D input
			//uint8_t in2_bpp;
			uint8_t in2_bytepp;
			GLenum in2_fmt;
			GLenum in2_type;
			GLuint in2_texture;
		};
		struct {
			retro_hw_render_callback in3;
		};
	};
	
	unsigned int in_lastwidth;
	unsigned int in_lastheight;
	unsigned int in_texwidth;
	unsigned int in_texheight;
	
	//GLuint sh_vertexarrayobj;
	GLuint sh_vertexbuf;
	GLuint sh_texcoordbuf;
	//GLuint sh_vertexbuf_flip;
	unsigned int sh_passes;
	GLuint* sh_programs;
	GLuint* sh_tex;
	GLuint* sh_fbo;
	
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
#ifdef WNDPROT_WINDOWS
			functions[i]=(funcptr)wgl.GetProcAddress(gl_names[i]);
			if (!functions[i]) functions[i]=(funcptr)GetProcAddress(wgl.lib, gl_names[i]);
#endif
#ifdef WNDPROT_X11
			functions[i]=(funcptr)glx.GetProcAddress((const GLubyte*)gl_names[i]);
#endif
			if (!gl_opts[i] && !functions[i]) return false;
		}
		return true;
	}
	
//	/*private*/ void begin()
//	{
//#ifdef WNDPROT_WINDOWS
//		if (wgl.GetCurrentContext()) abort();//cannot use two of these from the same thread
//		wgl.MakeCurrent(this->hdc, this->hglrc);
//#endif
//#ifdef WNDPROT_X11
//		glx.MakeCurrent(this->display, this->window, this->context);
//#endif
//	}
//	
//	/*private*/ void end()
//	{
//#ifdef WNDPROT_WINDOWS
//		wgl.MakeCurrent(this->hdc, NULL);
//#endif
//#ifdef WNDPROT_X11
//		glx.MakeCurrent(this->display, 0, NULL);
//#endif
//	}
	
#ifdef WNDPROT_X11
	/*private*/ static Bool XWaitForCreate(Display* d, XEvent* ev, char* arg)
	{
		return (ev->type==MapNotify && ev->xmap.window==(Window)arg);
	}
#endif
	
	/*private*/ bool construct(uintptr_t windowhandle, bool gles, unsigned int major, unsigned int minor)
	{
		this->out_buffer=NULL;
		this->out_bufsize=0;
		
#ifdef WNDPROT_WINDOWS
		if (!InitGlobalGLFunctions()) return false;
		if (gles) return false;//rejected for now
		if (major<2) return false;//reject these (cannot hoist to construct3d because InitGlobalGLFunctions must be called)
		//TODO: clone the hwnd, so I won't set pixel format twice
		//TODO: study if the above is necessary - it returns success twice
		//TODO: also study creating an OpenGL driver then Direct3D on the same window (restore pixel format on destruct?)
		//TODO: we need to handle multiple drivers on the same window - in case of chaining, maybe create a window and never show it?
		this->hwnd=(HWND)windowhandle;
		this->hdc=GetDC(this->hwnd);
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
			this->hglrc=wglCreateContextAttribs(this->hdc, NULL/*share*/, attribs);
			
			wgl.MakeCurrent(this->hdc, this->hglrc);
			wgl.DeleteContext(hglrc_v2);
		}
		
		if (!load_gl_functions(major*10+minor)) return false;
#endif
		
#ifdef WNDPROT_X11
		this->window=None;
		this->colormap=None;
		
		if (!InitGlobalGLFunctions()) return false;
		if (gles) return false;//rejected for now
		if (major<2) return false;//reject these (cannot hoist to construct3d because InitGlobalGLFunctions must be called)
		//TODO: clone the hwnd, so I won't set pixel format twice
		//TODO: study if the above is necessary - it returns success twice
		//TODO: also study creating an OpenGL driver then Direct3D on the same window (restore pixel format on destruct?)
		//TODO: we need to handle multiple drivers on the same window - in case of chaining, maybe create a window and never show it?
		
		this->display = window_x11_get_display()->display;
		int screen = window_x11_get_display()->screen;
		
		int glxmajor=0;
		int glxminor=0;
		glx.QueryVersion(this->display, &glxmajor, &glxminor);
		if (glxmajor*10+glxminor < 11) return false;
		
		bool doublebuffer;//TODO: use
		
		if (glxmajor*10+glxminor >= 13)
		{
			//these exist in 1.3 and higher - if the server claims 1.3, it should damn well be that
			if (!glx.ChooseFBConfig) return false;
			if (!glx.GetVisualFromFBConfig) return false;
			if (!glx.CreateNewContext) return false;
			if (!glx.CreateWindow) return false;
			if (!glx.DestroyWindow) return false;
			
			static const int attributes[]={ GLX_DOUBLEBUFFER, True, None };
			
			int numconfig;
			GLXFBConfig* configs=glx.ChooseFBConfig(this->display, screen, attributes, &numconfig);
			doublebuffer=(configs);
			if (!configs) configs=glx.ChooseFBConfig(this->display, screen, NULL, &numconfig);
			if (!configs) return false;
			
			XVisualInfo* vis=glx.GetVisualFromFBConfig(this->display, configs[0]);
			
			XSetWindowAttributes attr;
			memset(&attr, 0, sizeof(attr));
			attr.colormap=XCreateColormap(this->display, (Window)windowhandle, vis->visual, AllocNone);
			//TODO: free colormap
			attr.event_mask=StructureNotifyMask;//for MapNotify
			//TODO: remove above and see what happens
			
			this->context=glx.CreateNewContext(this->display, configs[0], GLX_RGBA_TYPE, NULL, True);
			this->window=glx.CreateWindow(this->display, configs[0], (Window)windowhandle, NULL);
			this->glxwindow=true;
			
			//XMapWindow(this->display, this->window);
			//XEvent ignore;
			//XPeekIfEvent(this->display, &ignore, this->XWaitForCreate, (char*)this->window);
			
			/*
			GLXContext oldcontext=this->context;
			PFNGLXCREATECONTEXTATTRIBSARBPROC glxCreateContextAttribs=
				(PFNGLXCREATECONTEXTATTRIBSARBPROC)glx.GetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");
			const int attribs[] = {
				GLX_CONTEXT_MAJOR_VERSION_ARB, (int)major,
				GLX_CONTEXT_MINOR_VERSION_ARB, (int)minor,
				//GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB|GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
				//https://www.opengl.org/wiki/Core_And_Compatibility_in_Contexts says do not use
				None };
			this->context=glxCreateContextAttribs(this->display, fbc[0], NULL, True, context_attribs);
			
			glx.MakeCurrent(this->display, this->window, this->context);
			glx.DeleteContext(oldcontext);
			*/
		}
		else
		{
			if (major*10+minor >= 30) return false;
			
			static const int attributes[] = { GLX_DOUBLEBUFFER, GLX_RGBA, None };
			
			XVisualInfo* vis=glx.ChooseVisual(this->display, screen, (int*)attributes);
			doublebuffer=(vis);//TODO: use
			if (!vis) vis=glx.ChooseVisual(this->display, screen, (int*)attributes+1);
			if (!vis) return false;
			
			this->context=glx.CreateContext(this->display, vis, NULL, True);
			if (!this->context) return false;
			
			XSetWindowAttributes attr;
			memset(&attr, 0, sizeof(attr));
			attr.colormap=XCreateColormap(this->display, (Window)windowhandle, vis->visual, AllocNone);
			attr.event_mask=StructureNotifyMask;//for MapNotify
			//TODO: remove above and see what happens
			
			this->window=XCreateWindow(this->display, (Window)windowhandle, 0, 0, 16, 16, 0,
			                           vis->depth, InputOutput, vis->visual, CWColormap|CWEventMask, &attr);
			this->glxwindow=false;
			
			XMapWindow(this->display, this->window);
			XEvent ignore;
			XPeekIfEvent(this->display, &ignore, this->XWaitForCreate, (char*)this->window);
		}
		
		glx.MakeCurrent(this->display, this->window, this->context);
		
		if (!load_gl_functions(major*10+minor)) return false;
#endif
		
		gl.GenBuffers(1, &this->sh_vertexbuf);
		static const GLfloat vertexcoord[] = {
			-1.0f,  1.0f, 0.0f,
			 1.0f,  1.0f, 0.0f,
			-1.0f, -1.0f, 0.0f,
			 1.0f, -1.0f, 0.0f,
		};
		gl.BindBuffer(GL_ARRAY_BUFFER, this->sh_vertexbuf);
		gl.BufferData(GL_ARRAY_BUFFER, sizeof(vertexcoord), vertexcoord, GL_STATIC_DRAW);
		
		gl.GenBuffers(1, &this->sh_texcoordbuf);
		
		this->out_chain=NULL;
		
#ifdef WNDPROT_WINDOWS
		wgl.MakeCurrent(this->hdc, NULL);
#endif
#ifdef WNDPROT_X11
		glx.MakeCurrent(this->display, 0, NULL);
#endif
		
		return true;
	}
	
	/*private*/ bool construct2d(uintptr_t windowhandle)
	{
		this->is3d=false;
		if (!construct(windowhandle, false, 2,0)) return false;
		
		gl.GenTextures(1, &this->in2_texture);
		
		return true;
	}
	
	void set_chain(video* next)
	{
		this->out_chain=next;
	}
	
	void initialize()
	{
#ifdef WNDPROT_WINDOWS
		if (wgl.GetCurrentContext()) abort();//cannot use two of these from the same thread
		wgl.MakeCurrent(this->hdc, this->hglrc);
#endif
#ifdef WNDPROT_X11
		glx.MakeCurrent(this->display, this->window, this->context);
#endif
		if (this->is3d) this->in3.context_reset();
		
		set_shader(sh_glsl, NULL);
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
		GLenum glfmt[]={ GL_RGBA, GL_BGRA, GL_RGB };
		this->in2_fmt=glfmt[depth];
		GLenum gltype[]={ GL_UNSIGNED_SHORT_5_5_5_1, GL_UNSIGNED_INT_8_8_8_8_REV, GL_UNSIGNED_SHORT_5_6_5 };
		this->in2_type=gltype[depth];
		unsigned char bytepp[]={2,4,2};
		this->in2_bytepp=bytepp[depth];
		
		gl.BindTexture(GL_TEXTURE_2D, this->in2_texture);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		this->in_texwidth=bitround(max_width);
		this->in_texheight=bitround(max_height);
		//why do I need to use in2_fmt for internal format, it works fine with GL_RGB on the old opengl driver
		gl.TexImage2D(GL_TEXTURE_2D, 0, this->in2_fmt, this->in_texwidth, this->in_texheight, 0, this->in2_fmt, this->in2_type, NULL);
	}
	
	//void draw_2d_where(unsigned int width, unsigned int height, void * * data, unsigned int * pitch);
	
	void draw_2d(unsigned int width, unsigned int height, const void * data, unsigned int pitch)
	{
		gl.BindTexture(GL_TEXTURE_2D, this->in2_texture);
		gl.PixelStorei(GL_UNPACK_ROW_LENGTH, pitch/this->in2_bytepp);
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, this->in2_fmt, this->in2_type, data);
		
		gl.Clear(GL_COLOR_BUFFER_BIT);
		
		if (width!=this->in_lastwidth || height!=this->in_lastheight)
		{
//left  = 1/2 / out.width
//right = width/texwidth - left
			GLfloat left=0.5f/this->out_width;
			GLfloat top=0.5f/this->out_height;
			GLfloat right=(float)width / this->in_texwidth - left;
			GLfloat bottom=(float)height / this->in_texheight - top;
			GLfloat texcoord[] = {
				left, top,
				right, top,
				left, bottom,
				right, bottom,
			};
			gl.BindBuffer(GL_ARRAY_BUFFER, this->sh_texcoordbuf);
			gl.BufferData(GL_ARRAY_BUFFER, sizeof(texcoord), texcoord, GL_STATIC_DRAW);
			
			this->in_lastwidth=width;
			this->in_lastheight=height;
		}
		
		gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		
		draw_shared();
	}
	
	/*private*/ bool construct3d(uintptr_t windowhandle, struct retro_hw_render_callback * desc)
	{
		this->is3d=true;
		//used elements:
		//context_type - handled
		//context_reset - TODO
		//get_current_framebuffer - handled externally
		//get_proc_address - handled externally
		//depth - TODO
		//stencil - TODO
		//bottom_left_origin - TODO
		//version_major - handled
		//version_minor - handled
		//cache_context - ignored (treated as always true)
		//context_destroy - TODO
		//debug_context - TODO
		bool gles;
		unsigned int major;
		unsigned int minor;
		switch (desc->context_type)
		{
			case RETRO_HW_CONTEXT_OPENGL:          gles=false; major=2; minor=0; break;
			case RETRO_HW_CONTEXT_OPENGLES2:        gles=true; major=2; minor=0; break;
			case RETRO_HW_CONTEXT_OPENGL_CORE:     gles=false; major=desc->version_major; minor=desc->version_minor; break;
			case RETRO_HW_CONTEXT_OPENGLES3:        gles=true; major=3; minor=0; break;
			case RETRO_HW_CONTEXT_OPENGLES_VERSION: gles=true; major=desc->version_major; minor=desc->version_minor; break;
			default: gles=false; major=0; minor=0;
		}
		if (!construct(windowhandle, gles, major, minor)) return false;
		this->in3=*desc;
		
		return true;
	}
	
	uintptr_t draw_3d_get_current_framebuffer()
	{
		return 0;
	}
	
	funcptr draw_3d_get_proc_address(const char * sym)
	{
#ifdef WNDPROT_WINDOWS
		return (funcptr)wgl.GetProcAddress(sym);
#endif
#ifdef WNDPROT_X11
		return (funcptr)glx.GetProcAddress((GLubyte*)sym);
#endif
	}
	
	void draw_3d(unsigned int width, unsigned int height)
	{
		draw_shared();
	}
	
	void draw_repeat()
	{
		draw_shared();
	}
	
	/*private*/ void draw_shared()
	{
		if (!this->out_chain)
		{
			gl.Finish();
			//TODO: check if this one improves anything
			//use the flicker test core
#ifdef WNDPROT_WINDOWS
			wgl.SwapBuffers(this->hdc);
#endif
#ifdef WNDPROT_X11
			glx.SwapBuffers(this->display, this->window);
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
		if(0);
		else if (gl.SwapIntervalEXT) gl.SwapIntervalEXT(this->display, this->window, fps ? 1 : 0);
		else if (gl.SwapIntervalMESA) gl.SwapIntervalMESA(fps ? 1 : 0);
		else if (gl.SwapIntervalSGI) gl.SwapIntervalSGI(fps ? 1 : 0);
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
			char version_s[strlen("#version 123\n\1")];
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
				char errstr[errlength+1];
				gl.GetShaderInfoLog(shader, errlength, NULL, errstr);
				errstr[errlength]='\0';
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
			char errstr[errlength+1];
			gl.GetProgramInfoLog(program, errlength, NULL, errstr);
			errstr[errlength]='\0';
			puts(errstr);
		}
		
		return program;
	}
	
	bool set_shader(shadertype type, const char * filename)
	{
		GLuint prog=createShaderProg(210, defaultShader);
		
		gl.UseProgram(prog);
		
		GLint vertexloc=gl.GetAttribLocation(prog, "VertexCoord");
		gl.EnableVertexAttribArray(vertexloc);
		gl.BindBuffer(GL_ARRAY_BUFFER, this->sh_vertexbuf);
		gl.VertexAttribPointer(vertexloc, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		
		GLint texcoordloc=gl.GetAttribLocation(prog, "TexCoord");
		gl.EnableVertexAttribArray(texcoordloc);
		gl.BindBuffer(GL_ARRAY_BUFFER, this->sh_texcoordbuf);
		gl.VertexAttribPointer(texcoordloc, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
		
		GLint texid=gl.GetUniformLocation(prog, "Texture");
		gl.ActiveTexture(GL_TEXTURE0);
//		gl.Enable(GL_TEXTURE_2D);//TODO: nuke
		gl.BindTexture(GL_TEXTURE_2D, this->in2_texture);
		gl.Uniform1i(texid, 0);
		
//vertex vec2 TexCoord [ = VertexCoord]
//vertex vec2 VertexCoord [ = (0,0), (0,1), (1,0), (1,1) ]
//vertex vec4 COLOR [ = (0,0.5,1,0.8) ] [to be changed to 1,1,1,1 if it works]
//global mat4 MVPMatrix [ = ((1,0,0,0),(0,1,0,0),(0,0,1,0),(0,0,0,1)) ]
//global sampler2D Texture
		
		return true;
	}
	
	video_shader_param* get_shader_params()
	{
		return NULL;
	}
	
	void set_shader_param(unsigned int index, double value)
	{
	}
	
	
	//TODO: fill in this
	void set_dest_size(unsigned int width, unsigned int height)
	{
		gl.Viewport(0, 0, width, height);
		this->in_lastwidth=0;
		this->in_lastheight=0;
		this->out_width=width;
		this->out_height=height;
		if (this->window && !this->glxwindow)
		{
      XResizeWindow(this->display, this->window, width, height);
		}
	}
	
	
	//int get_screenshot(unsigned int * width, unsigned int * height, unsigned int * pitch, unsigned int * depth, void* * data, size_t datasize);
	//int get_screenshot_out(unsigned int * width, unsigned int * height, unsigned int * pitch, unsigned int * depth, void* * data, size_t datasize);
	//void release_screenshot(int ret, void* data);
	
	
	~video_opengl()
	{
		free(this->out_buffer);
#ifdef WNDPROT_X11
		if (glx.MakeCurrent) glx.MakeCurrent(this->display, 0, NULL);
		if (this->window && !this->glxwindow) XDestroyWindow(this->display, this->window);
		if (this->window && this->glxwindow) glx.DestroyWindow(this->display, this->window);
		if (this->colormap) XFreeColormap(this->display, this->colormap);
#endif
#ifdef WNDPROT_WINDOWS
		//TODO: destroy various resources
		if (wgl.MakeCurrent) wgl.MakeCurrent(this->hdc, NULL);
		if (this->hglrc) wgl.DeleteContext(this->hglrc);
		if (this->hdc) ReleaseDC(this->hwnd, this->hdc);
#endif
		DeinitGlobalGLFunctions();
	}
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
