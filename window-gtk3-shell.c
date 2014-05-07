#include "minir.h"
#ifdef WINDOW_GTK3
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gtk/gtk.h>
#ifdef WNDPROT_X11
#include <gdk/gdkx.h>
#endif

//Number of ugly hacks: 5
//The status bar is a GtkGrid with GtkLabel, not a GtkStatusbar, because I couldn't get GtkStatusbar
// to cooperate. While the status bar is a GtkBox, I couldn't find how to get rid of its child.
//The status bar manages layout manually (by resizing the GtkLabels), because a GtkGrid with a large
// number of slots seem to assign pixels 3,3,2,2 if told to split 10 pixels to 4 equally sized
// slots, as opposed to 2,3,2,3 or 3,2,3,2.
//The labels in the status bar are declared with max width 1, and then ellipsizes. Apparently they
// use more space than maximum if they can. This doesn't seem to be documented, and is therefore not
// guaranteed to continue working.
//gtk_main_iteration_do(false) is called twice, so GTK thinks we're idle and sends out the mouse
// move events. Most of the time is spent waiting for A/V drivers, and our mouse move processor is
// cheap. (Likely fixable in GTK+ 3.12, but I'm on 3.8.)
//Refreshing a listbox is done by telling it to redraw, not by telling it that contents have changed.
// It's either that or send one hundred thousand contents-changed events, and I'd rather not.



struct windowmenu_gtk3;
struct windowradio_gtk3 {
	GtkRadioMenuItem* item;
	struct windowmenu_gtk3 * parent;
	unsigned int state;
};
enum menu_type { mtype_item, mtype_check, mtype_radio, mtype_sep, mtype_sub };
struct windowmenu_gtk3 {
	struct windowmenu i;
	
	GtkMenuItem* item;
	UNION_BEGIN
		void (*onactivate_normal)(struct windowmenu * subject, void* userdata);
		void (*onactivate_check)(struct windowmenu * subject, bool state, void* userdata);
		void (*onactivate_radio)(struct windowmenu * subject, unsigned int state, void* userdata);
	UNION_END
	UNION_BEGIN
		void* userdata;
		GtkMenuShell* submenu;
	UNION_END
	UNION_BEGIN
		struct windowmenu_gtk3 * * children;
		struct windowradio_gtk3 * radiochildren;
	UNION_END
	uint8_t type;
	uint8_t menupos;
	uint8_t numchildren;
	bool block_signals;
};

struct window_gtk3 {
	struct window i;
	
	GtkWindow* wndw;
	GtkGrid* grid;
	struct widget_base * contents;
	
	struct windowmenu_gtk3 * menu;
	
	GtkGrid* status;
	int * status_pos;
	int status_count;
	
	bool visible;
	//char padding1[3];
	
	bool (*onclose)(struct window * subject, void* userdata);
	void* oncloseuserdata;
};

static void statusbar_resize(struct window_gtk3 * this);

static gint get_widget_height(GtkWidget* widget)
{
	if (!widget) return 0;
	GtkRequisition size;
	gtk_widget_get_preferred_size(widget, NULL, &size);
	return size.height;
}

static void resize(struct window * this_, unsigned int width, unsigned int height)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	gtk_window_resize(this->wndw, width,
	                  get_widget_height(GTK_WIDGET(this->menu))+
	                  height+
	                  get_widget_height(GTK_WIDGET(this->status)));
	if (this->status) statusbar_resize(this);
}

static gboolean popup_esc_close(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	struct window_gtk3 * this=(struct window_gtk3*)user_data;
	if (event->key.keyval==GDK_KEY_Escape)
	{
		this->visible=false;
		gtk_widget_hide(GTK_WIDGET(this->wndw));
	}
	return false;
}

