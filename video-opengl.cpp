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
//#define GLX_SYM_ARB(ret, name, args) GLX_SYM_N("glX"#name"ARB", ret, name, args)
//#define GLX_SYM_ARB_OPT(ret, name, args) GLX_SYM_N_OPT("glX"#name"ARB", ret, name, args)
#define GLX_SYMS() \
	GLX_SYM(funcptr, GetProcAddress, (const GLubyte * procName)) \
	GLX_SYM(void, SwapBuffers, (Display* dpy, GLXDrawable drawable)) \
	GLX_SYM(Bool, MakeCurrent, (Display* dpy, GLXDrawable drawable, GLXContext ctx)) \
	GLX_SYM(Bool, QueryVersion, (Display* dpy, int* major, int* minor)) \
	GLX_SYM(XVisualInfo*, ChooseVisual, (Display* dpy, int screen, int * attribList)) \
	GLX_SYM(GLXContext, CreateContext, (Display* dpy, XVisualInfo* vis, GLXContext shareList, Bool direct)) \
	GLX_SYM_OPT(GLXFBConfig*, ChooseFBConfig, (Display* dpy, int screen, const int * attrib_list, int * nelements)) \
	GLX_SYM_OPT(XVisualInfo*, GetVisualFromFBConfig, (Display* dpy, GLXFBConfig config)) \
	GLX_SYM_OPT(GLXWindow, CreateWindow, (Display* dpy, GLXFBConfig config, Window win, const int * attrib_list)) \
	GLX_SYM_OPT(GLXContext, CreateNewContext, (Display* dpy, GLXFBConfig config, int render_type, GLXContext share_list, Bool direct)) \

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

/*
const char * defaultShader =
"varying vec2 tex_coord;\n"
"#if defined(VERTEX)\n"
    "attribute vec2 TexCoord;\n"
    "attribute vec2 VertexCoord;\n"
    "uniform mat4 MVPMatrix;\n"
    "void main()\n"
    "{\n"
        "gl_Position = MVPMatrix * vec4(VertexCoord, 0.0, 1.0);\n"
        "tex_coord = TexCoord;\n"
    "}\n"
"#elif defined(FRAGMENT)\n"
    "uniform sampler2D Texture;\n"
    "void main()\n"
    "{\n"
        "gl_FragColor = texture2D(Texture, tex_coord);\n"
    "}\n"
"#endif\n";
*/

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
	ONLY_WINDOWS(GL_SYM_OPT(BOOL, SwapInterval, (int interval))) \
	ONLY_X11(GL_SYM_OPT(void, SwapIntervalEXT, (Display* dpy, GLXDrawable drawable, int interval))) \
	ONLY_X11(GL_SYM_OPT(int, SwapIntervalMESA, (unsigned int interval))) \
	ONLY_X11(GL_SYM_OPT(int, SwapIntervalSGI, (int interval))) \

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
			unsigned int in2_width;
			unsigned int in2_height;
			GLuint in2_texture;
		};
		struct {
			retro_hw_render_callback in3;
		};
	};
	
	//GLuint* sh_tex;
	//GLuint* sh_fbo;
	
	video* out_chain;
	
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
/*
#define sym_r(name, str) sym_r_o(name, str); if (!gl.name) return false
#define sym(name) sym_r(name, "gl"#name)
#define sym_o(name) sym_r_o(name, "gl"#name)
#define symARB(name) sym_r(name, "gl"#name"ARB")
#define symver(name, minver) if (version >= minver) { symver(name); } else gl.name=NULL
#define symverARB(name, minver) if (version >= minver) { symverARB(name); } else gl.name=NULL
sym(Clear);
sym(ClearColor);
sym(GetError);
		sym(Finish);
		sym(ReadPixels);
		sym(GenTextures);
		sym(BindTexture);
		sym(TexImage2D);
		sym(TexSubImage2D);
		sym(PixelStorei);
		sym_r(SwapInterval, "wglSwapIntervalEXT");
#undef sym_r_o
#undef sym_r
#undef sym_o
#undef sym
#undef symARB
#undef symver
#undef symverARB
*/
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
	/*private*/ static Bool XWaitForCreate(Display* d, XEvent* e, char* arg)
	{
		return (e->type == MapNotify) && (e->xmap.window == (Window)arg);
	}
#endif
	
	/*private*/ bool construct(uintptr_t windowhandle, bool gles, unsigned int major, unsigned int minor)
	{
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
		pfd.cDepthBits=24;
		pfd.cStencilBits=8;
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
		wgl.MakeCurrent(this->hdc, NULL);
		
		this->out_chain=NULL;
		
		return true;
#endif
		
#ifdef WNDPROT_X11
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
			if (!glx.ChooseFBConfig || !glx.GetVisualFromFBConfig || !glx.CreateNewContext || !glx.CreateWindow) return false;
			
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
			//TODO: free colormap
			attr.event_mask=StructureNotifyMask;//for MapNotify
			//TODO: remove above and see what happens
			
			this->window=XCreateWindow(this->display, (Window)windowhandle, 0, 0, 100, 100, 0,
			                           vis->depth, InputOutput, vis->visual, CWColormap|CWEventMask, &attr);
			
			XMapWindow(this->display, this->window);
			XEvent ignore;
			XPeekIfEvent(this->display, &ignore, this->XWaitForCreate, (char*)this->window);
		}
		
		glx.MakeCurrent(this->display, this->window, this->context);
		
		if (!load_gl_functions(major*10+minor)) return false;
		glx.MakeCurrent(this->display, 0, NULL);
		
		this->out_chain=NULL;
		
		return true;
