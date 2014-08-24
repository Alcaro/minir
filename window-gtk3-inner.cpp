#include "minir.h"
#ifdef WINDOW_GTK3
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#ifdef WNDPROT_X11
#include <gdk/gdkx.h>
#endif
#undef this

static bool in_callback=false;
static GtkCssProvider* cssprovider;

void _window_init_inner()
{
	cssprovider=gtk_css_provider_new();
	gtk_css_provider_load_from_data(cssprovider,
		"GtkEntry#invalid { background-image: none; background-color: #F66; color: #FFF; }"
		"GtkEntry#invalid:selected { background-color: #3465A4; color: #FFF; }"
		//this selection doesn't look too good, but not terrible either.
		, -1, NULL);
}



widget_padding::widget_padding(bool vertical)
{
	widget=GTK_DRAWING_AREA(gtk_drawing_area_new());
	widthprio=(vertical ? 0 : 2);
	heightprio=(vertical ? 2 : 0);
}

widget_padding::~widget_padding() {}



widget_label::widget_label()
{
	widget=gtk_label_new("");
	widthprio=1;
	heightprio=1;
}

widget_label::~widget_label() {}

widget_label::widget_label(const char * text)
{
	widget=gtk_label_new(text);
	widthprio=1;
	heightprio=1;
}

widget_label* widget_label::set_alignment(int alignment)
{
	gtk_misc_set_alignment(GTK_MISC(widget), ((float)alignment)/2, 0.5);
	return this;
}



struct widget_button::impl {
	void (*onclick)(struct widget_button * button, void* userdata);
	void* userdata;
};

widget_button::widget_button() : m(new impl)
{
	widget=gtk_button_new_with_label("");
	widthprio=1;
	heightprio=1;
}

widget_button::widget_button(const char * text) : m(new impl)
{
	widget=gtk_button_new_with_label(text);
	widthprio=1;
	heightprio=1;
}

widget_button::~widget_button()
{
	delete m;
}

widget_button* widget_button::set_enabled(bool enable)
{
	gtk_widget_set_sensitive(GTK_WIDGET(widget), enable);
	return this;
}

widget_button* widget_button::set_text(const char * text)
{
	gtk_button_set_label(GTK_BUTTON(widget), text);
	return this;
}

static void widget_button_onclick(GtkButton* button, gpointer user_data)
{
	widget_button * obj=(widget_button*)user_data;
	obj->m->onclick(obj, obj->m->userdata);
}

widget_button* widget_button::set_onclick(void (*onclick)(struct widget_button * subject, void* userdata), void* userdata)
{
	g_signal_connect(widget, "clicked", G_CALLBACK(widget_button_onclick), this);
	m->onclick=onclick;
	m->userdata=userdata;
	return this;
}



struct widget_checkbox::impl {
	void (*onclick)(struct widget_checkbox * button, bool checked, void* userdata);
	void* userdata;
};

widget_checkbox::widget_checkbox() : m(new impl)
{
	widget=gtk_check_button_new_with_label("");
	widthprio=1;
	heightprio=1;
}

widget_checkbox::widget_checkbox(const char * text) : m(new impl)
{
	widget=gtk_check_button_new_with_label(text);
	widthprio=1;
	heightprio=1;
}

widget_checkbox::~widget_checkbox()
{
	delete m;
}

widget_checkbox* widget_checkbox::set_enabled(bool enable)
{
	gtk_widget_set_sensitive(GTK_WIDGET(widget), enable);
	return this;
}

widget_checkbox* widget_checkbox::set_text(const char * text)
{
	gtk_button_set_label(GTK_BUTTON(widget), text);
	return this;
}

bool widget_checkbox::get_state()
{
	return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

widget_checkbox* widget_checkbox::set_state(bool checked)
{
	in_callback=true;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), checked);
	in_callback=false;
	return this;
}

static void widget_checkbox_onclick(GtkButton* button, gpointer user_data)
{
	if (in_callback) return;
	widget_checkbox * obj=(widget_checkbox*)user_data;
	obj->m->onclick(obj, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(obj->widget)), obj->m->userdata);
}

widget_checkbox* widget_checkbox::set_onclick(void (*onclick)(struct widget_checkbox * subject, bool checked, void* userdata), void* userdata)
{
	g_signal_connect(widget, "clicked", G_CALLBACK(widget_checkbox_onclick), this);
	m->onclick=onclick;
	m->userdata=userdata;
	return this;
}



