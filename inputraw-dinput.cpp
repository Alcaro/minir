#include "minir.h"
#ifdef INPUT_DIRECTINPUT
#define DIRECTINPUT_VERSION 0x0800
#define CINTERFACE
#undef bind
#include <dinput.h>
#define bind BIND_CB
//#include "libretro.h"

#define this This

static HMODULE hDInput=NULL;
static HRESULT (WINAPI * lpDirectInput8Create)(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID * ppvOut, LPUNKNOWN punkOuter);

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
	lpDirectInput8Create=(HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN))GetProcAddress(hDInput, "DirectInput8Create");
	if (!lpDirectInput8Create) { FreeLibrary(hDInput); return false; }
	return true;
}

//stolen from http://msdn.microsoft.com/en-us/library/windows/desktop/ms687061%28v=vs.85%29.aspx
/*
void ErrorDescription(HRESULT hr) 
{ 
     if(FACILITY_WINDOWS == HRESULT_FACILITY(hr)) 
         hr = HRESULT_CODE(hr); 
     TCHAR* szErrMsg; 

     if(FormatMessage( 
       FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM, 
       NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
       (LPTSTR)&szErrMsg, 0, NULL) != 0) 
     { 
         printf(TEXT("%s"), szErrMsg); 
         LocalFree(szErrMsg); 
     } else 
         printf( TEXT("[Could not find a description for error # %#x.]\n"), hr); 
}
*/

struct inputraw * _inputraw_create_directinput(uintptr_t windowhandle)
{
	if (!libLoad()) return NULL;
	
	//const GUID IID_IDirectInput8 = { 0xBF798031, 0x483A, 0x4DA2, 0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00 };
	//const GUID GUID_SysKeyboard = { 0x6F1D2B61, 0xD5A0, 0x11CF, 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 };
	//enabling those adds half a dozen other dependencies
	
	struct inputraw_directinput * this=malloc(sizeof(struct inputraw_directinput));
	_inputraw_windows_keyboard_create_shared((struct inputraw*)this);
	//this->i.keyboard_num_keyboards=keyboard_num_keyboards;
	//this->i.keyboard_num_keys=keyboard_num_keys;
	this->i.keyboard_poll=keyboard_poll;
	//this->i.keyboard_get_map=keyboard_get_map;
this->i.feat=inputkb::f_direct|inputkb::f_public|inputkb::f_pollable;
	this->i.free=free_;
	
	this->context=NULL;
	this->keyboard=NULL;
	
	lpDirectInput8Create(GetModuleHandle(NULL), 0x0800, IID_IDirectInput8, (void**)&this->context, 0);
	if (!this->context) goto cancel;
	
	this->context->lpVtbl->CreateDevice(this->context, GUID_SysKeyboard, &this->keyboard, 0);
	if (!this->keyboard) goto cancel;
	
	if (FAILED(this->keyboard->lpVtbl->SetDataFormat(this->keyboard, &c_dfDIKeyboard))) goto cancel;
	if (FAILED(this->keyboard->lpVtbl->SetCooperativeLevel(this->keyboard, GetParent((HWND)windowhandle), DISCL_NONEXCLUSIVE | DISCL_FOREGROUND))) goto cancel;
	
	return (struct inputraw*)this;
	
cancel:
	free_((struct inputraw*)this);
	return NULL;
}
#endif
