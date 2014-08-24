#include "minir.h"
#ifdef INPUT_GDK
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#ifdef WNDPROT_X11
#include <gdk/gdkx.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "libretro.h"

struct inputkb_gdk {
	struct inputkb i;
	
	GdkDisplay* display;
	GdkDeviceManager* devicemanager;
	GtkWidget* widget;//likely a GtkWindow, but we only use it as GtkWidget so let's keep it as that.
	
	unsigned int numdevices;
	//char padding[4];
	GdkDevice* * devices;
	
	void (*key_cb)(struct inputkb * subject,
	               unsigned int keyboard, int scancode, int libretrocode,
	               bool down, bool changed, void* userdata);
	void* userdata;
};

//static void device_add(GdkDeviceManager* object, GdkDevice* device, gpointer user_data)
//{
//ignore everything, because there are a LOT of bogus entries in the device list
//If I plug in three keyboards, I get both duplicates and bogus devices:
//
//  Virtual core keyboard                         id=3  [master keyboard (2)]
//    Virtual core XTEST keyboard                 id=5  [slave  keyboard (3)]
//    Power Button                                id=6  [slave  keyboard (3)]
//    Power Button                                id=7  [slave  keyboard (3)]
//    CHESEN USB Keyboard                         id=10 [slave  keyboard (3)]
//    CHESEN USB Keyboard                         id=11 [slave  keyboard (3)]
//    LITEON Technology USB Multimedia Keyboard   id=9  [slave  keyboard (3)]
//    DELL Dell USB Wired Multimedia Keyboard     id=12 [slave  keyboard (3)]
//    DELL Dell USB Wired Multimedia Keyboard     id=13 [slave  keyboard (3)]
//where especially the duplicates seem irritating to get rid of.
//Instead, we give the lowest ID to the first device to send an event.
//
//	struct inputkb_gdk * this=(struct inputkb_gdk*)user_data;
//	if (gdk_device_get_source(device)!=GDK_SOURCE_KEYBOARD) return;
//	//if (gdk_device_get_device_type(device)!=GDK_DEVICE_TYPE_SLAVE) return;//allow floating ones too - masters fall to the Virtual test
//	if (!strcasecmp(gdk_device_get_name(device), "Power Button")) return;//Power Button
//	if (!strncasecmp(gdk_device_get_name(device), "Virtual ", strlen("Virtual "))) return;//Virtual core XTEST keyboard
//	//there are probably more things to ignore...
//	for (unsigned int i=0;i<this->numdevices;i++)
//	{
//		if (!this->devices[i])
//		{
//			this->devices[i]=device;
//			return;
//		}
//	}
//	this->devices=realloc(this->devices, sizeof(GdkDevice*)*(this->numdevices+1));
//	this->devices[this->numdevices]=device;
//	this->numdevices++;
//}

static void device_remove(GdkDeviceManager* object, GdkDevice* device, gpointer user_data)
{
	struct inputkb_gdk * this=(struct inputkb_gdk*)user_data;
	for (unsigned int i=0;i<this->numdevices;i++)
	{
		if (this->devices[i]==device) this->devices[i]=NULL;
	}
}

static gboolean key_action(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	struct inputkb_gdk * this=(struct inputkb_gdk*)user_data;
	GdkDevice* device=gdk_event_get_source_device(event);
	
	if (gdk_device_get_device_type(device)==GDK_DEVICE_TYPE_MASTER) return FALSE;
	//for some reason, repeated keystrokes come from the master device, which screws up device ID assignments
	//we don't want repeats all, let's just kill them.
	
	unsigned int kb=0;
	while (kb<this->numdevices && this->devices[kb]!=device) kb++;
	if (kb==this->numdevices)
	{
		kb=0;
		while (kb<this->numdevices && this->devices[kb]) kb++;
		if (kb==this->numdevices)
		{
			this->devices=realloc(this->devices, sizeof(GdkDevice*)*(this->numdevices+1));
			this->devices[this->numdevices]=device;
			this->numdevices++;
		}
		else this->devices[kb]=device;
	}
	
	guint16 keycode;
	gdk_event_get_keycode(event, &keycode);
	
//printf("%i: %.2X %.2X\n", kb, keycode, inputkb_translate_scan(keycode));
	this->key_cb((struct inputkb*)this, kb,
	             keycode, inputkb_translate_scan(keycode),
	             (event->type==GDK_KEY_PRESS), true, this->userdata);
	return FALSE;
}