#if 0
struct widget_radio_gtk3 {
	struct widget_radio i;
	
	GtkLabel* label;
	
	unsigned int grouplen;
	struct widget_radio_gtk3 * * group;
	
	unsigned int id;
	struct widget_radio_gtk3 * parent;
	
	void (*onclick)(struct widget_radio * subject, unsigned int state, void* userdata);
	void* userdata;
};

static void radio__free(struct widget_base * this_)
{
	struct widget_radio_gtk3 * this=(struct widget_radio_gtk3*)this_;
	if (this->group) free(this->group);
	free(this);
}

static void radio_set_enabled(struct widget_radio * this_, bool enable)
{
	struct widget_radio_gtk3 * this=(struct widget_radio_gtk3*)this_;
	gtk_widget_set_sensitive(GTK_WIDGET(this->i._base.widget), enable);
}

static void radio_set_text(struct widget_radio * this_, const char * text)
{
	struct widget_radio_gtk3 * this=(struct widget_radio_gtk3*)this_;
	gtk_label_set_text(this->label, text);
}

static void radio_group(struct widget_radio * this_, unsigned int numitems, struct widget_radio * * group_)
{
	struct widget_radio_gtk3 * this=(struct widget_radio_gtk3*)this_;
	struct widget_radio_gtk3 * * group=(struct widget_radio_gtk3**)group_;
	this->parent=this;
	this->id=0;
	for (unsigned int i=0;i<numitems-1;i++)
	{
		group[i+1]->parent=this;
		group[i+1]->id=i+1;
		gtk_radio_button_join_group(GTK_RADIO_BUTTON(group[i+1]->i._base.widget), GTK_RADIO_BUTTON(group[i]->i._base.widget));
	}
	this->group=malloc(sizeof(struct widget_radio_gtk3*)*numitems);
	memcpy(this->group, group, sizeof(struct widget_radio_gtk3*)*numitems);
	this->grouplen=numitems;
}

static unsigned int radio_get_state(struct widget_radio * this_)
{
	struct widget_radio_gtk3 * this=(struct widget_radio_gtk3*)this_;
	if (!this->group) return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(this->i._base.widget));
	
	for (unsigned int i=0;i<this->grouplen;i++)
	{
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(this->group[i]->i._base.widget))) return i;
	}
	return 0;
}

static void radio_set_state(struct widget_radio * this_, unsigned int state)
{
	struct widget_radio_gtk3 * this=(struct widget_radio_gtk3*)this_;
	in_callback=true;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(this->group[state]->i._base.widget), true);
	in_callback=false;
}

static void radio_onclick(GtkToggleButton* togglebutton, gpointer user_data)
{
	if (in_callback) return;
	struct widget_radio_gtk3 * this=(struct widget_radio_gtk3*)user_data;
	if (!gtk_toggle_button_get_active(togglebutton)) return;
	this->parent->onclick((struct widget_radio*)this->parent, this->id, this->parent->userdata);
}

static void radio_set_onclick(struct widget_radio * this_,
                              void (*onclick)(struct widget_radio * subject, unsigned int state, void* userdata), void* userdata)
{
	struct widget_radio_gtk3 * this=(struct widget_radio_gtk3*)this_;
	this->onclick=onclick;
	this->userdata=userdata;
	for (unsigned int i=0;i<this->grouplen;i++)
	{
		g_signal_connect(this->group[i]->i._base.widget, "toggled", G_CALLBACK(radio_onclick), this->group[i]);
	}
}

struct widget_radio * widget_create_radio(const char * text)
{
	struct widget_radio_gtk3 * this=malloc(sizeof(struct widget_radio_gtk3));
	this->i._base.widget=gtk_radio_button_new(NULL);
	this->label=GTK_LABEL(gtk_label_new(text));
	gtk_container_add(GTK_CONTAINER(this->i._base.widget), GTK_WIDGET(this->label));
	this->i._base.widthprio=1;
	this->i._base.heightprio=1;
	this->i._base.free=radio__free;
	
	this->i.set_enabled=radio_set_enabled;
	this->i.set_text=radio_set_text;
	this->i.group=radio_group;
	this->i.get_state=radio_get_state;
	this->i.set_state=radio_set_state;
	this->i.set_onclick=radio_set_onclick;
	
	this->group=NULL;
	
	return (struct widget_radio*)this;
}



struct widget_textbox_gtk3 {
	struct widget_textbox i;
	
