#include "io.h"
//Known bugs:
//RawInput cannot query device state; if anything is pressed on app boot, it will not be recorded.
// However, Windows tends to repeat a key when it is held down; these repeats will be recorded.
// (Though only one key is repeated.)
//Holding a key and unplugging its keyboard will make that key perpetually held, unless the keyboard
// is reinserted.
//
//Possible bugs:
// If multiple instances of this structure are created.
// The RawInput notifications may or may not stop once the window is destroyed.
// If this structure is deleted and recreated.
// If Joy2Key is in use.

//this file is heavily based on ruby by byuu

//TODO: http://molecularmusings.wordpress.com/2011/09/05/properly-handling-keyboard-input/
//TODO: GetKeyNameText may be useful for debugging

#ifdef INPUT_DIRECTINPUT
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#undef bind
#include <windows.h>
#define bind BIND_CB
#include <stdlib.h>
#include "libretro.h"

//guess whether the mingw headers include these
//I don't use all of them, but no reason not to include them.
#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC 0
#endif
#ifndef MAPVK_VSC_TO_VK
#define MAPVK_VSC_TO_VK 1
#endif
#ifndef MAPVK_VK_TO_CHAR
#define MAPVK_VK_TO_CHAR 2
#endif
#ifndef MAPVK_VSC_TO_VK_EX
#define MAPVK_VSC_TO_VK_EX 3
#endif
#ifndef MAPVK_VK_TO_VSC_EX
#define MAPVK_VK_TO_VSC_EX 4
#endif

#ifndef VK_OEM_PLUS
#define VK_OEM_PLUS 0xBB
#endif
#ifndef VK_OEM_COMMA
#define VK_OEM_COMMA 0xBC
#endif
#ifndef VK_OEM_MINUS
#define VK_OEM_MINUS 0xBD
#endif
#ifndef VK_OEM_PERIOD
#define VK_OEM_PERIOD 0xBE
#endif

namespace {

class inputkb_rawinput : public inputkb {
	HWND hwnd;
	
	int numkb;
	HANDLE * kbhandle;
	char* * kbnames;
	
public:
	void handle_event(RAWINPUT* input);
	
public:
	static const uint32_t features = f_multi|f_delta|f_auto|f_direct|f_public|f_background;
	
	inputkb_rawinput(uintptr_t windowhandle);
	~inputkb_rawinput();
	
