#include "minir.h"
#ifdef VIDEO_OPENGLgg
#undef bind
#include <gl/gl.h>
#include <gl/glext.h>
#ifdef _WIN32
#include <gl/wglext.h>
#define GLLibPath "opengl32.dll"
#define PSYM(name) RSYM(w##name, "wgl"#name)
#endif
#define bind BIND_CB
#include "libretro.h"

namespace {
struct {
	dylib* lib;
	
#ifdef _WIN32
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
	WINBOOL (WINAPI * wCreateContext)(HDC hdc);
	WINBOOL (WINAPI * wMakeCurrent)(HDC hdc, HGLRC hglrc);
	WINBOOL (WINAPI * wDeleteContext)(HGLRC hglrc);
#endif
} static gl;

bool InitGLFunctions()
{
	dylib* lib=dylib_create(GLLibPath);
	gl.lib=lib;
	if (!lib) return false;
#define RSYM(name, symbol) gl.name=lib->sym_func(symbol); if (!gl.name) return false
#define SYM(name) RSYM(
#ifdef _WIN32
	PSYM(CreateContext);
	PSYM(MakeCurrent);
	PSYM(DeleteContext);
#endif
	return true;
}

void DeinitGLFunctions()
{
	delete gl.lib;
}

class video_opengl : public video {
public:
	static const uint32_t max_features =
		f_sshot|f_chain|f_vsync|
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
		features|=v_vsync;
#endif
		return features;
	}
	
#ifdef _WIN32
	HWND hwnd;
	HDC hdc;
	HGLRC hglrc;
#endif
	
	void set_input_2d(unsigned int depth, double fps)
	{
	}
	
	void draw_2d(unsigned int width, unsigned int height, const void * data, unsigned int pitch)
	{
	}
	
	void draw_repeat()
	{
	}
	
	/*private*/ bool construct(uintptr_t windowhandle)
	{
		if (!InitGLFunctions()) return false;
		//TODO: clone the hwnd, so I won't need to set pixel format twice
		this->hwnd=(HWND)windowhandle;
		this->hdc=GetDC(this->hwnd);
		/*
		PIXELFORMATDESCRIPTOR pfd;
		memset(pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
		pfd.nSize=sizeof(PIXELFORMATDESCRIPTOR);
		pfd.nVersion=1;
		pfd.nFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
		pfd.iPixelType=PFD_TYPE_RGBA;
		pfd.cColorBits=24;
		pfd.cAlphaBits=0;
		pfd.cAccumBits=0;
		pfd.cDepthBits=0;
		pfd.cStencilBits=0;
		pfd.cAuxBuffers=0;
		pfd.iLayerType=PFD_MAIN_PLANE;
		SetPixelFormat(this->hdc, ChoosePixelFormat(this->hdc, &pfd), &pfd);
		*/
		this->hglrc=wglCreateContext(this->hdc);
		wglMakeCurrent(this->hdc, this->hglrc);
		return true;
	}
	
	~video_opengl()
	{
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(this->hglrc);
		ReleaseDC(this->hwnd, this->hdc);
	}
};


video* video_create_opengl(uintptr_t windowhandle)
{
	video_opengl* ret=new video_opengl;
	if (!ret->construct(windowhandle))
	{
		delete ret;
		return NULL;
	}
	return ret;
}

}

extern const driver_video video_opengl_desc = { "OpenGL", video_create_opengl, video_opengl::max_features };
#endif

video* video_create_opengl(uintptr_t windowhandle) { return NULL; }
extern const driver_video video_opengl_desc = { "OpenGL", video_create_opengl, 0 };