	void (*onchange)(struct widget_textbox * subject, const char * text, void* userdata);
	void* ch_userdata;
	void (*onactivate)(struct widget_textbox * subject, const char * text, void* userdata);
	void* ac_userdata;
};

static void textbox__free(struct widget_base * this_)
{
	struct widget_textbox_gtk3 * this=(struct widget_textbox_gtk3*)this_;
	free(this);
}

static void textbox_set_enabled(struct widget_textbox * this_, bool enable)
{
	struct widget_textbox_gtk3 * this=(struct widget_textbox_gtk3*)this_;
	gtk_widget_set_sensitive(GTK_WIDGET(this->i._base.widget), enable);
}

static void textbox_focus(struct widget_textbox * this_)
{
	struct widget_textbox_gtk3 * this=(struct widget_textbox_gtk3*)this_;
	gtk_widget_grab_focus(GTK_WIDGET(this->i._base.widget));
}

static void textbox_set_text(struct widget_textbox * this_, const char * text)
{
	struct widget_textbox_gtk3 * this=(struct widget_textbox_gtk3*)this_;
	gtk_entry_set_text(GTK_ENTRY(this->i._base.widget), text);
}

static void textbox_set_length(struct widget_textbox * this_, unsigned int maxlen)
{
	struct widget_textbox_gtk3 * this=(struct widget_textbox_gtk3*)this_;
	gtk_entry_set_max_length(GTK_ENTRY(this->i._base.widget), maxlen);
}

static void textbox_set_width(struct widget_textbox * this_, unsigned int xs)
{
	struct widget_textbox_gtk3 * this=(struct widget_textbox_gtk3*)this_;
	gtk_entry_set_width_chars(GTK_ENTRY(this->i._base.widget), xs);
}