	//void refresh(); // we cannot poll the device
	//void poll(); // we do this through the windows main loop
};

void inputkb_rawinput::handle_event(RAWINPUT* input)
{
	if (input->header.dwType==RIM_TYPEKEYBOARD)//unneeded check, we only ask for keyboard; could be relevant later, though.
	{
		int deviceid;
		for (deviceid=0;deviceid<this->numkb;deviceid++)
		{
			if (input->header.hDevice==this->kbhandle[deviceid]) break;
		}
		if (deviceid==this->numkb)
		{
			unsigned int size=0;
			GetRawInputDeviceInfo(input->header.hDevice, RIDI_DEVICENAME, NULL, &size);
			char * newname=malloc(size+1);
			GetRawInputDeviceInfo(input->header.hDevice, RIDI_DEVICENAME, newname, &size);
			newname[size]='\0';
//when adding item 'A' to list:
// query name of A
// for each item 'B':
//  if B has same name as A:
//   pull device list 'L' if it doesn't exist
//   if handle of B does not exist in L:
//    set handle of B to handle of A
//    clear state of B
//    return
// if not returned, add item
//#error see above
			
			for (int i=0;i<this->numkb;i++)
			{
				if (!strcmp(this->kbnames[i], newname))
				{
					this->kbhandle[i]=input->header.hDevice;
					goto handle;
				}
			}
			
			this->numkb++;
			
			this->kbhandle=realloc(this->kbhandle, sizeof(HANDLE)*this->numkb);
			this->kbhandle[this->numkb-1]=input->header.hDevice;
			
			this->kbnames=realloc(this->kbnames, sizeof(char*)*this->numkb);
			this->kbnames[this->numkb-1]=newname;
		}
	handle:
		
		USHORT scan=input->data.keyboard.MakeCode;
		USHORT flags=input->data.keyboard.Flags;
		USHORT vkey=input->data.keyboard.VKey;
		
//printf("%.4X %.4X %.4X\n",scan,flags,vkey);
		//mostly copypasta from http://molecularmusings.wordpress.com/2011/09/05/properly-handling-keyboard-input/
		//ignore vkey=0xFF scan=0x45, we get one with vkey=VK_PAUSE scan=0x1D at the same time which we do handle.
		if (vkey!=0xFF)
		{
			if (vkey==VK_SHIFT) vkey=MapVirtualKey(scan, MAPVK_VSC_TO_VK_EX);
			if (vkey==VK_NUMLOCK) scan=MapVirtualKey(vkey, MAPVK_VK_TO_VSC) | 0x100;
			
			if (flags&RI_KEY_E1)
			{
				if (vkey==VK_PAUSE) scan=0x45;
				else scan=MapVirtualKey(vkey, MAPVK_VK_TO_VSC);
			}
			if (flags&RI_KEY_E0)
			{
				if (vkey==VK_CONTROL) vkey=VK_RCONTROL;
				if (vkey==VK_MENU) vkey=VK_RMENU;
				if (vkey==VK_RETURN) vkey=0xFF;//there is no VK_ numpad enter
			}
			else
			{
				if (vkey==VK_CONTROL) vkey=VK_LCONTROL;
				if (vkey==VK_MENU) vkey=VK_LMENU;
				
				if (vkey==VK_INSERT) vkey=VK_NUMPAD0;
				if (vkey==VK_DELETE) vkey=VK_DECIMAL;
				if (vkey==VK_HOME) vkey=VK_NUMPAD7;
				if (vkey==VK_END) vkey=VK_NUMPAD1;
				if (vkey==VK_PRIOR) vkey=VK_NUMPAD9;
				if (vkey==VK_NEXT) vkey=VK_NUMPAD3;
				if (vkey==VK_LEFT) vkey=VK_NUMPAD4;
				if (vkey==VK_RIGHT) vkey=VK_NUMPAD6;
				if (vkey==VK_UP) vkey=VK_NUMPAD8;
				if (vkey==VK_DOWN) vkey=VK_NUMPAD2;
				if (vkey==VK_CLEAR) vkey=VK_NUMPAD5;
			}
			
			//TODO: figure out if happy dude key can be sent here while ignoring fake keys from SysRq/Pause/Ins/Del/etc.
			this->key_cb(deviceid, scan, inputkb_translate_vkey(vkey), !(flags&RI_KEY_BREAK));
		}
		
#ifdef DEBUG
//printf("KEY key=%.4X fl=%.4X res=%.4X vk=%.4X ms=%.8X exi=%.8lX\n",
//input->data.keyboard.MakeCode,input->data.keyboard.Flags,input->data.keyboard.Reserved,
//input->data.keyboard.VKey,input->data.keyboard.Message,input->data.keyboard.ExtraInformation);
//printf("KEY key=%.4X                  vk=%.4X\n",scan,vkey);
#endif
	}
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg==WM_INPUT)
	{
		UINT size=0;
		GetRawInputData((HRAWINPUT)lparam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
		char * data=malloc(size);
		GetRawInputData((HRAWINPUT)lparam, RID_INPUT, data, &size, sizeof(RAWINPUTHEADER));
		
		inputkb_rawinput* obj=(inputkb_rawinput*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
		obj->handle_event((RAWINPUT*)data);
		
		LRESULT result=DefRawInputProc((RAWINPUT**)&data, size, sizeof(RAWINPUTHEADER));
		free(data);
		return result;
	}
	
	//unsupported on XP
	//TODO: do it anyways
	//if (msg==WM_INPUT_DEVICE_CHANGE)
	//{
	//}
	
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

inputkb_rawinput::~inputkb_rawinput()
{
	DestroyWindow(this->hwnd);
	free(this->kbhandle);
	
	for (int i=0;i<this->numkb;i++) free(this->kbnames[i]);
	free(this->kbnames);
}

inputkb_rawinput::inputkb_rawinput(uintptr_t windowhandle)
{
	WNDCLASS wc;
	wc.style=0;
	wc.lpfnWndProc=window_proc;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=GetModuleHandle(NULL);
	wc.hIcon=LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));
	wc.hCursor=LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground=GetSysColorBrush(COLOR_3DFACE);
	wc.lpszMenuName=NULL;
	wc.lpszClassName="RawInputClass";
	RegisterClass(&wc);//this could fail if it's already regged, but in that case, the previous registration remains so who cares.
	
	this->hwnd=CreateWindow("RawInputClass", "RawInputClass", WS_POPUP, 0, 0, 64, 64, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);
	SetWindowLongPtr(this->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	
	this->numkb=0;
	this->kbhandle=NULL;
	this->kbnames=NULL;
	
	//this->i.keyboard_get_map((struct inputkb*)this, &this->keycode_to_libretro, NULL);
	
	
	//we could list the devices with GetRawInputDeviceList and GetRawInputDeviceInfo, but we don't
	// need to; we use their handles only (plus the name, to detect unplug and replug)
	//TODO: do it anyways, to force them to be in the same order every time
	
	RAWINPUTDEVICE device[1];
	//capture all keyboard input
	device[0].usUsagePage=1;
	device[0].usUsage=6;
	device[0].dwFlags=RIDEV_INPUTSINK/*|RIDEV_DEVNOTIFY*/;
	device[0].hwndTarget=this->hwnd;
	RegisterRawInputDevices(device, 1, sizeof(RAWINPUTDEVICE));
}

static inputkb* inputkb_create_rawinput(uintptr_t windowhandle)
{
	return new inputkb_rawinput(windowhandle);
}

}

extern const driver_inputkb inputkb_rawinput_desc={ "RawInput", inputkb_create_rawinput, inputkb_rawinput::features };
#endif
