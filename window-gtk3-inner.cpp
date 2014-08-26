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



widget_label::widget_label(const char * text)
{
	widget=gtk_label_new(text);
	widthprio=1;
	heightprio=1;
}

widget_label::~widget_label() {}

widget_label* widget_label::set_alignment(int alignment)
{
	gtk_misc_set_alignment(GTK_MISC(widget), ((float)alignment)/2, 0.5);
	return this;
}



struct widget_button::impl {
	void (*onclick)(struct widget_button * button, void* userdata);
	void* userdata;
};

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



struct widget_radio::impl {
	GtkLabel* label;
	
	unsigned int grouplen;
	widget_radio * * group;
	
	unsigned int id;//if state is set before grouping, this is used as state
	widget_radio * parent;
	
	void (*onclick)(struct widget_radio * subject, unsigned int state, void* userdata);
	void* userdata;
};

static void widget_radio_onclick(GtkToggleButton* togglebutton, gpointer user_data);
widget_radio::widget_radio(const char * text) : m(new impl)
{
	widget=gtk_radio_button_new(NULL);
	g_signal_connect(widget, "toggled", G_CALLBACK(widget_radio_onclick), this);
	m->label=GTK_LABEL(gtk_label_new(text));
	gtk_container_add(GTK_CONTAINER(widget), GTK_WIDGET(m->label));
	widthprio=1;
	heightprio=1;
	
	m->group=NULL;
}

widget_radio::~widget_radio()
{
	free(m->group);
	delete m;
}

widget_radio* widget_radio::set_enabled(bool enable)
{
	gtk_widget_set_sensitive(GTK_WIDGET(widget), enable);
	return this;
}

widget_radio* widget_radio::set_text(const char * text)
{
	gtk_label_set_text(m->label, text);
	return this;
}

widget_radio* widget_radio::group(unsigned int numitems, widget_radio * * group)
{
	m->parent=this;
	for (unsigned int i=1;i<numitems;i++)
	{
		group[i]->m->parent=this;
		group[i]->m->id=i;
		gtk_radio_button_join_group(GTK_RADIO_BUTTON(group[i]->widget), GTK_RADIO_BUTTON(group[i-1]->widget));
	}
	m->group=malloc(sizeof(widget_radio*)*numitems);
	memcpy(m->group, group, sizeof(widget_radio*)*numitems);
	m->grouplen=numitems;
	in_callback=true;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(group[m->id]->widget), true);
	m->id=0;
	in_callback=false;
	return this;
}

unsigned int widget_radio::get_state()
{
	unsigned int i=0;
	while (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m->group[i]->widget))) i++;
	return i;
}

widget_radio* widget_radio::set_state(unsigned int state)
{
	if (m->group)
	{
		in_callback=true;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m->group[state]->widget), true);
		in_callback=false;
	}
	else
	{
		m->id=state;
	}
	return this;
}

static void widget_radio_onclick(GtkToggleButton* togglebutton, gpointer user_data)
{
	if (in_callback) return;
	widget_radio * obj=(widget_radio*)user_data;
	if (!gtk_toggle_button_get_active(togglebutton)) return;
	obj->m->parent->m->onclick(obj, obj->m->id, obj->m->parent->m->userdata);
}

widget_radio* widget_radio::set_onclick(void (*onclick)(widget_radio * subject, unsigned int state, void* userdata), void* userdata)
{
	m->onclick=onclick;
	m->userdata=userdata;
	return this;
}



struct widget_textbox::impl {
	void (*onchange)(struct widget_textbox * subject, const char * text, void* userdata);
	void* ch_userdata;
	void (*onactivate)(struct widget_textbox * subject, const char * text, void* userdata);
	void* ac_userdata;
};

static void widget_textbox_onchange(GtkEntry* entry, gpointer user_data);
widget_textbox::widget_textbox() : m(new impl)
{
	widget=gtk_entry_new();
	widthprio=3;
	heightprio=1;
	
	gtk_entry_set_width_chars(GTK_ENTRY(widget), 5);
	
	g_signal_connect(widget, "changed", G_CALLBACK(widget_textbox_onchange), this);
	m->onchange=NULL;
}

widget_textbox::~widget_textbox()
{
	delete m;
}

widget_textbox* widget_textbox::set_enabled(bool enable)
{
	gtk_widget_set_sensitive(GTK_WIDGET(widget), enable);
	return this;
}

widget_textbox* widget_textbox::focus()
{
	gtk_widget_grab_focus(GTK_WIDGET(widget));
	return this;
}

widget_textbox* widget_textbox::set_text(const char * text)
{
	gtk_entry_set_text(GTK_ENTRY(widget), text);
	return this;
}

widget_textbox* widget_textbox::set_length(unsigned int maxlen)
{
	gtk_entry_set_max_length(GTK_ENTRY(widget), maxlen);
	return this;
}