static void set_is_dialog(struct window * this_)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	
	gtk_widget_add_events(GTK_WIDGET(this->wndw), GDK_KEY_PRESS_MASK);
	g_signal_connect(this->wndw, "key-press-event", G_CALLBACK(popup_esc_close), this);
	
	gtk_window_set_type_hint(this->wndw, GDK_WINDOW_TYPE_HINT_DIALOG);
}

static void set_parent(struct window * this_, struct window * parent_)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	struct window_gtk3 * parent=(struct window_gtk3*)parent_;
	gtk_window_set_transient_for(this->wndw, parent->wndw);
}

static void set_resizable(struct window * this_, bool resizable,
                          void (*onresize)(struct window * subject, unsigned int newwidth, unsigned int newheight, void* userdata),
                          void* userdata)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	gtk_window_set_resizable(this->wndw, resizable);
	
	if (this->status) statusbar_resize(this);
}

static void set_title(struct window * this_, const char * title)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	gtk_window_set_title(this->wndw, title);
}

static void onclose_set(struct window * this_, bool (*function)(struct window * subject, void* userdata), void* userdata)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	this->onclose=function;
	this->oncloseuserdata=userdata;
}



static void menu_delete(struct windowmenu_gtk3 * this)
{
	if (this->type==mtype_sub)
	{
		for (unsigned int i=0;i<this->numchildren;i++) menu_delete(this->children[i]);
		free(this->children);
	}
	if (this->type==mtype_radio)
	{
		for (unsigned int i=1;i<this->numchildren;i++) gtk_widget_destroy(GTK_WIDGET(this->radiochildren[i].item));
		free(this->radiochildren);
	}
	if (this->item) gtk_widget_destroy(GTK_WIDGET(this->item));//this if statement fires for topmenu
	free(this);
}

static void menu_set_text(GtkMenuItem* item, const char * text)
{
	GtkWidget* label=gtk_bin_get_child(GTK_BIN(item));
	if (*text=='_') gtk_label_set_text_with_mnemonic(GTK_LABEL(label), text+1);
	else gtk_label_set_text(GTK_LABEL(label), text);
}

static void menu_set_enabled(struct windowmenu * this_, bool enable)
{
	struct windowmenu_gtk3 * this=(struct windowmenu_gtk3*)this_;
	gtk_widget_set_sensitive(GTK_WIDGET(this->item), enable);
}

static void menu_activate_normal(GtkMenuItem* menuitem, gpointer user_data)
{
	struct windowmenu_gtk3 * this=(struct windowmenu_gtk3*)user_data;
	this->onactivate_normal((struct windowmenu*)this, this->userdata);
}

struct windowmenu * windowmenu_create_item(const char * text,
                                           void (*onactivate)(struct windowmenu * subject, void* userdata),
                                           void* userdata)
{
	struct windowmenu_gtk3 * this=malloc(sizeof(struct windowmenu_gtk3));
	this->item=GTK_MENU_ITEM(gtk_menu_item_new_with_label(""));
	if (onactivate)
	{
		this->onactivate_normal=onactivate;
		this->userdata=userdata;
		g_signal_connect(this->item, "activate", G_CALLBACK(menu_activate_normal), this);
	}
	this->type=mtype_item;
	this->i.set_enabled=menu_set_enabled;
	menu_set_text(this->item, text);
	return (struct windowmenu*)this;
}

static void menu_activate_check(GtkMenuItem* menuitem, gpointer user_data)
{
	struct windowmenu_gtk3 * this=(struct windowmenu_gtk3*)user_data;
	if (this->block_signals) return;
	this->onactivate_check((struct windowmenu*)this, gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(this->item)), this->userdata);
}

static unsigned int menu_check_get_state(struct windowmenu * this_)
{
	struct windowmenu_gtk3 * this=(struct windowmenu_gtk3*)this_;
	return gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(this->item));
}

static void menu_check_set_state(struct windowmenu * this_, unsigned int state)
{
	struct windowmenu_gtk3 * this=(struct windowmenu_gtk3*)this_;
	this->block_signals=true;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(this->item), state);
	this->block_signals=false;
}

