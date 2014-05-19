#include "minir.h"
//Known bugs:
//RawInput cannot query device state; if anything is pressed on app boot, it will not be recorded.
// However, Windows tends to repeat a key when it is held down; these repeats will be recorded.
// (Though only one key is repeated.)
//Holding a key and unplugging its keyboard will make that key perpetually held, unless the keyboard
// is reinserted.
//
//It is unknown if:
// Bugs appear if multiple instances of this structure are created
// The RawInput notifications are deleted once the window is destroyed
// Things work properly if this structure is deleted and recreated

//this file is heavily based on ruby by byuu

#ifdef INPUT_DIRECTINPUT
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <stdlib.h>
#include "libretro.h"

struct inputraw_rawinput {
	struct inputraw i;
	
	HWND hwnd;
	
	int numkb;
	HANDLE * kbhandle;
	char * * kbnames;
	unsigned char * kbstate;
	
	//this->i.keyboard_get_map
	const unsigned int * keycode_to_libretro;
};

static unsigned int keyboard_num_keyboards(struct inputraw * this_)
{
	struct inputraw_rawinput * this=(struct inputraw_rawinput*)this_;
	if (this->numkb) return this->numkb;
	else return 1;
}

static bool keyboard_poll(struct inputraw * this_, unsigned int kb_id, unsigned char * keys)
{
	struct inputraw_rawinput * this=(struct inputraw_rawinput*)this_;
	if (kb_id>=this->numkb) return false;
	memcpy(keys, this->kbstate + 256*kb_id, 256);
	return true;
}

static void free_(struct inputraw * this_)
{
	struct inputraw_rawinput * this=(struct inputraw_rawinput*)this_;
	
	DestroyWindow(this->hwnd);
	free(this->kbhandle);
	free(this->kbstate);
	
	for (int i=0;i<this->numkb;i++) free(this->kbnames[i]);
	free(this->kbnames);
	
	free(this);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg==WM_INPUT)
	{
		UINT size=0;
		GetRawInputData((HRAWINPUT)lparam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
		char data[size];
		GetRawInputData((HRAWINPUT)lparam, RID_INPUT, data, &size, sizeof(RAWINPUTHEADER));
		
		RAWINPUT* input=(RAWINPUT*)data;
		if (input->header.dwType==RIM_TYPEKEYBOARD)//unneeded check, we only ask for keyboard; could be relevant later, though.
		{
//printf(" Kbd: make=%04x Flags:%04x Reserved:%04x ExtraInformation:%08x, msg=%04x VK=%04x \n",
//            input->data.keyboard.MakeCode, 
//            input->data.keyboard.Flags, 
//            input->data.keyboard.Reserved, 
//            input->data.keyboard.ExtraInformation, 
//            input->data.keyboard.Message, 
//            input->data.keyboard.VKey);
			struct inputraw_rawinput * this=(struct inputraw_rawinput*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
			int deviceid;
			for (deviceid=0;deviceid<this->numkb;deviceid++)
			{
				if (input->header.hDevice==this->kbhandle[deviceid]) break;
			}
			if (deviceid==this->numkb)
			{
				unsigned int size=0;
				GetRawInputDeviceInfo(input->header.hDevice, RIDI_DEVICENAME, NULL, &size);
				char * newname=malloc(size)+1;
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
						memset(this->kbstate + 256*i, 0, 256);
						goto done;
					}
				}
				
				this->numkb++;
				
				this->kbhandle=realloc(this->kbhandle, sizeof(HANDLE)*this->numkb);
				this->kbstate=realloc(this->kbstate, 256*this->numkb);
				this->kbnames=realloc(this->kbnames, sizeof(char*)*this->numkb);
				
				this->kbhandle[this->numkb-1]=input->header.hDevice;
				memset(this->kbstate + 256*(this->numkb-1), 0, 256);
				this->kbnames[this->numkb-1]=newname;
			}
			
			USHORT code=input->data.keyboard.MakeCode;
			USHORT flags=input->data.keyboard.Flags;
			if (code>0 && code<=255 &&
			    !(this->keycode_to_libretro[code] && input->data.keyboard.VKey==0xFF)//since Windows is Windows, it naturally emits bogus keypresses.
			    )
			{
				this->kbstate[256*deviceid + code]=!(flags&RI_KEY_BREAK);
			}
		}
		
	done:;
		LRESULT result=DefRawInputProc(&input, size, sizeof(RAWINPUTHEADER));
		return result;
	}
	
	//unsupported on XP
	//if (msg==WM_INPUT_DEVICE_CHANGE)
	//{
	//}
	
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

struct inputraw * _inputraw_create_rawinput(uintptr_t windowhandle)
{
	struct inputraw_rawinput * this=malloc(sizeof(struct inputraw_rawinput));
	_inputraw_windows_keyboard_create_shared((struct inputraw*)this);
	this->i.keyboard_num_keyboards=keyboard_num_keyboards;
	//this->i.keyboard_num_keys=keyboard_num_keys;
	this->i.keyboard_poll=keyboard_poll;
	//this->i.keyboard_get_map=keyboard_get_map;
	this->i.free=free_;
	
	WNDCLASS wc;
	wc.style=0;
	wc.lpfnWndProc=window_proc;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=GetModuleHandle(NULL);
	wc.hIcon=LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(0));
	wc.hCursor=LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground=GetSysColorBrush(COLOR_3DFACE);
	wc.lpszMenuName=NULL;
	wc.lpszClassName="RawInputClass";
	RegisterClass(&wc);//this could fail if it's already regged, but in that case, the previous registration remains so who cares.
	
	this->hwnd=CreateWindow("RawInputClass", "RawInputClass", WS_POPUP, 0, 0, 64, 64, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);
	SetWindowLongPtr(this->hwnd, GWLP_USERDATA, (LONG)this);
	
	this->numkb=0;
	this->kbhandle=NULL;
	this->kbstate=NULL;
	this->kbnames=NULL;
	
	this->i.keyboard_get_map((struct inputraw*)this, &this->keycode_to_libretro, NULL);
	
	
	//we could list the devices with GetRawInputDeviceList and GetRawInputDeviceInfo, but we don't
	// need to; we use their handles only (plus the name, to detect unplug and replug)
	
	RAWINPUTDEVICE device[1];
	//capture all keyboard input
	device[0].usUsagePage=1;
	device[0].usUsage=6;
	device[0].dwFlags=RIDEV_INPUTSINK/*|RIDEV_DEVNOTIFY*/;
	device[0].hwndTarget=this->hwnd;
	RegisterRawInputDevices(device, 1, sizeof(RAWINPUTDEVICE));
	
	return (struct inputraw*)this;
} 
#endif
