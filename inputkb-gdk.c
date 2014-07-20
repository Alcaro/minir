#include "minir.h"
#ifdef INPUT_GDK
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <stdlib.h>
#include <string.h>
#include "libretro.h"

struct inputkb_gdk {
	struct inputkb i;
	
	GdkDisplay* display;
	
	void (*key_cb)(struct inputkb * subject,
	               unsigned int keyboard, int scancode, int libretrocode,
	               bool down, bool changed, void* userdata);
	void* userdata;
};

static void device_add(GdkDeviceManager* object, GdkDevice* device, gpointer user_data)
{
	//struct inputkb_gdk * this=(struct inputkb_gdk*)user_data;
	
}

static void device_remove(GdkDeviceManager* object, GdkDevice* device, gpointer user_data)
{
	//struct inputkb_gdk * this=(struct inputkb_gdk*)user_data;
	
}

static gboolean key_action(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	struct inputkb_gdk * this=(struct inputkb_gdk*)user_data;
	guint16 keycode;
	gdk_event_get_keycode(event, &keycode);
	this->key_cb((struct inputkb*)this, 0/*keyboard*/,
	             keycode, inputkb_x11_translate_key(keycode),
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
	
	free(this);
}

struct inputkb * inputkb_create_gdk(uintptr_t windowhandle)
{
	struct inputkb_gdk * this=malloc(sizeof(struct inputkb_gdk));
	this->i.set_callback=set_callback;
	this->i.poll=poll;
	this->i.free=free_;
	
	inputkb_x11_translate_init();
	
	this->display=gdk_x11_lookup_xdisplay(window_x11_get_display()->display);
	GdkDeviceManager* devicemanager=gdk_display_get_device_manager(this->display);
	g_signal_connect(devicemanager, "device-added", G_CALLBACK(device_add), this);
	g_signal_connect(devicemanager, "device-removed", G_CALLBACK(device_remove), this);
	
	GtkWidget* widget;
	gdk_window_get_user_data(gdk_x11_window_lookup_for_display(this->display, windowhandle), (void**)&widget);
	//we probably have a GtkDrawingArea, and those can't have keyboard focus. Let's ask for its GtkWindow instead.
	widget=gtk_widget_get_toplevel(widget);
	gtk_widget_add_events(widget, GDK_KEY_PRESS_MASK|GDK_KEY_RELEASE_MASK);
	g_signal_connect(widget, "key-press-event", G_CALLBACK(key_action), this);
	g_signal_connect(widget, "key-release-event", G_CALLBACK(key_action), this);
	
	GdkDeviceType types[2]={ GDK_DEVICE_TYPE_SLAVE, GDK_DEVICE_TYPE_FLOATING };
	for (int i=0;i<2;i++)
	{
		GList* devices=gdk_device_manager_list_devices(devicemanager, types[i]);
		GList* list=devices;
		while (list)
		{
			device_add(NULL, GDK_DEVICE(list->data), this);
			list=list->next;
		}
		g_list_free(devices);
	}
	
	return (struct inputkb*)this;
}
#endif