struct windowmenu * windowmenu_create_check(const char * text,
                                            void (*onactivate)(struct windowmenu * subject, bool checked, void* userdata),
                                            void* userdata)
{
	struct windowmenu_gtk3 * this=malloc(sizeof(struct windowmenu_gtk3));
	this->item=GTK_MENU_ITEM(gtk_check_menu_item_new_with_label(""));
	//gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(this->item), checked);
	if (onactivate)
	{
		this->onactivate_check=onactivate;
		this->userdata=userdata;
		if (onactivate) g_signal_connect(this->item, "activate", G_CALLBACK(menu_activate_check), this);
	}
	this->block_signals=false;
	this->type=mtype_check;
	this->i.set_enabled=menu_set_enabled;
	this->i.get_state=menu_check_get_state;
	this->i.set_state=menu_check_set_state;
	menu_set_text(this->item, text);
	return (struct windowmenu*)this;
}

static void menu_activate_radio(GtkMenuItem* menuitem, gpointer user_data)
{
	struct windowradio_gtk3 * item=(struct windowradio_gtk3*)user_data;
	struct windowmenu_gtk3 * this=item->parent;
	if (this->block_signals) return;
	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item->item))) return;
	this->onactivate_radio((struct windowmenu*)this, item->state, this->userdata);
}

static void menu_radio_set_enabled(struct windowmenu * this_, bool enable)
{
	struct windowmenu_gtk3 * this=(struct windowmenu_gtk3*)this_;
	for (unsigned int i=0;i<this->numchildren;i++)
	{
		gtk_widget_set_sensitive(GTK_WIDGET(this->radiochildren[i].item), enable);
	}
}

static unsigned int menu_radio_get_state(struct windowmenu * this_)
{
	struct windowmenu_gtk3 * this=(struct windowmenu_gtk3*)this_;
	unsigned int ret=0;
	while (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(this->radiochildren[ret].item))) ret++;
	return ret;
}

static void menu_radio_set_state(struct windowmenu * this_, unsigned int state)
{
	struct windowmenu_gtk3 * this=(struct windowmenu_gtk3*)this_;
	this->block_signals=true;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(this->radiochildren[state].item), true);
	this->block_signals=false;
}

struct windowmenu * windowmenu_create_radio_l(unsigned int numitems, const char * const * texts,
                                              void (*onactivate)(struct windowmenu * subject, unsigned int state, void* userdata),
                                              void* userdata)
{
	struct windowmenu_gtk3 * this=malloc(sizeof(struct windowmenu_gtk3));
	this->block_signals=true;
	this->radiochildren=malloc(sizeof(struct windowradio_gtk3)*numitems);
	
	this->numchildren=numitems;
	GSList* group=NULL;
	for (unsigned int i=0;i<numitems;i++)
	{
		GtkRadioMenuItem* item=GTK_RADIO_MENU_ITEM(gtk_radio_menu_item_new_with_label(group, ""));
		group=gtk_radio_menu_item_get_group(item);
		menu_set_text(GTK_MENU_ITEM(item), texts[i]);
		if (onactivate) g_signal_connect(item, "activate", G_CALLBACK(menu_activate_radio), &this->radiochildren[i]);
		this->radiochildren[i].item=item;
		this->radiochildren[i].parent=this;
		this->radiochildren[i].state=i;
	}
	this->item=GTK_MENU_ITEM(this->radiochildren[0].item);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(this->item), true);
	
	this->type=mtype_radio;
	this->onactivate_radio=onactivate;
	this->userdata=userdata;
	this->i.set_enabled=menu_radio_set_enabled;
	this->i.get_state=menu_radio_get_state;
	this->i.set_state=menu_radio_set_state;
	this->block_signals=false;
	return (struct windowmenu*)this;
}

static unsigned int menu_get_native_length(struct windowmenu_gtk3 * this)
{
	return (this->type==mtype_radio ? this->numchildren : 1);
}

