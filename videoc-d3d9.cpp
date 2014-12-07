#include "io.h"
#ifdef VIDEO_D3D9
#define video cvideo
#define CINTERFACE
#undef bind
#include <D3D9.h>
#define bind BIND_CB

#define this This

#define D3DSWAPEFFECT_FLIPEX ((D3DSWAPEFFECT)5)//lazy compiler. and it's an enum so I can't #ifdef it
                                               //(if this one exists, it's safe to ignore; 5 is still 5,
                                               // and they can't break ABI so it must remain 5.)

#ifndef D3DPRESENT_FORCEIMMEDIATE
#define D3DPRESENT_FORCEIMMEDIATE 0x00000100L
#endif
#ifndef D3DPRESENT_DONOTWAIT
#define D3DPRESENT_DONOTWAIT 0x00000001L
#endif

STATIC_ASSERT_GSCOPE_CAN_EVALUATE(((IDirect3D9Ex*)NULL)->lpVtbl->RegisterSoftwareDevice, fix_your_d3d9_header);
//IF THIS ONE FIRES:
//The DirectX SDK, at least versions November 2008 and August 2009 (others untested),
// lacks an entry in the vtable for IDirect3D9Ex (it's present in IDirect3D9).
//This can cause rather nasty runtime crashes; for ease of debugging, it's documented here and tested at compile time.
//The fix is as follows:
//Look up the following line in your D3d9.h
//  DECLARE_INTERFACE_(IDirect3D9Ex, IDirect3D9)
//then find the following line, which about five lines down
//  /*** IDirect3D9 methods ***/
//and add the following line directly after it
//  STDMETHOD(RegisterSoftwareDevice)(THIS_ void* pInitializeFunction) PURE;
//and save. Things should now compile and run properly.

static HMODULE hD3D9=NULL;
static HRESULT (WINAPI * lpDirect3DCreate9Ex)(UINT SDKVersion, IDirect3D9Ex* * ppD3D);
static IDirect3D9* (WINAPI * lpDirect3DCreate9)(UINT SDKVersion);

static bool libLoad();
static void libRelease();

struct video_d3d9 {
	struct video i;
	
	HWND hwnd;
	
	bool ex;
	
	IDirect3D9Ex* d3d;
	IDirect3DDevice9Ex* device;
	IDirect3DVertexBuffer9* vertexbuf;
	
	unsigned int screenwidth;
	unsigned int screenheight;
	
	D3DFORMAT texformat;
	UINT texwidth;
	UINT texheight;
	UINT bytes_per_row;
	IDirect3DTexture9* texture;
	float texoffsetx;
	float texoffsety;
	
	DWORD syncflags;
};

static void clear(struct video_d3d9 * this)
{
	if (this->vertexbuf) this->vertexbuf->lpVtbl->Release(this->vertexbuf);
	this->vertexbuf=NULL;
	
	if (this->texture) this->texture->lpVtbl->Release(this->texture);
	this->texture=NULL;
	this->texwidth=0;
	this->texheight=0;
	
	if (this->device) this->device->lpVtbl->Release(this->device);
	this->device=NULL;
}

