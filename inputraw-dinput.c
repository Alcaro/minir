#include "minir.h"
#ifdef INPUT_DIRECTINPUT
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
//#include "libretro.h"

static HMODULE hDInput=NULL;
static HRESULT WINAPI (*lpDirectInput8Create)(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID * ppvOut, LPUNKNOWN punkOuter);

struct inputraw_directinput {
	struct inputraw i;
	
	LPDIRECTINPUT8 context;
	LPDIRECTINPUTDEVICE8 keyboard;
};

static bool keyboard_poll(struct inputraw * this_, unsigned int kb_id, unsigned char * keys)
{
	struct inputraw_directinput * this=(struct inputraw_directinput*)this_;
	if (FAILED(this->keyboard->lpVtbl->GetDeviceState(this->keyboard, 256, keys)))
	{
		this->keyboard->lpVtbl->Acquire(this->keyboard);
		if (FAILED(this->keyboard->lpVtbl->GetDeviceState(this->keyboard, 256, keys))) return false;
	}
//printf("bad scancode=%.2X vk=%.2X\n",i,MapVirtualKey(i, MAPVK_VSC_TO_VK));
	return true;
}

static void free_(struct inputraw * this_)
{
	struct inputraw_directinput * this=(struct inputraw_directinput*)this_;
	if (this->keyboard) this->keyboard->lpVtbl->Release(this->keyboard);
	if (this->context) this->context->lpVtbl->Release(this->context);
	FreeLibrary(hDInput);
	free(this);
}

static bool libLoad()
{
	hDInput=LoadLibrary("dinput8.dll");
	if (!hDInput) return false;
	//lpDirectInput8Create=DirectInput8Create;
	lpDirectInput8Create=(HRESULT WINAPI(*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN))GetProcAddress(hDInput, "DirectInput8Create");
	if (!lpDirectInput8Create) { FreeLibrary(hDInput); return false; }
	return true;
}

struct inputraw * _inputraw_create_directinput(uintptr_t windowhandle)
{
	if (!libLoad()) return NULL;
	
	struct inputraw_directinput * this=malloc(sizeof(struct inputraw_directinput));
	_inputraw_windows_keyboard_create_shared((struct inputraw*)this);
	//this->i.keyboard_num_keyboards=keyboard_num_keyboards;
	//this->i.keyboard_num_keys=keyboard_num_keys;
	this->i.keyboard_poll=keyboard_poll;
	//this->i.keyboard_get_map=keyboard_get_map;
	this->i.free=free_;
	
	this->context=NULL;
	this->keyboard=NULL;
	
	lpDirectInput8Create(GetModuleHandle(NULL), 0x0800, &IID_IDirectInput8, (void**)&this->context, 0);
	if (!this->context) goto cancel;
	
	this->context->lpVtbl->CreateDevice(this->context, &GUID_SysKeyboard, &this->keyboard, 0);
	if (!this->keyboard) goto cancel;
	
	if (FAILED(this->keyboard->lpVtbl->SetDataFormat(this->keyboard, &c_dfDIKeyboard))) goto cancel;
	if (FAILED(this->keyboard->lpVtbl->SetCooperativeLevel(this->keyboard, (HWND)windowhandle, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND))) goto cancel;
	
	return (struct inputraw*)this;
	
cancel:
	free_((struct inputraw*)this);
	return NULL;
}
#endif