static unsigned int menu_get_native_start(struct windowmenu_gtk3 * this, unsigned int pos)
{
	return (pos ? this->children[pos-1]->menupos+menu_get_native_length(this->children[pos-1]) : 0);
}

static void menu_insert_child(struct windowmenu * this_, struct windowmenu * child_, unsigned int pos)
{
	struct windowmenu_gtk3 * this=(struct windowmenu_gtk3*)this_;
	struct windowmenu_gtk3 * child=(struct windowmenu_gtk3*)child_;
	
	unsigned int menupos=menu_get_native_start(this, pos);
	if (child->type!=mtype_radio)
	{
		gtk_menu_shell_insert(this->submenu, GTK_WIDGET(child->item), menupos);
	}
	else
	{
		for (unsigned int i=0;i<child->numchildren;i++)
		{
			gtk_menu_shell_insert(this->submenu, GTK_WIDGET(child->radiochildren[i].item), menupos);
			menupos++;
		}
	}
	gtk_widget_show_all(GTK_WIDGET(child->item));
	if ((this->numchildren&(this->numchildren-1))==0)
	{
		this->children=realloc(this->children, (this->numchildren*2)*sizeof(struct windowmenu_gtk3*));
		if (!this->children) abort();
	}
	memmove(this->children+pos+1, this->children+pos, (this->numchildren-pos)*sizeof(struct windowmenu_gtk3*));
	this->children[pos]=child;
	this->numchildren++;
	for (unsigned int i=pos;i<this->numchildren;i++)
	{
		struct windowmenu_gtk3 * child=this->children[i];
		child->menupos=menupos;
		menupos+=menu_get_native_length(child);
	}
}

static void menu_remove_child(struct windowmenu * this_, struct windowmenu * child_)
{
	struct windowmenu_gtk3 * this=(struct windowmenu_gtk3*)this_;
	struct windowmenu_gtk3 * child=(struct windowmenu_gtk3*)child_;
	menu_delete(child);
	this->numchildren--;
	unsigned int pos=0;
	while (this->children[pos]!=child) pos++;
	memmove(this->children+pos, this->children+pos+1, (this->numchildren-pos)*sizeof(struct windowmenu_gtk3*));
}

struct windowmenu * windowmenu_create_submenu_l(const char * text, unsigned int numchildren, struct windowmenu * * children)
{
	struct windowmenu_gtk3 * this=malloc(sizeof(struct windowmenu_gtk3));
	this->item=GTK_MENU_ITEM(gtk_menu_item_new_with_label(""));
	this->submenu=GTK_MENU_SHELL(gtk_menu_new());
	this->children=NULL;
	this->numchildren=0;
	for (unsigned int i=0;i<numchildren;i++) menu_insert_child((struct windowmenu*)this, children[i], i);
	gtk_menu_item_set_submenu(this->item, GTK_WIDGET(this->submenu));
	this->type=mtype_sub;
	this->i.set_enabled=menu_set_enabled;
	this->i.insert_child=menu_insert_child;
	this->i.remove_child=menu_remove_child;
	menu_set_text(this->item, text);
	return (struct windowmenu*)this;
}

struct windowmenu * windowmenu_create_topmenu_l(unsigned int numchildren, struct windowmenu * * children)
{
	struct windowmenu_gtk3 * this=malloc(sizeof(struct windowmenu_gtk3));
	this->submenu=GTK_MENU_SHELL(gtk_menu_bar_new());
	this->children=NULL;
	this->item=NULL;
	this->numchildren=0;
	for (unsigned int i=0;i<numchildren;i++) menu_insert_child((struct windowmenu*)this, children[i], i);
	this->type=mtype_sub;
	this->i.insert_child=menu_insert_child;
	this->i.remove_child=menu_remove_child;
	return (struct windowmenu*)this;
}

