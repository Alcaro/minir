#include "minir.h"
#if defined(VIDEO_OPENGL) && !defined(_WIN32)
#define video cvideo
#include <stdlib.h>

#define this This

//this file is based on ruby by byuu

//this file uses OpenGL 1.2 or 1.1, depending on what's available
//1.2 will give a significant performance boost as it won't need to unpack the
// pixel data in software; 1.1 is only used for the Windows default renderer
// (however, Windows default renderer doesn't support vsync either, so we reject it - and therefore the unpacker will never run)
//nothing from 1.3 or 1.4 is used

//check wglInitSwapControlARB
/*
static char *proc_names[] =
{
#if !EXT_DEFINES_PROTO || !defined(WGL_EXT_swap_control)
	"wglSwapIntervalEXT",
	"wglGetSwapIntervalEXT",
#endif
	NULL
};

#define wglInitSwapControlARB() InitExtension("WGL_EXT_swap_control", proc_names)

return in_extension_list(name, (char*)glGetString(GL_EXTENSIONS)) ||
       in_extension_list(name, (char*)gluGetString(GLU_EXTENSIONS)) ||
       in_extension_list(name, (char*)GetPlatformExtensionsString());
*/
//also check glFinish()

#if 0
#include<stdio.h>
#include<time.h>
static unsigned long t()
{
static struct timespec prev;
struct timespec this;
struct timespec diff;
clock_gettime(CLOCK_MONOTONIC, &this);
if (this.tv_nsec<prev.tv_nsec)
{
diff.tv_sec=this.tv_sec-1-prev.tv_sec;
diff.tv_nsec=this.tv_nsec+1000000000-prev.tv_nsec;
}
else
{
diff.tv_sec=this.tv_sec-prev.tv_sec;
diff.tv_nsec=this.tv_nsec-prev.tv_nsec;
}
prev=this;
return diff.tv_nsec;
}
#define q1 unsigned long t_sum=0; unsigned long t_tmp;
#define q2 t_tmp=t(); t_sum+=t_tmp; printf("%09lu ",t_tmp);
#define q22(x) t_tmp=t(); t_sum+=t_tmp; printf(x"%09lu ",t_tmp);
#define q3 printf(" %09lu\n",t_sum);
#else
#define q1
#define q2
#define q22(x)
#define q3
#endif

//#define GL_VERSION_1_2 0
#define GL_VERSION_1_3 0
#define GL_ARB_imaging 0

#if defined(WNDPROT_X11)
  #include <GL/gl.h>
  #include <GL/glx.h>
  #define glGetProcAddress(name) (*glXGetProcAddress)((const GLubyte*)(name))
#elif defined(WNDPROT_WINDOWS)
  #undef bind
  #include <windows.h>
  #define bind BIND_CB
  #include <GL/gl.h>
  #include <GL/glext.h>
  #define glGetProcAddress(name) wglGetProcAddress(name)
#else
  #error Cannot use this driver with this window protocol
#endif

struct video_opengl {
	struct video i;
	
#ifdef WNDPROT_X11
	Display* display;
	int screen;
	
	Window xwindow;
	
	Colormap colormap;
	GLXContext glxcontext;
	
	int (*glSwapInterval)(int);
#endif
	
#ifdef WNDPROT_WINDOWS
	HDC display;
	HGLRC wglcontext;
	
	int (WINAPI * glSwapInterval)(int);
#endif
	
	unsigned int screenwidth;
	unsigned int screenheight;
	
	GLuint gltexture;
	unsigned int texwidth;
	unsigned int texheight;
	
	int pixelformat;
	int byteperpix;
	GLenum inputformat;
	GLenum format;
	
	bool doublebuffer;
	
	bool support_non_power_2;
	bool support_bitpack_1555;
	bool support_bitpack_565;
	bool support_bitpack_8888;
	
	bool vsync;
	
	bool convert_image;
	void* convert_buf;
	size_t convert_bufsize;
};