static void textbox_set_invalid(struct widget_textbox * this_, bool invalid)
{
	struct widget_textbox_gtk3 * this=(struct widget_textbox_gtk3*)this_;
	if (invalid)
	{
		GtkStyleContext* context=gtk_widget_get_style_context(GTK_WIDGET(this->i._base.widget));
		gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(cssprovider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		gtk_widget_set_name(GTK_WIDGET(this->i._base.widget), "invalid");
		gtk_widget_grab_focus(GTK_WIDGET(this->i._base.widget));
	}
	else
	{
		gtk_widget_set_name(GTK_WIDGET(this->i._base.widget), "x");
	}
}

static const char * textbox_get_text(struct widget_textbox * this_)
{
	struct widget_textbox_gtk3 * this=(struct widget_textbox_gtk3*)this_;
	return gtk_entry_get_text(GTK_ENTRY(this->i._base.widget));
}

static void textbox_onchange(GtkEntry* entry, gpointer user_data)
{
	struct widget_textbox_gtk3 * this=(struct widget_textbox_gtk3*)user_data;
	gtk_widget_set_name(GTK_WIDGET(this->i._base.widget), "x");
	if (this->onchange)
	{
		this->onchange((struct widget_textbox*)this, gtk_entry_get_text(GTK_ENTRY(this->i._base.widget)), this->ch_userdata);
	}
}

static void textbox_set_onchange(struct widget_textbox * this_,
                                 void (*onchange)(struct widget_textbox * subject, const char * text, void* userdata),
                                 void* userdata)
{
	struct widget_textbox_gtk3 * this=(struct widget_textbox_gtk3*)this_;
	
	this->onchange=onchange;
	this->ch_userdata=userdata;
}

static void textbox_onactivate(GtkEntry* entry, gpointer user_data)
{
	struct widget_textbox_gtk3 * this=(struct widget_textbox_gtk3*)user_data;
	this->onactivate((struct widget_textbox*)this, gtk_entry_get_text(GTK_ENTRY(this->i._base.widget)), this->ac_userdata);
}

static void textbox_set_onactivate(struct widget_textbox * this_,
                                   void (*onactivate)(struct widget_textbox * subject, const char * text, void* userdata),
                                   void* userdata)
{
	struct widget_textbox_gtk3 * this=(struct widget_textbox_gtk3*)this_;
	
	g_signal_connect(this->i._base.widget, "activate", G_CALLBACK(textbox_onactivate), this);
	this->onactivate=onactivate;
	this->ac_userdata=userdata;
}

struct widget_textbox * widget_create_textbox()
{
	struct widget_textbox_gtk3 * this=malloc(sizeof(struct widget_textbox_gtk3));
	this->i._base.widget=gtk_entry_new();
	this->i._base.widthprio=3;
	this->i._base.heightprio=1;
	this->i._base.free=textbox__free;
	
	this->i.set_enabled=textbox_set_enabled;
	this->i.focus=textbox_focus;
	this->i.get_text=textbox_get_text;
	this->i.set_text=textbox_set_text;
	this->i.set_length=textbox_set_length;
	this->i.set_width=textbox_set_width;
	this->i.set_invalid=textbox_set_invalid;
	this->i.set_onchange=textbox_set_onchange;
	this->i.set_onactivate=textbox_set_onactivate;
	
	gtk_entry_set_width_chars(GTK_ENTRY(this->i._base.widget), 5);
	
	g_signal_connect(this->i._base.widget, "changed", G_CALLBACK(textbox_onchange), this);
	this->onchange=NULL;
	
	return (struct widget_textbox*)this;
}



struct widget_canvas_gtk3;



struct widget_viewport_gtk3 {
	struct widget_viewport i;
	
	bool hide_mouse;
	bool hide_mouse_timer_active;
	
	guint32 hide_mouse_at;
	GdkCursor* hidden_cursor;
	
	void (*on_file_drop)(struct widget_viewport * subject, const char * const * filenames, void* userdata);
	void* dropuserdata;
};

static void viewport__free(struct widget_base * this_)
{
	struct widget_viewport_gtk3 * this=(struct widget_viewport_gtk3*)this_;
	if (this->hidden_cursor) g_object_unref(this->hidden_cursor);
	free(this);
}

static void viewport_resize(struct widget_viewport * this_, unsigned int width, unsigned int height)
{
	struct widget_viewport_gtk3 * this=(struct widget_viewport_gtk3*)this_;
	
	gtk_widget_set_size_request(GTK_WIDGET(this->i._base.widget), width, height);
}

static uintptr_t viewport_get_window_handle(struct widget_viewport * this_)
{
	struct widget_viewport_gtk3 * this=(struct widget_viewport_gtk3*)this_;
	
	//this won't work on anything except X11, but should be trivial to create an equivalent for.
	uintptr_t tmp=GDK_WINDOW_XID(gtk_widget_get_window(GTK_WIDGET(this->i._base.widget)));
	return tmp;
}

static void viewport_set_hide_cursor_now(struct widget_viewport_gtk3 * this, bool hide)
{
	GdkWindow* gdkwindow=gtk_widget_get_window(GTK_WIDGET(this->i._base.widget));
	if (gdkwindow) gdk_window_set_cursor(gdkwindow, hide ? this->hidden_cursor : NULL);
}

static gboolean viewport_mouse_timeout(gpointer user_data)
{
	struct widget_viewport_gtk3 * this=(struct widget_viewport_gtk3*)user_data;
	
	guint32 now=g_get_monotonic_time()/1000;
	if (now >= this->hide_mouse_at)
	{
		this->hide_mouse_timer_active=false;
		viewport_set_hide_cursor_now(this, this->hide_mouse);
	}
	else
	{
		guint32 remaining=this->hide_mouse_at-now+10;
		g_timeout_add(remaining, viewport_mouse_timeout, this);
	}
	
	return G_SOURCE_REMOVE;
}

static gboolean viewport_mouse_move_handler(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	struct widget_viewport_gtk3 * this=(struct widget_viewport_gtk3*)user_data;
	
	this->hide_mouse_at=g_get_monotonic_time()/1000 + 990;
	if (!this->hide_mouse_timer_active)
	{
		this->hide_mouse_timer_active=true;
		g_timeout_add(1000, viewport_mouse_timeout, this);
		viewport_set_hide_cursor_now(this, false);
	}
	
	return G_SOURCE_CONTINUE;
}

static void viewport_set_hide_cursor(struct widget_viewport * this_, bool hide)
{
	struct widget_viewport_gtk3 * this=(struct widget_viewport_gtk3*)this_;
	
	if (this->hide_mouse_at && g_get_monotonic_time()/1000 >= this->hide_mouse_at)
	{
		viewport_set_hide_cursor_now(this, hide);
	}
	
	this->hide_mouse=hide;
	if (!hide || this->hide_mouse_at) return;
	
	if (!this->hidden_cursor) this->hidden_cursor=gdk_cursor_new(GDK_BLANK_CURSOR);
	
	gtk_widget_add_events(GTK_WIDGET(this->i._base.widget), GDK_POINTER_MOTION_MASK);
	g_signal_connect(this->i._base.widget, "motion-notify-event", G_CALLBACK(viewport_mouse_move_handler), this);
	
	//seems to not exist in gtk+ 3.8
	//gdk_window_set_event_compression(gtk_widget_get_window(this->i._base.widget), false);
	
	this->hide_mouse_timer_active=false;
	viewport_mouse_move_handler(NULL, NULL, this);
}

/*
void (*keyboard_cb)(struct window * subject, unsigned int keycode, void* userdata);
void* keyboard_ud;

//static gboolean kb_signal(GtkWidget* widget, GdkEvent* event, gpointer user_data)
//{
//	struct window_gtk3 * this=(struct window_gtk3*)user_data;
//	return FALSE;
//}

static void set_kb_callback(struct window * this_,
                            void (*keyboard_cb)(struct window * subject, unsigned int keycode, void* userdata), void* userdata)
{
	struct window_gtk3 * this=(struct window_gtk3*)this_;
	gtk_widget_add_events(GTK_WIDGET(this->wndw), GDK_KEY_PRESS_MASK);
	//g_signal_connect(this->contents, "key-press-event", G_CALLBACK(kb_signal), this);
	this->keyboard_cb=keyboard_cb;
	this->keyboard_ud=userdata;
}
*/

static void viewport_drop_handler(GtkWidget* widget, GdkDragContext* drag_context, gint x, gint y,
                                  GtkSelectionData* selection_data, guint info, guint time, gpointer user_data)
{
	struct widget_viewport_gtk3 * this=(struct widget_viewport_gtk3*)user_data;
	if (!selection_data || !gtk_selection_data_get_length(selection_data))
	{
		gtk_drag_finish(drag_context, FALSE, FALSE, time);
		return;
	}
	
	const char * data=(gchar*)gtk_selection_data_get_data(selection_data);
	int numstr=0;
	for (int i=0;data[i];i++)
	{
		if (data[i]=='\n') numstr++;
	}
	
	char* datacopy=strdup(data);
	const char** strings=malloc(sizeof(char*)*(numstr+1));
	char* last=datacopy;
	int strnum=0;
	for (int i=0;datacopy[i];i++)
	{
		if (datacopy[i]=='\r') datacopy[i]='\0';//where did those come from? this isn't Windows, we shouldn't be getting Windows-isms.
		if (datacopy[i]=='\n')
		{
			datacopy[i]='\0';
			strings[strnum]=window_get_absolute_path(last);
			last=datacopy+i+1;
			strnum++;
		}
	}
	strings[numstr]=NULL;
	free(datacopy);
	
	this->on_file_drop((struct widget_viewport*)this, strings, this->dropuserdata);
	
	for (int i=0;strings[i];i++) free((void*)strings[i]);
	free(strings);
	gtk_drag_finish(drag_context, TRUE, FALSE, time);
}

static void viewport_set_support_drop(struct widget_viewport * this_,
                             void (*on_file_drop)(struct widget_viewport * subject, const char * const * filenames, void* userdata),
                             void* userdata)
{
	struct widget_viewport_gtk3 * this=(struct widget_viewport_gtk3*)this_;
	
	GtkTargetList* list=gtk_target_list_new(NULL, 0);
	gtk_target_list_add_uri_targets(list, 0);
	
	int n_targets;
	GtkTargetEntry* targets=gtk_target_table_new_from_list(list, &n_targets);
	//GTK_DEST_DEFAULT_MOTION|GTK_DEST_DEFAULT_DROP
	gtk_drag_dest_set(GTK_WIDGET(this->i._base.widget), GTK_DEST_DEFAULT_ALL, targets,n_targets, GDK_ACTION_COPY);
	
	gtk_target_table_free(targets, n_targets);
	gtk_target_list_unref(list);
	
	g_signal_connect(this->i._base.widget, "drag-data-received", G_CALLBACK(viewport_drop_handler), this);
	this->on_file_drop=on_file_drop;
	this->dropuserdata=userdata;
}

struct widget_viewport * widget_create_viewport(unsigned int width, unsigned int height)
{
	struct widget_viewport_gtk3 * this=malloc(sizeof(struct widget_viewport_gtk3));
	this->i._base.widget=gtk_drawing_area_new();
	this->i._base.widthprio=0;
	this->i._base.heightprio=0;
	this->i._base.free=viewport__free;
	this->i.resize=viewport_resize;
	this->i.get_window_handle=viewport_get_window_handle;
	this->i.set_hide_cursor=viewport_set_hide_cursor;
	this->i.set_support_drop=viewport_set_support_drop;
	
	this->hide_mouse_at=0;
	this->hidden_cursor=NULL;
	gtk_widget_set_size_request(GTK_WIDGET(this->i._base.widget), width, height);
	
	return (struct widget_viewport*)this;
}



struct widget_listbox_gtk3;



struct widget_frame_gtk3 {
	struct widget_frame i;
	
	struct widget_base * child;
};

static void frame__free(struct widget_base * this_)
{
	struct widget_frame_gtk3 * this=(struct widget_frame_gtk3*)this_;
	this->child->free(this->child);
	free(this);
}

static void frame_set_text(struct widget_frame * this_, const char * text)
{
	struct widget_frame_gtk3 * this=(struct widget_frame_gtk3*)this_;
	gtk_frame_set_label(GTK_FRAME(this->i._base.widget), text);
}

struct widget_frame * widget_create_frame(const char * text, void* contents)
{
	struct widget_frame_gtk3 * this=malloc(sizeof(struct widget_frame_gtk3));
	struct widget_base * child=(struct widget_base*)contents;
	this->i._base.widget=gtk_frame_new(text);
	this->i._base.widthprio=child->widthprio;
	this->i._base.heightprio=child->heightprio;
	this->i._base.free=frame__free;
	this->i.set_text=frame_set_text;
	this->child=(widget_base*)contents;
	gtk_container_add(GTK_CONTAINER(this->i._base.widget), GTK_WIDGET(child->widget));
	
	return (struct widget_frame*)this;
}



struct widget_layout_gtk3 {
	struct widget_layout i;
	
	struct widget_base * * children;
	unsigned int numchildren;
};

static void layout__free(struct widget_base * this_)
{
	struct widget_layout_gtk3 * this=(struct widget_layout_gtk3*)this_;
	for (unsigned int i=0;i<this->numchildren;i++)
	{
		this->children[i]->free(this->children[i]);
	}
	free(this->children);
	free(this);
}

struct widget_layout * widget_create_layout_l(unsigned int numchildren, void * * children,
                                              unsigned int totwidth,  unsigned int * widths,  bool uniformwidths,
                                              unsigned int totheight, unsigned int * heights, bool uniformheights)
{
	struct widget_layout_gtk3 * this=malloc(sizeof(struct widget_layout_gtk3));
	GtkGrid* grid=GTK_GRID(gtk_grid_new());
	this->i._base.widget=grid;
	this->i._base.free=layout__free;
	
	this->numchildren=numchildren;
	this->children=malloc(sizeof(struct widget_base*)*numchildren);
	memcpy(this->children, children, sizeof(struct widget_base*)*numchildren);
	
	this->i._base.widthprio=0;
	this->i._base.heightprio=0;
	
	bool posused[totheight*totwidth];
	memset(posused, 0, sizeof(posused));
	unsigned int firstempty=0;
	for (unsigned int i=0;i<numchildren;i++)
	{
		while (posused[firstempty]) firstempty++;
		
		unsigned int width=(widths ? widths[i] : 1);
		unsigned int height=(heights ? heights[i] : 1);
		
		gtk_grid_attach(grid, GTK_WIDGET(this->children[i]->widget),
		                firstempty%totwidth, firstempty/totwidth,
		                width, height);
		
		for (unsigned int x=0;x<width ;x++)
		for (unsigned int y=0;y<height;y++)
		{
			posused[firstempty + y*totwidth + x]=true;
		}
		
		if (this->children[i]->widthprio  > this->i._base.widthprio)  this->i._base.widthprio =this->children[i]->widthprio;
		if (this->children[i]->heightprio > this->i._base.heightprio) this->i._base.heightprio=this->children[i]->heightprio;
	}
	
	for (unsigned int i=0;i<numchildren;i++)
	{
		gtk_widget_set_hexpand(GTK_WIDGET(this->children[i]->widget), (this->children[i]->widthprio  == this->i._base.widthprio));
		gtk_widget_set_vexpand(GTK_WIDGET(this->children[i]->widget), (this->children[i]->heightprio == this->i._base.heightprio));
	}
	
	return (struct widget_layout*)this;
}
#endif
#endif