struct windowmenu * windowmenu_create_separator()
{
	struct windowmenu_gtk3 * this=malloc(sizeof(struct windowmenu_gtk3));
	this->item=GTK_MENU_ITEM(gtk_separator_menu_item_new());
	this->type=mtype_sep;
	return (struct windowmenu*)this;
}

static void set_menu(struct window * this_, struct windowmenu * menu_)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	struct windowmenu_gtk3 * menu=(struct windowmenu_gtk3*)menu_;
	if (this->menu) menu_delete(this->menu);
	this->menu=menu;
	gtk_grid_attach(this->grid, GTK_WIDGET(this->menu->submenu), 0,0, 1,1);
	gtk_widget_show_all(GTK_WIDGET(this->menu->submenu));
}



static void statusbar_create(struct window * this_, int numslots, const int * align, const int * dividerpos)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	if (this->status) gtk_widget_destroy(GTK_WIDGET(this->status));
	this->status=NULL;
	//if (this->status_parent) gtk_widget_destroy(GTK_WIDGET(this->status_parent));
	//this->status_parent=NULL;
	free(this->status_pos);
	this->status_pos=NULL;
	
	gtk_window_set_has_resize_grip(this->wndw, (bool)numslots);
	
	if (!numslots) return;
	
	this->status=GTK_GRID(gtk_grid_new());
	//gtk_box_set_spacing(box, 2);
	for (int i=0;i<numslots;i++)
	{
		GtkWidget* label=gtk_label_new("");
		
		gtk_widget_set_margin_top(label, 2);
		gtk_widget_set_margin_bottom(label, 2);
		gtk_widget_set_margin_left(label, 2);
		gtk_widget_set_margin_right(label, 2);
		
//printf("a=%i\n",align[i]);
		//const GtkJustification just[]={ GTK_JUSTIFY_LEFT, GTK_JUSTIFY_CENTER, GTK_JUSTIFY_RIGHT };
		//gtk_label_set_justify(label, just[align[i]]);
		
		gtk_label_set_single_line_mode(GTK_LABEL(label), true);
		gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars(GTK_LABEL(label), 1);//why does this work
		
		gtk_misc_set_alignment(GTK_MISC(label), ((float)align[i])/2, 0.5);
		//const GtkAlign gtkalign[]={ GTK_ALIGN_START, GTK_ALIGN_CENTER, GTK_ALIGN_END };
		//gtk_widget_set_halign(label, gtkalign[align[i]]);
		
		gtk_grid_attach(this->status, label, i,0, 1,1);
//GdkRGBA color={1,0,i&1,1};
//gtk_widget_override_background_color(label,GTK_STATE_FLAG_NORMAL,&color);
	}
	
	this->status_count=numslots;
	this->status_pos=malloc(sizeof(int)*numslots);
	memcpy(this->status_pos, dividerpos, sizeof(int)*(numslots-1));
	this->status_pos[numslots-1]=240;
	//gtk_widget_set_size_request(GTK_WIDGET(this->status), width, -1);
	
	//this->status_parent=GTK_FRAME(gtk_frame_new(NULL));
	//gtk_frame_set_shadow_type(this->status_parent, GTK_SHADOW_IN);
	//gtk_container_add(GTK_CONTAINER(this->status_parent), GTK_WIDGET(this->status));
	//
	//gtk_grid_attach(this->grid, GTK_WIDGET(this->status_parent), 0,2, 1,1);
	gtk_grid_attach(this->grid, GTK_WIDGET(this->status), 0,2, 1,1);
	gtk_widget_show_all(GTK_WIDGET(this->status));
	
	statusbar_resize(this);
}

static void statusbar_set(struct window * this_, int slot, const char * text)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	GtkLabel* label=GTK_LABEL(gtk_grid_get_child_at(this->status, slot, 0));
	gtk_label_set_text(label, text);
}

