#include "minir.h"
//This one uses GDK for listing the devices, as well as notifications on devices being attached and
// detached; since we don't have access to the X event loop, we need to ask the one that does.
//It uses XInput directly for the actual device access. Due to the lack of event loop, it also polls
// the entire device each frame, instead of checking for changes; I don't like allocations that run
// each frame, but it seems unavoidable here.
//(Using GDK entirely for the input could work, but I can't find any dang way to query a GdkDevice at all.)
#ifdef INPUT_XINPUT2
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
//#include <X11/extensions/XKB.h>
#include <stdlib.h>
#include <string.h>
#include "libretro.h"
#ifdef WINDOW_GTK3
#include <gdk/gdkx.h>
#endif

struct inputraw_xinput2 {
	struct inputraw i;
	
	Display* display;
	Window windowhandle;
	
#ifdef WINDOW_GTK3
	GdkDisplay* gdkdisplay;
#endif
	
	int numdevices;
	int numvaliddevices;
	int* deviceids;
	XDevice* * devices;
};

static bool keyboard_poll(struct inputraw * this_, unsigned int kb_id, unsigned char * keys);

static unsigned int keyboard_num_keyboards(struct inputraw * this_)
{
	struct inputraw_xinput2 * this=(struct inputraw_xinput2*)this_;
	unsigned char state[256];
	for (int i=this->numvaliddevices;i<this->numdevices;i++)
	{
		if (!keyboard_poll(this_, i, state)) continue;
		for (int j=0;j<256;j++)
		{
			if (state[j])
			{
				int deviceid=this->deviceids[this->numvaliddevices];
				XDevice* device=this->devices[this->numvaliddevices];
				this->deviceids[this->numvaliddevices]=this->deviceids[i];
				this->devices[this->numvaliddevices]=this->devices[i];
				this->deviceids[i]=deviceid;
				this->devices[i]=device;
				this->numvaliddevices++;
				break;
			}
		}
	}
	if (this->numvaliddevices) return this->numvaliddevices;
	else return 1;
}

static bool keyboard_poll(struct inputraw * this_, unsigned int kb_id, unsigned char * keys)
{
	struct inputraw_xinput2 * this=(struct inputraw_xinput2*)this_;
	if (kb_id>=this->numdevices) return false;
	
#ifdef WINDOW_GTK3
	gdk_x11_display_error_trap_push(this->gdkdisplay);
#endif
	XDeviceState * state=XQueryDeviceState(this->display, this->devices[kb_id]);
#ifdef WINDOW_GTK3
	gdk_x11_display_error_trap_pop_ignored(this->gdkdisplay);
#endif
	if (!state) return false;
	
	XInputClass * cls=state->data;
	for (int j=0;j<state->num_classes;j++)
	{
		if (cls->class==KeyClass)
		{
			XKeyState * key_state=(XKeyState*)cls;
			memset(keys+key_state->num_keys, 0, 256-key_state->num_keys);
			for (int k=0;k<key_state->num_keys;k++)
			{
				keys[k]=((key_state->keys[k / 8] & (1 << (k % 8))));
			}
			break;
		}
		cls=(XInputClass*)((char*)cls + cls->length);
	}
	XFreeDeviceState(state);
	return true;
}

//static void recreate_devices(struct inputraw_xinput2 * this, bool delete)
//{
//	int ndevices=0;
//	XIDeviceInfo * devices=XIQueryDevice(this->display, XIAllDevices, &ndevices);
//	
//	int numdevices=0;
//	bool forceswitch=false;
//	
//	if (!delete)
//	{
//		for (int i=0;i<ndevices;i++)
//		{
//			if (devices[i].enabled && (devices[i].use==XISlaveKeyboard || devices[i].use==XIFloatingSlave))
//			{
//				if (numdevices>=this->numdevices || devices[i].deviceid!=this->deviceids[numdevices]) forceswitch=true;
//				numdevices++;
//			}
//		}
//	}
//	
//	if (numdevices!=this->numdevices || forceswitch)
//	{
//		for (int i=0;i<this->numdevices;i++)
//		{
//			bool delete=false;
//			for (int j=0;j<ndevices;j++)
//			{
//				if (this->deviceids[i]==devices[j].deviceid) delete=true;
//			}
//			if (delete) XCloseDevice(this->display, this->devices[i]);
//		}
//		
//		free(this->deviceids);
//		free(this->devices);
//		this->deviceids=malloc(sizeof(int)*numdevices);
//		this->devices=malloc(sizeof(XDevice*)*numdevices);
//		
//		if (!delete)
//		{
//			numdevices=0;
//			for (int i=0;i<ndevices;i++)
//			{
//				if (devices[i].enabled && (devices[i].use==XISlaveKeyboard || devices[i].use==XIFloatingSlave))
//				{
//					XDevice* device=XOpenDevice(this->display, devices[i].deviceid);
//					this->deviceids[numdevices]=devices[i].deviceid;
//					this->devices[numdevices]=device;
//					numdevices++;
//				}
//			}
//		}
//		
//		this->numdevices=numdevices;
//	}
//	
//	XIFreeDeviceInfo(devices);
//}