static void reinit(struct video * this_, unsigned int screen_width, unsigned int screen_height, unsigned int depth, double fps)
{
	struct video_opengl * this=(struct video_opengl*)this_;
#ifdef WNDPROT_X11
	XResizeWindow(this->display, this->xwindow, screen_width, screen_height);
#endif
	this->screenwidth=screen_width;
	this->screenheight=screen_height;
	
	this->pixelformat=depth;
	this->convert_image=false;
	this->byteperpix=1;//to avoid a zero division
	
	if (depth==15)
	{
		if (this->support_bitpack_1555)
		{
			this->inputformat=GL_UNSIGNED_SHORT_1_5_5_5_REV;
			this->format=GL_BGRA;
			this->byteperpix=sizeof(GLushort);
		}
		else
		{
			this->convert_image=true;
		}
	}
	if (depth==16)
	{
		if (this->support_bitpack_565)
		{
			this->inputformat=GL_UNSIGNED_SHORT_5_6_5;
			this->format=GL_RGB;
			this->byteperpix=sizeof(GLushort);
		}
		else
		{
			this->convert_image=true;
		}
	}
	if (depth==32)
	{
		if (this->support_bitpack_8888)
		{
			this->inputformat=GL_UNSIGNED_INT_8_8_8_8_REV;
			this->format=GL_BGRA;
			this->byteperpix=sizeof(GLuint);
		}
		else
		{
			this->convert_image=true;
		}
	}
}

static unsigned int bitround(unsigned int in)
{
	in--;
	in|=in>>1;
	in|=in>>2;
	in|=in>>4;
	in|=in>>8;
	in|=in>>16;
	in++;
	return in;
}

static void draw(struct video * this_, unsigned int width, unsigned int height, const void * data, unsigned int pitch)
{
//puts("b");
q1
q2
	struct video_opengl * this=(struct video_opengl*)this_;
//printf("a%i\n",glGetError());
	
#ifdef WNDPROT_WINDOWS
	wglMakeCurrent(this->display, this->wglcontext);
#endif
	
	if (data)
	{
		if (this->texwidth<width || this->texheight<height)
		{
			if (this->gltexture) glDeleteTextures(1, &this->gltexture);
			if (this->texwidth <width)  this->texwidth =width;
			if (this->texheight<height) this->texheight=height;
			
			if (!this->support_non_power_2) this->texwidth =bitround(this->texwidth );
			if (!this->support_non_power_2) this->texheight=bitround(this->texheight);
			
			glGenTextures(1, &this->gltexture);
			glBindTexture(GL_TEXTURE_2D, this->gltexture);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch/this->byteperpix);
			if (!this->convert_image)
			{
				glTexImage2D(GL_TEXTURE_2D,
					/* mip-map level = */ 0, /* internal format = */ GL_RGB,
					this->texwidth, this->texheight, /* border = */ 0, /* format = */ this->format,
					this->inputformat, NULL);
			}
			else
			{
				glTexImage2D(GL_TEXTURE_2D,
					/* mip-map level = */ 0, /* internal format = */ GL_RGB,
					this->texwidth, this->texheight, /* border = */ 0, /* format = */ GL_RGB,
					GL_UNSIGNED_BYTE, NULL);
			}
		}
		
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, this->screenwidth, 0, this->screenheight, -1.0, 1.0);
		glViewport(0, 0, this->screenwidth, this->screenheight);
		
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		
		if (!this->convert_image)
		{
			glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch/this->byteperpix);
q2
			glTexSubImage2D(GL_TEXTURE_2D,
				/* mip-map level = */ 0, /* x = */ 0, /* y = */ 0,
				width, height, this->format, this->inputformat, data);
q22("tx")
		}
		else
		{
			if (3*width*height > this->convert_bufsize)
			{
				free(this->convert_buf);
				this->convert_buf=malloc(3*width*height);
				this->convert_bufsize=3*width*height;
			}
			struct image src;
			src.width=width;
			src.height=height;
			src.pixels=(void*)data;
			src.pitch=pitch;
			src.bpp=this->pixelformat;
			struct image dst;
			dst.width=width;
			dst.height=height;
			dst.pixels=this->convert_buf;
			dst.pitch=3*width;
			dst.bpp=24;
			image_convert(&src, &dst);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, width);
			glTexSubImage2D(GL_TEXTURE_2D,
				/* mip-map level = */ 0, /* x = */ 0, /* y = */ 0,
				width, height, GL_BGR, GL_UNSIGNED_BYTE, this->convert_buf);
		}
	}
	
	//OpenGL projection sets 0,0 as *bottom-left* of screen.
	//therefore, below vertices flip image to support top-left source.
	//texture range = x1:0.0, y1:0.0, x2:1.0, y2:1.0
	//vertex range = x1:0, y1:0, x2:width, y2:height
	double w = (double)width  / (double)this->texwidth;
	double h = (double)height / (double)this->texheight;
	int u = this->screenwidth;
	int v = this->screenheight;
	glBegin(GL_TRIANGLE_STRIP);
	glTexCoord2f(0, 0); glVertex3i(0, v, 0);
	glTexCoord2f(w, 0); glVertex3i(u, v, 0);
	glTexCoord2f(0, h); glVertex3i(0, 0, 0);
	glTexCoord2f(w, h); glVertex3i(u, 0, 0);
	glEnd();
	