widget_textbox* widget_textbox::set_width(unsigned int xs)
{
	gtk_entry_set_width_chars(GTK_ENTRY(widget), xs);
	return this;
}

widget_textbox* widget_textbox::set_invalid(bool invalid)
{
	if (invalid)
	{
		GtkStyleContext* context=gtk_widget_get_style_context(GTK_WIDGET(widget));
		gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(cssprovider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		gtk_widget_set_name(GTK_WIDGET(widget), "invalid");
		gtk_widget_grab_focus(GTK_WIDGET(widget));
	}
	else
	{
		gtk_widget_set_name(GTK_WIDGET(widget), "x");
	}
	return this;
}

const char * widget_textbox::get_text()
{
	return gtk_entry_get_text(GTK_ENTRY(widget));
}

static void widget_textbox_onchange(GtkEntry* entry, gpointer user_data)
{
	widget_textbox * obj=(widget_textbox*)user_data;
	gtk_widget_set_name(GTK_WIDGET(obj->widget), "x");
	if (obj->m->onchange)
	{
		obj->m->onchange(obj, gtk_entry_get_text(GTK_ENTRY(obj->widget)), obj->m->ch_userdata);
	}
}

widget_textbox* widget_textbox::set_onchange(void (*onchange)(struct widget_textbox * subject, const char * text, void* userdata),
                                             void* userdata)
{
	m->onchange=onchange;
	m->ch_userdata=userdata;
	return this;
}

static void widget_textbox_onactivate(GtkEntry* entry, gpointer user_data)
{
	widget_textbox * obj=(widget_textbox*)user_data;
	obj->m->onactivate(obj, gtk_entry_get_text(GTK_ENTRY(obj->widget)), obj->m->ac_userdata);
}

widget_textbox* widget_textbox::set_onactivate(void (*onactivate)(struct widget_textbox * subject, const char * text, void* userdata),
                                               void* userdata)
{
	g_signal_connect(widget, "activate", G_CALLBACK(widget_textbox_onactivate), this);
	m->onactivate=onactivate;
	m->ac_userdata=userdata;
	return this;
}



class widget_canvas;



#if 0
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
#endif



class widget_listbox;



struct widget_frame::impl {
	struct widget_base * child;
};

widget_frame::widget_frame(const char * text, widget_base* child) : m(new impl)
{
	widget=gtk_frame_new(text);
	widthprio=child->widthprio;
	heightprio=child->heightprio;
	m->child=child;
	gtk_container_add(GTK_CONTAINER(widget), GTK_WIDGET(child->widget));
}

widget_frame::~widget_frame()
{
	delete m->child;
	delete m;
}

widget_frame* widget_frame::set_text(const char * text)
{
	gtk_frame_set_label(GTK_FRAME(m->child), text);
	return this;
}



struct widget_layout::impl {
	widget_base * * children;
	unsigned int numchildren;
};

void widget_layout::construct(unsigned int numchildren, widget_base * * children,
                             unsigned int totwidth,  unsigned int * widths,  bool uniformwidths,
                             unsigned int totheight, unsigned int * heights, bool uniformheights)
{
	m=new impl;
	
	GtkGrid* grid=GTK_GRID(gtk_grid_new());
	widget=grid;
	
	m->numchildren=numchildren;
	m->children=malloc(sizeof(struct widget_base*)*numchildren);
	memcpy(m->children, children, sizeof(struct widget_base*)*numchildren);
	
	widthprio=0;
	heightprio=0;
	
	bool posused[totheight*totwidth];
	memset(posused, 0, sizeof(posused));
	unsigned int firstempty=0;
	for (unsigned int i=0;i<numchildren;i++)
	{
		while (posused[firstempty]) firstempty++;
		
		unsigned int width=(widths ? widths[i] : 1);
		unsigned int height=(heights ? heights[i] : 1);
		
		gtk_grid_attach(grid, GTK_WIDGET(children[i]->widget),
		                firstempty%totwidth, firstempty/totwidth,
		                width, height);
		
		for (unsigned int x=0;x<width ;x++)
		for (unsigned int y=0;y<height;y++)
		{
			posused[firstempty + y*totwidth + x]=true;
		}
		
		if (children[i]->widthprio  > widthprio)  widthprio =children[i]->widthprio;
		if (children[i]->heightprio > heightprio) heightprio=children[i]->heightprio;
	}
	
	for (unsigned int i=0;i<numchildren;i++)
	{
		gtk_widget_set_hexpand(GTK_WIDGET(children[i]->widget), (children[i]->widthprio  == widthprio));
		gtk_widget_set_vexpand(GTK_WIDGET(children[i]->widget), (children[i]->heightprio == heightprio));
	}
}

widget_layout::~widget_layout()
{
	for (unsigned int i=0;i<m->numchildren;i++)
	{
		delete m->children[i];
	}
	free(m->children);
	delete m;
}
#endif