#ifdef WINDOW_GTK3
static void add_device_gdk(GdkDeviceManager* object, GdkDevice* device, void * this_)
{
	struct inputraw_xinput2 * this=(struct inputraw_xinput2*)this_;
	if (gdk_device_get_source(device)!=GDK_SOURCE_KEYBOARD) return;
	
	int deviceid=gdk_x11_device_get_id(device);
	
	for (int i=0;i<this->numdevices;i++)
	{
		if (this->deviceids[i]==deviceid) return;//dupes not allowed
	}
	
	this->numdevices++;
	this->devices=realloc(this->devices, sizeof(XDevice*)*this->numdevices);
	this->deviceids=realloc(this->deviceids, sizeof(int*)*this->numdevices);
	
	//If this fails, we're very likely to get it deleted soon. Worst case, it'll emit no events.
#ifdef WINDOW_GTK3
	gdk_x11_display_error_trap_push(this->gdkdisplay);
#endif
	this->devices[this->numdevices-1]=XOpenDevice(this->display, deviceid);
#ifdef WINDOW_GTK3
	gdk_x11_display_error_trap_pop_ignored(this->gdkdisplay);
#endif
	
	this->deviceids[this->numdevices-1]=deviceid;
}

static void remove_device_gdk(GdkDeviceManager* object, GdkDevice* device, void * this_)
{
	struct inputraw_xinput2 * this=(struct inputraw_xinput2*)this_;
	if (gdk_device_get_source(device)!=GDK_SOURCE_KEYBOARD) return;
	int deviceid=gdk_x11_device_get_id(device);
	for (int i=0;i<this->numdevices;i++)
	{
		if (this->deviceids[i]==deviceid)
		{
			//this thing seems to get corrupted upon being unplugged; leak is the only option. I can only find
			// 2*32 bytes per unplug anyways, and unplugging stuff that often means you have bigger problems.
			//I could also use 'wrong' free() function, but that could also blow up. Leak is the least bad.
			//XCloseDevice(this->display, this->devices[i]);
			
			memmove(this->devices+i, this->devices+i+1, sizeof(XDevice*)*(this->numdevices-i-1));
			memmove(this->deviceids+i, this->deviceids+i+1, sizeof(int)*(this->numdevices-i-1));
			this->numdevices--;
			if (i<this->numvaliddevices) this->numvaliddevices--;
			break;
		}
	}
}
#endif

static void free_(struct inputraw * this_)
{
	struct inputraw_xinput2 * this=(struct inputraw_xinput2*)this_;
	
#ifdef WINDOW_GTK3
	GdkDeviceManager* devicemanager=gdk_display_get_device_manager(this->gdkdisplay);
	g_signal_handlers_disconnect_by_func(devicemanager, add_device_gdk, this);
	g_signal_handlers_disconnect_by_func(devicemanager, remove_device_gdk, this);
#endif
	
	//we can probably be reasonably sure that devices aren't disconnected at exactly this point, but
	// the massively async nature of X does make things quite complex...
#ifdef WINDOW_GTK3
	gdk_x11_display_error_trap_push(this->gdkdisplay);
#endif
	for (int i=0;i<this->numdevices;i++)
	{
		XCloseDevice(this->display, this->devices[i]);
	}
#ifdef WINDOW_GTK3
	gdk_x11_display_error_trap_pop_ignored(this->gdkdisplay);
#endif
	this->numdevices=0;
	
	free(this->deviceids);
	free(this->devices);
	free(this);
}

struct inputraw * _inputraw_create_xinput2(uintptr_t windowhandle)
{
	struct inputraw_xinput2 * this=malloc(sizeof(struct inputraw_xinput2));
	_inputraw_x11_keyboard_create_shared((struct inputraw*)this);
	this->i.keyboard_num_keyboards=keyboard_num_keyboards;
	//this->i.keyboard_num_keys=keyboard_num_keys;
	this->i.keyboard_poll=keyboard_poll;
	//this->i.keyboard_get_map=keyboard_get_map;
	this->i.free=free_;
	
	this->display=(Display*)window_x11_get_display()->display;
	this->windowhandle=(Window)windowhandle;
	this->numdevices=0;
	this->numvaliddevices=0;
	this->devices=NULL;
	this->deviceids=NULL;
	
	int xi_opcode;
	int event;
	int error;
	if (!XQueryExtension(this->display, "XInputExtension", &xi_opcode, &event, &error))
	{
		free(this);
		return NULL;
	}
	
#ifdef WINDOW_GTK3
	this->gdkdisplay=gdk_display_get_default();
	GdkDeviceManager* devicemanager=gdk_display_get_device_manager(this->gdkdisplay);
	g_signal_connect(devicemanager, "device-added", G_CALLBACK(add_device_gdk), this);
	g_signal_connect(devicemanager, "device-removed", G_CALLBACK(remove_device_gdk), this);
	
	GdkDeviceType types[2]={ GDK_DEVICE_TYPE_SLAVE, GDK_DEVICE_TYPE_FLOATING };
	for (int i=0;i<2;i++)
	{
		GList* devices=gdk_device_manager_list_devices(devicemanager, types[i]);
		GList* list=devices;
		while (list)
		{
			GdkDevice* device=GDK_DEVICE(list->data);
			if (gdk_device_get_source(device)==GDK_SOURCE_KEYBOARD)
			{
				add_device_gdk(NULL, device, this);
			}
			list=list->next;
		}
		g_list_free(devices);
	}
#else
#error fill this in
#endif
	
	//recreate_devices(this, false);
//free_((struct inputraw*)this);
//exit(0);
	
	return (struct inputraw*)this;
}
#endif