static bool recreate(struct video_d3d9 * this, unsigned int screenwidth, unsigned int screenheight, unsigned int depth)
{
	clear(this);
	
	//depth=0 is allowed too, it means keep current value
	if (depth==15) this->texformat=D3DFMT_A1R5G5B5;//X1R5G5B5 fails for no obvious reason
	if (depth==16) this->texformat=D3DFMT_R5G6B5;
	if (depth==32) this->texformat=D3DFMT_X8R8G8B8;
	
	D3DPRESENT_PARAMETERS parameters;
	memset(&parameters, 0, sizeof(parameters));
	parameters.BackBufferWidth=0;
	parameters.BackBufferHeight=0;
	//parameters.BackBufferFormat=this->texformat;
	parameters.BackBufferFormat=D3DFMT_UNKNOWN;
	parameters.BackBufferCount=2;//this value is confirmed by experiments; it is the lowest value that doesn't force vsync on.
	parameters.MultiSampleType=D3DMULTISAMPLE_NONE;
	parameters.MultiSampleQuality=0;
	if (this->ex) parameters.SwapEffect=D3DSWAPEFFECT_FLIPEX;
	else parameters.SwapEffect=D3DSWAPEFFECT_DISCARD;
	parameters.hDeviceWindow=this->hwnd;
	parameters.Windowed=TRUE;
	parameters.EnableAutoDepthStencil=FALSE;//TODO: is this useful?
	//parameters.AutoDepthStencilFormat;//ignored
	parameters.Flags=0;
	parameters.FullScreen_RefreshRateInHz=0;
	parameters.PresentationInterval=D3DPRESENT_INTERVAL_DEFAULT;//TODO: try _ONE
	
	if (this->ex)
	{
		if (FAILED(this->d3d->lpVtbl->CreateDeviceEx(this->d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, this->hwnd,
			                             D3DCREATE_MIXED_VERTEXPROCESSING/* | D3DCREATE_PUREDEVICE*/,
			                             //PUREDEVICE doesn't work for everyone, and is of questionable use anyways
			                             &parameters, NULL, &this->device)) &&
			   FAILED(this->d3d->lpVtbl->CreateDeviceEx(this->d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_SW, this->hwnd,
			                             D3DCREATE_MIXED_VERTEXPROCESSING/* | D3DCREATE_PUREDEVICE*/,
			                             &parameters, NULL, &this->device)))
		{
			return false;
		}
	}
	else
	{
		parameters.PresentationInterval=(this->syncflags?D3DPRESENT_INTERVAL_DEFAULT:D3DPRESENT_INTERVAL_IMMEDIATE);
		
		if (FAILED(this->d3d->lpVtbl->CreateDevice(this->d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, this->hwnd,
		                             D3DCREATE_MIXED_VERTEXPROCESSING/* | D3DCREATE_PUREDEVICE*/,
		                             &parameters, (IDirect3DDevice9**)&this->device)) &&
		    FAILED(this->d3d->lpVtbl->CreateDevice(this->d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_SW, this->hwnd,
		                             D3DCREATE_MIXED_VERTEXPROCESSING/* | D3DCREATE_PUREDEVICE*/,
		                             &parameters, (IDirect3DDevice9**)&this->device)))
		{
			return false;
		}
	}
	
	this->device->lpVtbl->SetRenderState(this->device, D3DRS_CULLMODE, D3DCULL_NONE);
	this->device->lpVtbl->SetRenderState(this->device, D3DRS_LIGHTING, FALSE);
	
	if (FAILED(this->device->lpVtbl->CreateVertexBuffer(this->device, sizeof(float)*5*4, 0, D3DFVF_XYZ|D3DFVF_TEX1, D3DPOOL_DEFAULT, &this->vertexbuf, NULL)))
	{
		return false;
	}
	
	if (screenwidth)
	{
		this->screenwidth=screenwidth;
		this->screenheight=screenheight;
	}
	this->texoffsetx=0.5/this->screenwidth;
	this->texoffsety=0.5/this->screenheight;
	
	return true;
}

static void reinit(struct video * this_, unsigned int screen_width, unsigned int screen_height, unsigned int depth, double fps)
{
	struct video_d3d9 * this=(struct video_d3d9*)this_;
	
	recreate(this, screen_width, screen_height, depth);
}