#endif
	}
	
	/*private*/ bool construct2d(uintptr_t windowhandle)
	{
		this->is3d=false;
		if (!construct(windowhandle, false, 2,0)) return false;
		
		/*
		//GLuint programID = LoadShaders( "SimpleVertexShader.vertexshader", "SimpleFragmentShader.fragmentshader" );
		
		// Get a handle for our buffers
		GLuint vertexPosition_modelspaceID = glGetAttribLocation(programID, "vertexPosition_modelspace");
		
		static const GLfloat g_vertex_buffer_data[] = { 
			-1.0f, -1.0f, 0.0f,
			1.0f, -1.0f, 0.0f,
			0.0f,  1.0f, 0.0f,
		};
		
		GLuint vertexbuffer;
		gl.GenBuffers(1, &vertexbuffer);
		gl.BindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		gl.BufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);
		*/
		
		return true;
	}
	
	void finalize_2d(unsigned int base_width, unsigned int base_height, unsigned int depth)
	{
#ifdef WNDPROT_WINDOWS
		if (wgl.GetCurrentContext()) abort();//cannot use two of these from the same thread
		wgl.MakeCurrent(this->hdc, this->hglrc);
#endif
#ifdef WNDPROT_X11
		glx.MakeCurrent(this->display, this->window, this->context);
#endif
		
		this->in2_width=base_width;
		this->in2_height=base_height;
		
		if (depth==15)
		{
			this->in2_fmt=GL_UNSIGNED_SHORT_5_5_5_1;
			this->in2_bytepp=2;
		}
		if (depth==16)
		{
			this->in2_fmt=GL_UNSIGNED_SHORT_5_6_5;
			this->in2_bytepp=2;
		}
		if (depth==32)
		{
			this->in2_fmt=GL_UNSIGNED_INT_8_8_8_8;
			this->in2_bytepp=4;
		}
		
		gl.GenTextures(1, &this->in2_texture);
		gl.BindTexture(GL_TEXTURE_2D, this->in2_texture);
		gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, base_width, base_height, 0, GL_RGB, this->in2_fmt, NULL);
		gl.BindTexture(GL_TEXTURE_2D, 0);
		
		//end();
	}
	
	//void draw_2d_where(unsigned int width, unsigned int height, void * * data, unsigned int * pitch);
	
	void draw_2d(unsigned int width, unsigned int height, const void * data, unsigned int pitch)
	{
		//begin();
		
		gl.BindTexture(GL_TEXTURE_2D, this->in2_texture);
		gl.PixelStorei(GL_UNPACK_ROW_LENGTH, pitch/this->in2_bytepp);
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, this->in2_fmt, data);
		gl.BindTexture(GL_TEXTURE_2D, 0);
		
gl.ClearColor(0.5f,0.6f,0.7f,0.5f);
gl.Clear(GL_COLOR_BUFFER_BIT);
		
		draw_shared();
		//end();
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
	
	void finalize_3d()
	{
#ifdef WNDPROT_WINDOWS
		wgl.MakeCurrent(this->hdc, this->hglrc);
#endif
#ifdef WNDPROT_X11
		glx.MakeCurrent(this->display, this->window, this->context);
#endif
		this->in3.context_reset();
		//end();
	}
	
	uintptr_t input_3d_get_current_framebuffer()
	{
		//begin();
		return 0;
	}
	
	funcptr input_3d_get_proc_address(const char * sym)
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
		//end();
	}
	
	/*private*/ void draw_shared()
	{
		if (!this->out_chain)
		{
			//gl.Finish();
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
			//gl.ReadPixels
		}
	}
	
	void draw_repeat()
	{
		//begin();
		draw_shared();
		//end();
	}
	
	
	void set_vsync(double fps)
	{
#ifdef WNDPROT_WINDOWS
		if (gl.SwapInterval) gl.SwapInterval(fps ? 1 : 0);
#endif
#ifdef WNDPROT_X11
		if (gl.SwapIntervalEXT) gl.SwapIntervalEXT(this->display, this->window, fps ? 1 : 0);
		if (gl.SwapIntervalMESA) gl.SwapIntervalMESA(fps ? 1 : 0);
		if (gl.SwapIntervalSGI) gl.SwapIntervalSGI(fps ? 1 : 0);
#endif
	}
	
	
	//bool set_shader(shadertype type, const char * filename);
	//video_shader_param* get_shader_params();
	//void set_shader_param(unsigned int index, double value);
	
	
	//TODO: fill in those two
	void get_base_size(unsigned int * width, unsigned int * height)
	{
		*width=this->in2_width;
		*height=this->in2_height;
	}
	
	void set_size(unsigned int width, unsigned int height)
	{
		
	}
	
	
	void set_chain(video* backend)
	{
		this->out_chain=backend;
	}
	
	
	//int get_screenshot(unsigned int * width, unsigned int * height, unsigned int * pitch, unsigned int * depth, void* * data, size_t datasize);
	//int get_screenshot_out(unsigned int * width, unsigned int * height, unsigned int * pitch, unsigned int * depth, void* * data, size_t datasize);
	//void release_screenshot(int ret, void* data);
	
	
	~video_opengl()
	{
#ifdef WNDPROT_X11
		if (glx.MakeCurrent) glx.MakeCurrent(this->display, 0, NULL);
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
#endif

//video* video_create_opengl(uintptr_t windowhandle, unsigned int depth) { return NULL; }
//extern const driver_video video_opengl_desc = { "OpenGL", video_create_opengl, NULL, 0 };