static void statusbar_resize(struct window_gtk3 * this)
{
	//if (width<0) gtk_widget_get_size_request(GTK_WIDGET(this->contents), &width, NULL);
	GtkAllocation size;
	gtk_widget_get_allocation(GTK_WIDGET(this->contents->_widget), &size);
	if (size.width<=1) return;
	
	size.width-=(this->status_count*2*2);
	if (gtk_window_get_resizable(this->wndw))
	{
		gint gripwidth;
		gtk_widget_style_get(GTK_WIDGET(this->wndw), "resize-grip-width", &gripwidth, NULL);
		size.width-=gripwidth;
	}
	int lastpos=0;
	for (int i=0;i<this->status_count;i++)
	{
		int nextpos=(size.width*this->status_pos[i] + 120)/240;
		GtkWidget* label=gtk_grid_get_child_at(this->status, i, 0);
		gtk_widget_set_size_request(label, nextpos-lastpos, -1);
		lastpos=nextpos;
	}
}



static void replace_contents(struct window * this_, void * contents)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	gtk_widget_destroy(this->contents->_widget);
	this->contents->_free(this->contents);
	gtk_grid_attach(this->grid, GTK_WIDGET(this->contents->_widget), 0,1, 1,1);
	this->contents=(struct widget_base*)contents;
}



static bool is_visible(struct window * this_)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	return this->visible;
}

static void set_visible(struct window * this_, bool visible)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	this->visible=visible;
	if (visible)
	{
		gtk_widget_show_all(GTK_WIDGET(this->wndw));
		statusbar_resize(this);
	}
	else gtk_widget_hide(GTK_WIDGET(this->wndw));
}

static void focus(struct window * this_)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	gtk_window_present(this->wndw);
}

static bool is_active(struct window * this_)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	
	if (this->menu && gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(this->menu->submenu))!=NULL) return false;
	return gtk_window_is_active(this->wndw);
}

static bool menu_active(struct window * this_)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	
	if (this->menu && gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(this->menu->submenu))!=NULL) return true;
	return false;
}

static void free_(struct window * this_)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	this->contents->_free(this->contents);
	
	if (this->menu) menu_delete(this->menu);
	gtk_widget_destroy(GTK_WIDGET(this->wndw));
	free(this->status_pos);
	
	free(this);
}

static void* _get_handle(struct window * this_)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	return this->wndw;
}

static gboolean onclose_gtk(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	struct window_gtk3 * this=(struct window_gtk3*)user_data;
	if (this->onclose && this->onclose((struct window*)this, this->oncloseuserdata)==false) return TRUE;
	
	this->visible=false;
	gtk_widget_hide(GTK_WIDGET(this->wndw));
	return TRUE;
}

const struct window_gtk3 window_gtk3_base = {{
	set_is_dialog, set_parent, resize, set_resizable, set_title, onclose_set, set_menu,
	statusbar_create, statusbar_set, replace_contents,
	set_visible, is_visible, focus, is_active, menu_active, free_, _get_handle, NULL
}};
struct window * window_create(void * contents_)
{
	struct window_gtk3 * this=malloc(sizeof(struct window_gtk3));
	memcpy(&this->i, &window_gtk3_base, sizeof(struct window_gtk3));
	
	this->wndw=GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	g_signal_connect(this->wndw, "delete-event", G_CALLBACK(onclose_gtk), this);//GtkWidget delete-event maybe
	gtk_window_set_has_resize_grip(this->wndw, false);
	gtk_window_set_resizable(this->wndw, false);
	
	this->grid=GTK_GRID(gtk_grid_new());
	gtk_container_add(GTK_CONTAINER(this->wndw), GTK_WIDGET(this->grid));
	
	this->contents=(struct widget_base*)contents_;
	gtk_grid_attach(this->grid, GTK_WIDGET(this->contents->_widget), 0,1, 1,1);
	
	this->visible=false;
	
//GdkRGBA color={0,0,1,1};
//gtk_widget_override_background_color(GTK_WIDGET(this->wndw),GTK_STATE_FLAG_NORMAL,&color);
	return (struct window*)this;
}
#endif
