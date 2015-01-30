#include "io.h"
//This one uses GDK for listing the devices, as well as notifications on devices being attached and
// detached; since we don't have access to the X event loop, we need to ask the one that does.
//It uses XInput directly for the actual device access. Due to the lack of event loop, it also polls
// the entire device each frame, instead of checking for changes; I don't like allocations that run
// each frame, but it seems unavoidable here.
#ifdef INPUT_XINPUT2
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
//#include <X11/extensions/XKB.h>
#include <stdlib.h>
#include <string.h>
#include "libretro.h"
#ifdef WINDOW_GTK3
#include <gdk/gdkx.h>
#else
#error fill this in
#endif

namespace {

class inputkb_xinput2 : public inputkb {
	Display* display;
	Window windowhandle;
	
#ifdef WINDOW_GTK3
	GdkDisplay* gdkdisplay;
#endif
	
	unsigned int numdevices;
	unsigned int numvaliddevices;
	XID * deviceids;
	
#ifdef WINDOW_GTK3
	static void add_device_gdk_raw(GdkDeviceManager* device_manager, GdkDevice* device, gpointer user_data)
	{
		inputkb_xinput2* obj=(inputkb_xinput2*)user_data;
		obj->add_device_gdk(device_manager, device);
	}
	
	void add_device_gdk(GdkDeviceManager* device_manager, GdkDevice* device)
	{
		if (gdk_device_get_source(device)!=GDK_SOURCE_KEYBOARD) return;
		
		XID deviceid=gdk_x11_device_get_id(device);
		for (unsigned int i=0;i<(unsigned int)this->numdevices;i++)
		{
			if (this->deviceids[i]==deviceid) return;//dupes not allowed
		}
		
		this->numdevices++;
		this->deviceids=realloc(this->deviceids, sizeof(int*)*this->numdevices);
		
		//If this fails, we're very likely to get it deleted soon. Worst case, it'll emit no events.
#ifdef WINDOW_GTK3
		//gdk_x11_display_error_trap_push(this->gdkdisplay);
#endif
		//this->devices[this->numdevices-1]=XOpenDevice(this->display, deviceid);
#ifdef WINDOW_GTK3
		//gdk_x11_display_error_trap_pop_ignored(this->gdkdisplay);
#endif
		
		this->deviceids[this->numdevices-1]=deviceid;
	}
	
	static void remove_device_gdk_raw(GdkDeviceManager* device_manager, GdkDevice* device, gpointer user_data)
	{
		inputkb_xinput2* obj=(inputkb_xinput2*)user_data;
		obj->remove_device_gdk(device_manager, device);
	}
	
	void remove_device_gdk(GdkDeviceManager* device_manager, GdkDevice* device)
	{
		if (gdk_device_get_source(device)!=GDK_SOURCE_KEYBOARD) return;
		int deviceid=gdk_x11_device_get_id(device);
		for (unsigned int i=0;i<this->numdevices;i++)
		{
			if (this->deviceids[i]==deviceid)
			{
				//memmove(this->devices+i, this->devices+i+1, sizeof(XDevice*)*(this->numdevices-i-1));
				memmove(this->deviceids+i, this->deviceids+i+1, sizeof(int)*(this->numdevices-i-1));
				this->numdevices--;
				if (i<this->numvaliddevices) this->numvaliddevices--;
				break;
			}
		}
	}
#endif
	
public:
	static const uint32_t feat = f_multi|f_public|f_pollable|f_remote;
	uint32_t features() { return feat; }
	