static void set_callback(struct inputkb * this_,
                         void (*key_cb)(struct inputkb * subject,
                                        unsigned int keyboard, int scancode, int libretrocode, 
                                        bool down, bool changed, void* userdata),
                         void* userdata)
{
	struct inputkb_gdk * this=(struct inputkb_gdk*)this_;
	this->key_cb=key_cb;
	this->userdata=userdata;
}

static void poll(struct inputkb * this)
{
	//do nothing - we're polled through the gtk+ main loop
}

static void free_(struct inputkb * this_)
{
	struct inputkb_gdk * this=(struct inputkb_gdk*)this_;
	g_signal_handlers_disconnect_by_data(this->widget, this);
	g_signal_handlers_disconnect_by_data(this->devicemanager, this);
	free(this->devices);
	free(this);
}

struct inputkb * inputkb_create_gdk(uintptr_t windowhandle)
{
	struct inputkb_gdk * this=malloc(sizeof(struct inputkb_gdk));
	this->i.set_callback=set_callback;
	this->i.poll=poll;
	this->i.free=free_;
	
	inputkb_translate_init();
	
#ifdef WNDPROT_X11
	this->display=gdk_x11_lookup_xdisplay(window_x11_get_display()->display);
#else
#error Fill this in.
#endif
	this->devicemanager=gdk_display_get_device_manager(this->display);
	//g_signal_connect(this->devicemanager, "device-added", G_CALLBACK(device_add), this);
	g_signal_connect(this->devicemanager, "device-removed", G_CALLBACK(device_remove), this);
	
	gdk_window_get_user_data(gdk_x11_window_lookup_for_display(this->display, windowhandle), (void**)&this->widget);
	//we probably have a GtkDrawingArea, and those can't have keyboard focus. Let's ask for the GtkWindow it is in instead.
	this->widget=gtk_widget_get_toplevel(this->widget);
	gtk_widget_add_events(this->widget, GDK_KEY_PRESS_MASK|GDK_KEY_RELEASE_MASK);
	g_signal_connect(this->widget, "key-press-event", G_CALLBACK(key_action), this);
	g_signal_connect(this->widget, "key-release-event", G_CALLBACK(key_action), this);
	
	this->numdevices=0;
	this->devices=NULL;
	
	//GdkDeviceType types[2]={ GDK_DEVICE_TYPE_SLAVE, GDK_DEVICE_TYPE_FLOATING };
	//for (int i=0;i<2;i++)
	//{
	//	GList* devices=gdk_device_manager_list_devices(this->devicemanager, types[i]);
	//	GList* list=devices;
	//	while (list)
	//	{
	//		device_add(NULL, GDK_DEVICE(list->data), this);
	//		list=list->next;
	//	}
	//	g_list_free(devices);
	//}
	
	//a GdkDevice* can be queried with:
	//XDevice dev;
	//dev.device_id=gdk_x11_device_get_id(device);
	//XDeviceState * state=XQueryDeviceState(this->display, this->devices[kb_id]);
	//if (state)
	//{
	//	XInputClass * cls=state->data;
	//	for (int j=0;j<state->num_classes;j++)
	//	{
	//		if (cls->class==KeyClass)
	//		{
	//			XKeyState * key_state=(XKeyState*)cls;
	//			memset(keys+key_state->num_keys, 0, 256-key_state->num_keys);
	//			for (int k=0;k<key_state->num_keys;k++)
	//			{
	//				keys[k]=((key_state->keys[k / 8] & (1 << (k % 8))));
	//			}
	//			break;
	//		}
	//		cls=(XInputClass*)((char*)cls + cls->length);
	//	}
	//	XFreeDeviceState(state);
	//}
	
	return (struct inputkb*)this;
}
#endif