static void draw(struct video * this_, unsigned int width, unsigned int height, const void * data, unsigned int pitch)
{
	struct video_d3d9 * this=(struct video_d3d9*)this_;
	
	HRESULT status=this->device->lpVtbl->TestCooperativeLevel(this->device);
	if (status==D3DERR_DEVICELOST) return;
	if (status==D3DERR_DEVICENOTRESET)
	{
		recreate(this, 0,0, 0);
		status=this->device->lpVtbl->TestCooperativeLevel(this->device);
		if (status!=D3D_OK) return;
	}
	
//this->device->lpVtbl->Clear(this->device, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 255), 1.0f, 0);
	
	if (this->texwidth!=width || this->texheight!=height || !this->texture)
	{
		this->texoffsetx=0.5/this->screenwidth;
		this->texoffsety=0.5/this->screenheight;
		
		this->texwidth=width;
		this->texheight=height;
		
		if (this->texture) this->texture->lpVtbl->Release(this->texture);
		this->texture=NULL;
		HRESULT hr=this->device->lpVtbl->CreateTexture(this->device, width, height, 1, D3DUSAGE_DYNAMIC, this->texformat, D3DPOOL_DEFAULT, &this->texture, NULL);
		if (FAILED(hr))
		{
			this->texture=NULL;
		}
		
		if (!this->texture)
		{
			this->device->lpVtbl->Clear(this->device, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(255, 0, 0), 1.0f, 0);
			goto present;
		}
		
		if (this->texformat==D3DFMT_X8R8G8B8) this->bytes_per_row=width*4;
		else this->bytes_per_row=width*2;
		
		
		float* vertices;
		if (FAILED(this->vertexbuf->lpVtbl->Lock(this->vertexbuf, 0, 0, (void**)&vertices, 0)))
		{
			this->texture->lpVtbl->Release(this->texture);
			this->texture=NULL;
			return;
		}
		
		float srcvertices[]={
			-1,-1,0, 0+this->texoffsetx,1+this->texoffsety,
			-1, 1,0, 0+this->texoffsetx,0+this->texoffsety,
			 1,-1,0, 1+this->texoffsetx,1+this->texoffsety,
			 1, 1,0, 1+this->texoffsetx,0+this->texoffsety,
		};
		memcpy(vertices, srcvertices, sizeof(float)*5*4);
		this->vertexbuf->lpVtbl->Unlock(this->vertexbuf);
	}
	
	if (data)
	{
		D3DLOCKED_RECT locked;
		if (FAILED(this->texture->lpVtbl->LockRect(this->texture, 0, &locked, NULL, D3DLOCK_DISCARD|D3DLOCK_NOOVERWRITE)))
		{
			this->device->lpVtbl->Clear(this->device, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(255, 255, 0), 1.0f, 0);
			if (this->ex) this->device->lpVtbl->PresentEx(this->device, NULL, NULL, NULL, NULL, 0);
			else this->device->lpVtbl->Present(this->device, NULL, NULL, NULL, NULL);
			return;
		}
		if ((unsigned int)locked.Pitch==pitch) memcpy(locked.pBits, data, pitch*(height-1)+this->bytes_per_row);
		else
		{
			for (unsigned int i=0;i<height;i++)
			{
				memcpy((uint8_t*)locked.pBits + i*locked.Pitch, (uint8_t*)data + i*pitch, this->bytes_per_row);
			}
		}
		this->texture->lpVtbl->UnlockRect(this->texture, 0);
//  [in]   UINT Level,
//  [out]  D3DLOCKED_RECT *pLockedRect,
//  [in]   const RECT *pRect,
//  [in]   DWORD Flags
//);
	}
	
	this->device->lpVtbl->BeginScene(this->device);
	
	this->device->lpVtbl->SetTexture(this->device, 0, (struct IDirect3DBaseTexture9*)this->texture);//apparently this one is subclassed
	this->device->lpVtbl->SetTextureStageState(this->device, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	this->device->lpVtbl->SetTextureStageState(this->device, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	this->device->lpVtbl->SetTextureStageState(this->device, 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	
	this->device->lpVtbl->SetSamplerState(this->device, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	this->device->lpVtbl->SetSamplerState(this->device, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	this->device->lpVtbl->SetSamplerState(this->device, 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
	
	//this->device->lpVtbl->SetTextureStageState(this->device, 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT4 | D3DTTFF_PROJECTED );
	//this->device->lpVtbl->SetTextureStageState(this->device, 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION );
	
	this->device->lpVtbl->SetStreamSource(this->device, 0, this->vertexbuf, 0, sizeof(float)*5);
	this->device->lpVtbl->SetFVF(this->device, D3DFVF_XYZ|D3DFVF_TEX1);
	this->device->lpVtbl->DrawPrimitive(this->device, D3DPT_TRIANGLESTRIP, 0, 2);
	
	this->device->lpVtbl->EndScene(this->device);
	
present:
	if (this->ex) this->device->lpVtbl->PresentEx(this->device, NULL, NULL, NULL, NULL, this->syncflags);
	else this->device->lpVtbl->Present(this->device, NULL, NULL, NULL, NULL);
}

static bool set_sync(struct video * this_, bool sync)
{
	struct video_d3d9 * this=(struct video_d3d9*)this_;
	bool ret=this->syncflags;
	if (this->ex)
	{
		this->syncflags=(sync ? 0 : D3DPRESENT_FORCEIMMEDIATE|D3DPRESENT_DONOTWAIT);
	}
	else
	{
		if (this->syncflags != (DWORD)sync)
		{
			this->syncflags=(sync);
			recreate(this, 0,0, 0);
		}
	}
	return ret;
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

static bool has_sync(struct video * this_)
{
	//struct video_d3d9 * this=(struct video_d3d9*)this_;
	//return (this->ex);
	return true;
}

static void free_(struct video * this_)
{
	struct video_d3d9 * this=(struct video_d3d9*)this_;
	
	clear(this);
	if (this->d3d) this->d3d->lpVtbl->Release(this->d3d);
	libRelease();
	
	free(this);
}

static bool libLoad()
{
	hD3D9=LoadLibrary("d3d9.dll");
	if (!hD3D9) return false;
	//lpDirect3DCreate9=Direct3DCreate9;//these are for type checking; they're not needed anymore
	//lpDirect3DCreate9Ex=Direct3DCreate9Ex;
	lpDirect3DCreate9=(IDirect3D9* (WINAPI*)(UINT))GetProcAddress(hD3D9, "Direct3DCreate9");
	if (!lpDirect3DCreate9) { FreeLibrary(hD3D9); return false; }
	lpDirect3DCreate9Ex=(HRESULT (WINAPI*)(UINT,IDirect3D9Ex**))GetProcAddress(hD3D9, "Direct3DCreate9Ex");
	//if (!lpDirect3DCreate9Ex) return false;
	return true;
}

static void libRelease()
{
	FreeLibrary(hD3D9);
}

struct video * cvideo_create_d3d9(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                 unsigned int depth, double fps)

{
	if (!libLoad()) return NULL;
	
	struct video_d3d9 * this=malloc(sizeof(struct video_d3d9));
	this->i.reinit=reinit;
	this->i.draw=draw;
	this->i.set_sync=set_sync;
	this->i.has_sync=has_sync;
	this->i.repeat_frame=repeat_frame;
	this->i.free=free_;
	
	this->hwnd=(HWND)windowhandle;
	
	this->ex=false;
	
	this->d3d=NULL;
	this->device=NULL;
	this->texture=NULL;
	this->vertexbuf=NULL;
	
	this->syncflags=0;
	
	if (lpDirect3DCreate9Ex && !FAILED(lpDirect3DCreate9Ex(D3D_SDK_VERSION, &this->d3d)))
	{
		this->ex=true;
	}
	else
	{
		this->d3d=(IDirect3D9Ex*)lpDirect3DCreate9(D3D_SDK_VERSION);
		if (!this->d3d) goto cancel;
		this->syncflags=true;
	}
	
	if (!recreate(this, screen_width, screen_height, depth)) goto cancel;
	
	return (struct video*)this;
	
cancel:
	free_((struct video*)this);
	return NULL;
}

#undef video
static video* video_create_d3d9(uintptr_t windowhandle)
{
	return video_create_compat(cvideo_create_d3d9(windowhandle, 32, 32, 16, 60));
}
const video::driver video::create_d3d9 = {"Direct3D", video_create_d3d9, NULL, video::f_vsync};
#endif