q2
	//glFlush();
	
q22("fl")
#ifdef WNDPROT_X11
	if (this->doublebuffer) glXSwapBuffers(this->display, this->xwindow);
	else glFlush();
#endif
	
#ifdef WNDPROT_WINDOWS
	glFinish();
	SwapBuffers(this->display);
#endif
q2
q3
}

static bool set_sync(struct video * this_, bool sync)
{
	struct video_opengl * this=(struct video_opengl*)this_;
	bool ret=this->vsync;
	this->vsync=sync;
	if (this->glSwapInterval) this->glSwapInterval(sync?1:0);
	return ret;
}

static bool has_sync(struct video * this_)
{
	struct video_opengl * this=(struct video_opengl*)this_;
	(void)this;//may be unused on some protocols
#ifdef WNDPROT_X11
	return (this->glSwapInterval && this->doublebuffer);
#endif
#ifdef WNDPROT_WINDOWS
	return (this->glSwapInterval);
#endif
}

static bool repeat_frame(struct video * this_, unsigned int * width, unsigned int * height,
                                               const void * * data, unsigned int * pitch, unsigned int * bpp)
{
	if (width) *width=0;
	if (height) *height=0;
	if (data) *data=NULL;
	if (pitch) *pitch=0;
	if (bpp) *bpp=16;
	return false;
}

static void free_(struct video * this_)
{
	struct video_opengl * this=(struct video_opengl*)this_;
	
	if (this->gltexture) glDeleteTextures(1, &this->gltexture);
	
#ifdef WNDPROT_X11
	if (this->glxcontext) glXDestroyContext(this->display, this->glxcontext);
	if (this->xwindow) XDestroyWindow(this->display, this->xwindow);
	if (this->colormap) XFreeColormap(this->display, this->colormap);
#endif
#ifdef WNDPROT_WINDOWS
	if (this->wglcontext) wglDeleteContext(this->wglcontext);
	if (this->display) ReleaseDC(WindowFromDC(this->display), this->display);
#endif
	
	free(this->convert_buf);
	
	free(this);
}

#ifdef WNDPROT_X11
static Bool glx_wait_for_map_notify(Display* d, XEvent* e, char* arg) {
  return (e->type == MapNotify) && (e->xmap.window == (Window)arg);
}
#endif

struct video * cvideo_create_opengl_old(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                   unsigned int depth, double fps)
{
	struct video_opengl * this=malloc(sizeof(struct video_opengl));
	this->i.reinit=reinit;
	this->i.draw=draw;
	this->i.set_sync=set_sync;
	this->i.has_sync=has_sync;
	this->i.repeat_frame=repeat_frame;
	this->i.free=free_;
	
	this->gltexture=0;
	
#ifdef WNDPROT_X11
	this->display = window_x11_get_display()->display;
	this->screen = window_x11_get_display()->screen;
	
	int version_major=0;
	int version_minor=0;
	glXQueryVersion(this->display, &version_major, &version_minor);
	//require GLX 1.2+ API
	if(version_major < 1 || (version_major == 1 && version_minor < 2)) goto cancel;
	
	//let GLX determine the best Visual to use for GL output; provide a few hints
	//note: some video drivers will override double buffering attribute
	XVisualInfo* vi;
	{
		int attributelist[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };
		vi = glXChooseVisual(this->display, this->screen, attributelist);
	}
	
	//Window windowhandle has already been realized, most likely with DefaultVisual.
	//GLX requires that the GL output window has the same Visual as the GLX context.
	//it is not possible to change the Visual of an already realized (created) window.
	//therefore a new child window, using the same GLX Visual, must be created and binded to settings.handle.
	this->colormap = XCreateColormap(this->display, RootWindow(this->display, vi->screen), vi->visual, AllocNone);
	XSetWindowAttributes attributes;
	attributes.colormap = this->colormap;
	attributes.border_pixel = 0;
	attributes.event_mask = StructureNotifyMask;
	this->xwindow = XCreateWindow(this->display, /* parent = */ windowhandle,
		/* x = */ 0, /* y = */ 0, screen_width, screen_height,
		/* border_width = */ 0, vi->depth, InputOutput, vi->visual,
		CWColormap | CWBorderPixel | CWEventMask, &attributes);
	XSetWindowBackground(this->display, this->xwindow, /* color = */ 0);
	XMapWindow(this->display, this->xwindow);
	XEvent event;
	//window must be realized (appear onscreen) before we make the context current
	XPeekIfEvent(this->display, &event, glx_wait_for_map_notify, (char*)this->xwindow);
	
