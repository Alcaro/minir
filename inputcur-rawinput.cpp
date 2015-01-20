#include "io.h"

#ifdef INPUTCUR_RAWINPUT
#undef bind
#include <windows.h>
#define bind bind_func
#ifndef RIDEV_DEVNOTIFY
#define RIDEV_DEVNOTIFY 0x2000
#endif
#ifndef WM_INPUT_DEVICE_CHANGE
#define WM_INPUT_DEVICE_CHANGE 0xFE
#endif

namespace {
class inputcursor_rawinput : public inputcursor {
public:

HWND hwnd;

/*private*/ bool construct(uintptr_t windowhandle)
{
	this->hwnd=CreateWindow("minir", "minir", WS_POPUP, 0, 0, 64, 64, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);
	SetWindowLongPtr(this->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SetWindowLongPtr(this->hwnd, GWLP_WNDPROC, (LONG_PTR)wndproc);
	
	RAWINPUTDEVICE device;
	device.usUsagePage=1;
	device.usUsage=2;//not even any #defines for those. crazy
	device.dwFlags=RIDEV_INPUTSINK/*|RIDEV_DEVNOTIFY*/; // no use for DEVNOTIFY
	device.hwndTarget=this->hwnd;
	bool ok=RegisterRawInputDevices(&device, 1, sizeof(RAWINPUTDEVICE));
	//if (!ok)
	//{
	//	device.dwFlags&=~RIDEV_DEVNOTIFY;//disable this because XP
	//	ok=RegisterRawInputDevices(&device, 1, sizeof(RAWINPUTDEVICE));
	//}
	if (!ok)
	{
		DestroyWindow(this->hwnd);
		this->hwnd=NULL;
		return false;
	}
	return true;
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg==WM_INPUT)
	{
		UINT size=0;
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
		RAWINPUT* data=malloc(size);
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, data, &size, sizeof(RAWINPUTHEADER));
		
		((inputcursor_rawinput*)GetWindowLongPtr(hwnd, GWLP_USERDATA))->event(data);
		
		LRESULT result=DefRawInputProc((RAWINPUT**)&data, size, sizeof(RAWINPUTHEADER));
		free(data);
		return result;
		
		//((inputcursor_rawinput*)GetWindowLongPtr(hwnd, GWLP_USERDATA))->event(NULL);
		//fall out to DefWindowProc
	}
	if (uMsg==WM_INPUT_DEVICE_CHANGE)
	{
		//not needed - commented out so it doesn't call GetWindowLongPtr
		
		//wParam can be GIDC_ARRIVAL = 1 or GIDC_REMOVAL = 2 (weird names, why not ADDED/REMOVED?), so we can just use the lowest bit.
		//((inputcursor_rawinput*)GetWindowLongPtr(hwnd, GWLP_USERDATA))->event_dev(wParam&1, (HANDLE)lParam);
		//fall out to DefWindowProc
	}
	
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/*private*/ void event(RAWINPUT* ev)
{
	if (ev->header.dwType==RIM_TYPEMOUSE)
	{
		if (ev->data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE)
		{
			this->move_cb(0, ev->data.mouse.lLastX, ev->data.mouse.lLastY);
		}
		else refresh_pos();
		
		if (ev->data.mouse.usButtonFlags & 0x3FF)
		{
			for (unsigned int i=0;i<5;i++)
			{
				if (ev->data.mouse.usButtonFlags & (3 << (i*2)))
				{
					this->button_cb(0, i, ev->data.mouse.usButtonFlags & (1 << (i*2)));
				}
			}
		}
	}
}

/*private*/ void event_dev(bool add, HANDLE device)
{
	//no multi-mouse support - do nothing
}

/*private*/ void refresh_pos()
{
	POINT pt;
	GetCursorPos(&pt);
	this->move_cb(0, pt.x, pt.y);
}

/*private*/ void refresh_btn()
{
	this->button_cb(0, button::left,   GetAsyncKeyState(VK_LBUTTON)&0x8000);
	this->button_cb(0, button::right,  GetAsyncKeyState(VK_RBUTTON)&0x8000);
	this->button_cb(0, button::middle, GetAsyncKeyState(VK_MBUTTON)&0x8000);
	this->button_cb(0, button::x4,     GetAsyncKeyState(VK_XBUTTON1)&0x8000);
	this->button_cb(0, button::x5,     GetAsyncKeyState(VK_XBUTTON2)&0x8000);
}

void refresh()
{
	refresh_pos();
	refresh_btn();
}

void poll() {}

void move(unsigned int mouse, signed int x, signed int y)
{
}

void grab(unsigned int cursor, mode::grabmode mode)
{
}

~inputcursor_rawinput()
{
	if (this->hwnd) DestroyWindow(this->hwnd);
}

};

inputcursor* inputcursor_create_rawinput(uintptr_t windowhandle)
{
	inputcursor_rawinput* ret=new inputcursor_rawinput;
	if (!ret->construct(windowhandle))
	{
		delete ret;
		return NULL;
	}
	return ret;
}

};

const inputcursor::driver inputcursor::driver_rawinput={ "RawInput", inputcursor_create_rawinput, 0 };
#endif
