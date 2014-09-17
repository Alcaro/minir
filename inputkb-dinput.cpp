#include "minir.h"
#ifdef INPUT_DIRECTINPUT
#define DIRECTINPUT_VERSION 0x0800
#undef bind
#include <dinput.h>
#define bind BIND_CB

namespace {

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

class inputkb_dinput : public inputkb {
	HMODULE hDInput;
	LPDIRECTINPUTDEVICE8 keyboard;
public:
	bool construct(uintptr_t windowhandle)
	{
		this->keyboard=NULL;
		
		this->hDInput=LoadLibrary("dinput8.dll");
		if (!this->hDInput) return false;
		HRESULT (WINAPI * lpDirectInput8Create)(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID * ppvOut, LPUNKNOWN punkOuter);
		//lpDirectInput8Create=DirectInput8Create;
		lpDirectInput8Create=(HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN))GetProcAddress(this->hDInput, "DirectInput8Create");
		if (!lpDirectInput8Create) return false;
		
		LPDIRECTINPUT8 input;
		lpDirectInput8Create(GetModuleHandle(NULL), 0x0800, IID_IDirectInput8, (void**)&input, 0);
		if (!input) return false;
		
		input->CreateDevice(GUID_SysKeyboard, &this->keyboard, 0);
		input->Release();
		if (!this->keyboard) return false;
		
		if (FAILED(this->keyboard->SetDataFormat(&c_dfDIKeyboard))) return false;
		if (FAILED(this->keyboard->SetCooperativeLevel(GetParent((HWND)windowhandle), DISCL_NONEXCLUSIVE | DISCL_FOREGROUND))) return false;
		
		return true;
	}
	
	~inputkb_dinput()
	{
		if (this->keyboard) this->keyboard->Release();
		if (this->hDInput) FreeLibrary(this->hDInput);
	}
	
	uint32_t features() { return f_public|f_pollable; }
	
	void refresh() { poll(); }
	
	void poll()
	{
		unsigned char keys[256];
		if (FAILED(this->keyboard->GetDeviceState(256, keys)))
		{
			this->keyboard->Acquire();
			if (FAILED(this->keyboard->GetDeviceState(256, keys))) return;
		}
		for (unsigned int i=0;i<256;i++)
		{
			this->key_cb(0, i, inputkb_translate_scan(i), keys[i]);
		}
	}
};

}

inputkb* inputkb_create_directinput(uintptr_t windowhandle)
{
	inputkb_dinput* ret=new inputkb_dinput();
	if (!ret->construct(windowhandle))
	{
		delete ret;
		return NULL;
	}
	return ret;
}
#endif