	this->glxcontext = glXCreateContext(this->display, vi, /* sharelist = */ 0, /* direct = */ GL_TRUE);
	glXMakeCurrent(this->display, this->xwindow, this->glxcontext);
	
	//read attributes of frame buffer for later use, as requested attributes from above are not always granted
	{
		int value = 0;
		glXGetConfig(this->display, vi, GLX_DOUBLEBUFFER, &value);
		this->doublebuffer = value;
	}
	
	XFree(vi);
	
	//vertical synchronization
	this->glSwapInterval=NULL;
	if(!this->glSwapInterval) this->glSwapInterval = (int (*)(int))glGetProcAddress("glXSwapIntervalMESA");
	if(!this->glSwapInterval) this->glSwapInterval = (int (*)(int))glGetProcAddress("glXSwapIntervalSGI");
	if( this->glSwapInterval) this->glSwapInterval(1);
#endif
	
#ifdef WNDPROT_WINDOWS
	this->wglcontext=NULL;
	this->display=NULL;
	
	PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
	pfd.nSize      = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion   = 1;
	pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	
	this->display = GetDC((HWND)windowhandle);
	int pixel_format = ChoosePixelFormat(this->display, &pfd);
	if (!pixel_format) goto cancel;
	if (!SetPixelFormat(this->display, pixel_format, &pfd)) goto cancel;
	
	this->wglcontext = wglCreateContext(this->display);
	if (!this->wglcontext) goto cancel;
	if (!wglMakeCurrent(this->display, this->wglcontext)) goto cancel;
	
	//vertical synchronization
	this->glSwapInterval=NULL;
	if(!this->glSwapInterval) this->glSwapInterval = (BOOL (WINAPI*)(int))glGetProcAddress("wglSwapIntervalEXT");
	if( this->glSwapInterval) this->glSwapInterval(1);
	else goto cancel;
#endif
	
	//disable unused features
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_POLYGON_SMOOTH);
	glDisable(GL_STENCIL_TEST);
	
	//enable useful and required features
	glEnable(GL_DITHER);
	glEnable(GL_TEXTURE_2D);
	
	//test for some apparently non-universal features (aka hi Microsoft)
	glGenTextures(1, &this->gltexture);
	glBindTexture(GL_TEXTURE_2D, this->gltexture);
	glTexImage2D(GL_TEXTURE_2D,
					/* mip-map level = */ 0, /* internal format = */ GL_RGB,
					64, 96, /* border = */ 0, /* format = */ GL_RGB,
					GL_UNSIGNED_BYTE, NULL);
	this->support_non_power_2=(glGetError()==0);
	glTexImage2D(GL_TEXTURE_2D,
		/* mip-map level = */ 0, /* internal format = */ GL_RGB,
		128, 128, /* border = */ 0, /* format = */ GL_BGRA,
		GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
	this->support_bitpack_1555=(glGetError()==0);
	glTexImage2D(GL_TEXTURE_2D,
		/* mip-map level = */ 0, /* internal format = */ GL_RGB,
		128, 128, /* border = */ 0, /* format = */ GL_RGB,
		GL_UNSIGNED_SHORT_5_6_5, NULL);
	this->support_bitpack_565=(glGetError()==0);
	glTexImage2D(GL_TEXTURE_2D,
		/* mip-map level = */ 0, /* internal format = */ GL_RGB,
		128, 128, /* border = */ 0, /* format = */ GL_BGRA,
		GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	this->support_bitpack_8888=(glGetError()==0);
	glDeleteTextures(1, &this->gltexture);
	
	this->vsync=true;
	reinit((struct video*)this, screen_width, screen_height, depth, fps);
	
	this->gltexture=0;
	this->texwidth=0;
	this->texheight=0;
	
	this->convert_bufsize=0;
	this->convert_buf=NULL;
	
	return (struct video*)this;
	
cancel:
	free_((struct video*)this);
	return NULL;
}

#undef video
static video* video_create_opengl_old(uintptr_t windowhandle)
{
	return video_create_compat(cvideo_create_opengl_old(windowhandle, 256, 256, 16, 60));
}
extern const driver_video video_opengl_old_desc = {"OpenGL-1.x", video_create_opengl_old, NULL, 0};
#else
static video* video_create_opengl_old(uintptr_t windowhandle)
{
	return NULL;
}
extern const driver_video video_opengl_old_desc = {"OpenGL-1.x", video_create_opengl_old, NULL, 0};
#endif