	inputkb_xinput2(uintptr_t windowhandle)
	{
		this->display=window_x11.display;
		this->windowhandle=(Window)windowhandle;
		this->numdevices=0;
		this->numvaliddevices=0;
		//this->devices=NULL;
		this->deviceids=NULL;
		
#ifdef WINDOW_GTK3
		this->gdkdisplay=gdk_display_get_default();
		GdkDeviceManager* devicemanager=gdk_display_get_device_manager(this->gdkdisplay);
		g_signal_connect(devicemanager, "device-added", G_CALLBACK(add_device_gdk_raw), this);
		g_signal_connect(devicemanager, "device-removed", G_CALLBACK(remove_device_gdk_raw), this);
		
		for (unsigned int i=0;i<2;i++)
		{
			GdkDeviceType types[2]={ GDK_DEVICE_TYPE_SLAVE, GDK_DEVICE_TYPE_FLOATING };
			GList* devices=gdk_device_manager_list_devices(devicemanager, types[i]);
			GList* list=devices;
			while (list)
			{
				GdkDevice* device=GDK_DEVICE(list->data);
				if (gdk_device_get_source(device)==GDK_SOURCE_KEYBOARD)
				{
					add_device_gdk(NULL, device);
				}
				list=list->next;
			}
			g_list_free(devices);
		}
#endif
		
		//recreate_devices(this, false);
	//free_((struct inputraw*)this);
	//exit(0);
	}
	
	void refresh() { poll(); }
	
	void poll()
	{
		for (unsigned int kb_id=0;kb_id<this->numdevices;kb_id++)
		{
			//http://cgit.freedesktop.org/xorg/lib/libXi/tree/src/XQueryDv.c uses only device_id, but it could change without telling...
			//but what can I do when XOpenDevice/XCloseDevice aren't refcounted?
			XDevice device={this->deviceids[kb_id], 0, NULL};
#ifdef WINDOW_GTK3
			gdk_x11_display_error_trap_push(this->gdkdisplay);
#endif
			XDeviceState * state=XQueryDeviceState(this->display, &device);
#ifdef WINDOW_GTK3
			gdk_x11_display_error_trap_pop_ignored(this->gdkdisplay);
#endif
			if (!state) return;
			
			XInputClass* cls=state->data;
			for (unsigned int cl=0;cl<(unsigned int)state->num_classes;cl++)
			{
				if (cls->c_class==KeyClass)
				{
					XKeyState* key_state=(XKeyState*)cls;
					for (unsigned int key=0;key<key_state->num_keys;key++)
					{
						bool down=key_state->keys[key / 8] & (1 << (key % 8));
						if (down && kb_id>=this->numvaliddevices)
						{
							XID deviceid=this->deviceids[this->numvaliddevices];
							this->deviceids[this->numvaliddevices]=this->deviceids[kb_id];
							this->deviceids[kb_id]=deviceid;
							this->numvaliddevices++;
						}
						if (kb_id<this->numvaliddevices) key_cb(kb_id, key, inputkb::translate_scan(key), down);
					}
					break;
				}
				cls=(XInputClass*)((char*)cls + cls->length);
			}
			XFreeDeviceState(state);
		}
	}
	
	~inputkb_xinput2()
	{
#ifdef WINDOW_GTK3
		GdkDeviceManager* devicemanager=gdk_display_get_device_manager(this->gdkdisplay);
		g_signal_handlers_disconnect_by_func(devicemanager, (gpointer)add_device_gdk_raw, this);
		g_signal_handlers_disconnect_by_func(devicemanager, (gpointer)remove_device_gdk_raw, this);
#endif
		
		//we can probably be reasonably sure that devices aren't disconnected at exactly this point, but
		// the massively async nature of X does make things quite complex...
#ifdef WINDOW_GTK3
		//gdk_x11_display_error_trap_push(this->gdkdisplay);
#endif
		//for (unsigned int i=0;i<this->numdevices;i++)
		//{
		//	XCloseDevice(this->display, this->devices[i]);
		//}
#ifdef WINDOW_GTK3
		//gdk_x11_display_error_trap_pop_ignored(this->gdkdisplay);
#endif
		this->numdevices=0;
		
		free(this->deviceids);
		//free(this->devices);
	}
};

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

static inputkb* inputkb_create_xinput2(uintptr_t windowhandle)
{
	int xi_opcode;
	int event;
	int error;
	if (!XQueryExtension(window_x11.display, "XInputExtension", &xi_opcode, &event, &error)) return NULL;
	
	return new inputkb_xinput2(windowhandle);
}

}

const inputkb::driver inputkb::driver_xinput2={ "XInput2", inputkb_create_xinput2, inputkb_xinput2::feat };
#endif
