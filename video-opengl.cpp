#include "minir.h"
#ifdef VIDEO_OPENGL
#undef bind
#include <gl/gl.h>
#include <gl/glext.h>
#ifdef _WIN32
#include <gl/wglext.h>
#endif
#define bind BIND_CB
#include "libretro.h"

namespace {
#ifdef _WIN32
struct {
	HMODULE lib;
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
	HGLRC (WINAPI * CreateContext)(HDC hdc);
	WINBOOL (WINAPI * MakeCurrent)(HDC hdc, HGLRC hglrc);
	WINBOOL (WINAPI * DeleteContext)(HGLRC hglrc);
	PROC (WINAPI * GetProcAddress)(LPCSTR lpszProc);
  WINBOOL (WINAPI * SwapBuffers)(HDC);
} static wgl;
#define pgl wgl // platform-specific gl; only GetProcAddress and SwapBuffers allowed from this object, as the others only exist in this form on Windows.
#endif

bool InitGlobalGLFunctions()
{
	//this can yield multiple unsynchronized writers to global variables - however, this is safe, because they all write the same values in the same order.
#ifdef _WIN32
	wgl.lib=LoadLibrary("opengl32.dll");
	if (!wgl.lib) return false;
#define sym_r(name, str) *(funcptr*)&wgl.name = (funcptr)GetProcAddress(wgl.lib, "wgl"#name); if (!wgl.name) return false;
#define sym(name) sym_r(name, "wgl"#name)
#define sym_n(name) sym_r(name, #name)
	sym(CreateContext);
	sym(MakeCurrent);
	sym(DeleteContext);
	sym(GetProcAddress);
	sym_n(SwapBuffers);
#undef sym
#undef sym_r
#undef sym_n
#define WIN32_SYM_FALLBACK(name) if (!gl.name) *(funcptr*)&gl.name = (funcptr)GetProcAddress(wgl.lib, "gl"#name);
#else
#define WIN32_SYM_FALLBACK(name) ;
#define APIENTRY /* */
#endif
	return true;
}

void DeinitGlobalGLFunctions()
{
#ifdef _WIN32
	FreeLibrary(wgl.lib);
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
		features|=v_vsync;
#endif
		return features;
	}
	
#ifdef _WIN32
	HWND hwnd;
	HDC hdc;
	HGLRC hglrc;
#endif
	
	struct {
void (APIENTRY * Clear)(GLbitfield mask);
void (APIENTRY * ClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
GLenum (APIENTRY * GetError)();
		void (APIENTRY * ReadPixels)(GLint x,GLint y,GLsizei width,GLsizei height,GLenum format,GLenum type,GLvoid *pixels);
		
		void (APIENTRY * GenTextures)(GLsizei n,GLuint *textures);
		void (APIENTRY * BindTexture)(GLenum target,GLuint texture);
		void (APIENTRY * TexImage2D)(GLenum target,GLint level,GLint internalformat,GLsizei width,GLsizei height,
		                             GLint border,GLenum format,GLenum type,const GLvoid *pixels);
		void (APIENTRY * TexSubImage2D)(GLenum target,GLint level,GLint xoffset,GLint yoffset,GLsizei width,GLsizei height,
		                                GLenum format,GLenum type,const GLvoid *pixels);
		void (APIENTRY * PixelStorei)(GLenum pname,GLint param);
	} gl;
	
	bool is3d;
	
	//2D input
	//uint8_t in2_bpp;
	uint8_t in2_bytepp;
	GLenum in2_fmt;
	unsigned int in2_width;
	unsigned int in2_height;
	GLuint in2_texture;
	
	GLuint* sh_tex;
	GLuint* sh_fbo;
	
	video* out_chain;
	
	/*private*/ bool load_gl_functions(unsigned int version)
	{
#define sym(name) *(funcptr*)&gl.name = (funcptr)pgl.GetProcAddress("gl"#name); WIN32_SYM_FALLBACK(name); if (!gl.name) return false;
#define symARB(name) *(funcptr*)&gl.name = (funcptr)pgl.GetProcAddress("gl"#name"ARB"); if (!gl.name) return false;
#define symver(name, minver) if (version >= minver) { symver(name); } else gl.name=NULL;
#define symverARB(name, minver) if (version >= minver) { symverARB(name); } else gl.name=NULL;
sym(Clear);
sym(ClearColor);
sym(GetError);
		sym(ReadPixels);
		sym(GenTextures);
		sym(BindTexture);
		sym(TexImage2D);
		sym(TexSubImage2D);
		sym(PixelStorei);
#undef sym
#undef symARB
		return true;
	}
	
	/*private*/ void begin()
	{
		wgl.MakeCurrent(this->hdc, this->hglrc);
	}
	
	/*private*/ void end()
	{
		wgl.MakeCurrent(this->hdc, NULL);
	}
	
	/*private*/ bool construct(uintptr_t windowhandle, bool gles, unsigned int major, unsigned int minor)
	{
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
			PFNWGLCREATECONTEXTATTRIBSARBPROC CreateContextAttribs=(PFNWGLCREATECONTEXTATTRIBSARBPROC)wgl.GetProcAddress("wglCreateContextAttribsARB");
			const int attribs[] = {
				WGL_CONTEXT_MAJOR_VERSION_ARB, (int)major,
				WGL_CONTEXT_MINOR_VERSION_ARB, (int)minor,
				//WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,//https://www.opengl.org/wiki/Core_And_Compatibility_in_Contexts says do not use
			0 };
			this->hglrc=CreateContextAttribs(this->hdc, NULL/*share*/, attribs);
			
			wgl.MakeCurrent(this->hdc, this->hglrc);
			wgl.DeleteContext(hglrc_v2);
		}
		
		if (!load_gl_functions(major*10+minor)) return false;
		end();
		
		this->out_chain=NULL;
		
		return true;
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
		begin();
		
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
		
		end();
	}
	
	//void draw_2d_where(unsigned int width, unsigned int height, void * * data, unsigned int * pitch);
	
	void draw_2d(unsigned int width, unsigned int height, const void * data, unsigned int pitch)
	{
		begin();
		
		gl.BindTexture(GL_TEXTURE_2D, this->in2_texture);
		gl.PixelStorei(GL_UNPACK_ROW_LENGTH, pitch/this->in2_bytepp);
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, this->in2_fmt, data);
		gl.BindTexture(GL_TEXTURE_2D, 0);
		
gl.ClearColor(0.5f,0.6f,0.7f,0.5f);
gl.Clear(GL_COLOR_BUFFER_BIT);
		
		draw_shared();
		end();
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
		
		return false;
	}
	
	void finalize_3d()
	{
	}
	
	uintptr_t input_3d_get_current_framebuffer()
	{
		return 0;
	}
	
	funcptr input_3d_get_proc_address(const char * sym)
	{
		return (funcptr)pgl.GetProcAddress(sym);
	}
	
	void draw_3d(unsigned int width, unsigned int height)
	{
		draw_shared();
	}
	
	/*private*/ void draw_shared()
	{
		if (!this->out_chain)
		{
			pgl.SwapBuffers(this->hdc);
		}
		else
		{
			//gl.ReadPixels
		}
	}
	
	void draw_repeat()
	{
		begin();
		draw_shared();
		end();
	}
	
	
	//void set_vsync(double fps);
	
	
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
		//TODO: destroy various resources
		wgl.MakeCurrent(this->hdc, NULL);
		if (this->hglrc) wgl.DeleteContext(this->hglrc);
		if (this->hdc) ReleaseDC(this->hwnd, this->hdc);
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
