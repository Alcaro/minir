//#include<stdint.h>
//#include<time.h>
//static uint64_t nanotime(){
//struct timespec tv;
//clock_gettime(CLOCK_MONOTONIC, &tv);
//return tv.tv_sec*(uint64_t)1000000000 + tv.tv_nsec;}
#include "minir.h"
#ifdef WINDOW_GTK3
#if 0
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ROWS 100000 // GtkTreeView seems to be at least O(n log n) for creating a long list. Let's just add a hard cap.

//http://scentric.net/tutorial/
static GType M_VIRTUAL_TYPE=0;
#define M_VIRTUAL_LIST(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), M_VIRTUAL_TYPE, struct VirtualList))
#define M_IS_VIRTUAL_LIST(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), M_VIRTUAL_TYPE))

struct VirtualList
{
	GObject g_parent;//leave this one on top
	
	size_t rows;
	unsigned int columns;
	bool checkboxes;
	
	struct widget_listbox * subject;
	const char * (*get_cell)(struct widget_listbox * subject, size_t row, int column, void * userdata);
	void * get_userdata;
};

struct VirtualListClass
{
	GObjectClass parent_class;
};

static GtkTreeModelFlags virtual_list_get_flags(GtkTreeModel* tree_model)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), (GtkTreeModelFlags)0);
	return (GtkTreeModelFlags)(GTK_TREE_MODEL_LIST_ONLY|GTK_TREE_MODEL_ITERS_PERSIST);
}

static gint virtual_list_get_n_columns(GtkTreeModel* tree_model)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), 0);
	return M_VIRTUAL_LIST(tree_model)->columns;
}

static GType virtual_list_get_column_type(GtkTreeModel* tree_model, gint index)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), G_TYPE_INVALID);
	g_return_val_if_fail(index>=0 && (unsigned int)index<M_VIRTUAL_LIST(tree_model)->columns, G_TYPE_INVALID);
	
	return G_TYPE_STRING;
}

static gboolean virtual_list_get_iter(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreePath* path)
{
	g_assert(M_IS_VIRTUAL_LIST(tree_model));
	g_assert(path!=NULL);
	g_assert(gtk_tree_path_get_depth(path)==1);
	
	struct VirtualList* virtual_list=M_VIRTUAL_LIST(tree_model);
	
	uintptr_t n=gtk_tree_path_get_indices(path)[0];
	if (n>=virtual_list->rows || n<0) return FALSE;
	
	iter->stamp=0;
	iter->user_data=(void*)n;
	iter->user_data2=NULL;//we don't need those two
	iter->user_data3=NULL;
	
	return TRUE;
}

static GtkTreePath* virtual_list_get_path(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), NULL);
	g_return_val_if_fail(iter!=NULL, NULL);
	
	GtkTreePath* path=gtk_tree_path_new();
	gtk_tree_path_append_index(path, (uintptr_t)iter->user_data);
	
	return path;
}

static void virtual_list_get_value(GtkTreeModel* tree_model, GtkTreeIter* iter, gint column, GValue* value)
{
	g_return_if_fail(M_IS_VIRTUAL_LIST(tree_model));
	g_return_if_fail(iter!=NULL);
	g_return_if_fail(column>=0);
	
	struct VirtualList* virtual_list=M_VIRTUAL_LIST(tree_model);
	uintptr_t row=(uintptr_t)iter->user_data;
	
	unsigned int ucolumn=column;
	
	if (row == MAX_ROWS)
	{
		if (ucolumn == virtual_list->columns)
		{
			g_value_init(value, G_TYPE_BOOLEAN);
			g_value_set_boolean(value, false);
			return;
		}
		else
		{
			g_value_init(value, G_TYPE_STRING);
			if(0);
			else if (virtual_list->columns==1) g_value_set_string(value, "(sorry, not supported)");
			else if (virtual_list->columns==2 && column==0) g_value_set_string(value, "(sorry, not");
			else if (virtual_list->columns==2 && column==1) g_value_set_string(value, "supported)");
			else if (virtual_list->columns==3 && column==0) g_value_set_string(value, "(sorry,");
			else if (virtual_list->columns==3 && column==1) g_value_set_string(value, "not");
			else if (virtual_list->columns==3 && column==2) g_value_set_string(value, "supported)");
			else g_value_set_string(value, "");
		}
		return;
	}
	
	if (ucolumn == virtual_list->columns)
	{
		g_value_init(value, G_TYPE_BOOLEAN);
		g_value_set_boolean(value, virtual_list->get_cell(virtual_list->subject, row, -1, virtual_list->get_userdata) ? true : false);
		return;
	}
	
	g_return_if_fail(ucolumn<virtual_list->columns);
	
	
	if (row>=virtual_list->rows) g_return_if_reached();
	
	g_value_init(value, G_TYPE_STRING);
	const char * ret=virtual_list->get_cell(virtual_list->subject, row, ucolumn, virtual_list->get_userdata);
	g_value_set_string(value, ret);
}

static gboolean virtual_list_iter_next(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), FALSE);
	
	if (!iter) return FALSE;
	
	struct VirtualList* virtual_list=M_VIRTUAL_LIST(tree_model);
	
	uintptr_t id=(uintptr_t)iter->user_data;
	if (id>=virtual_list->rows-1) return FALSE;
	
	iter->stamp=0;
	iter->user_data=(void*)(id+1);
	
	return TRUE;
}

static gboolean virtual_list_iter_children(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* parent)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), FALSE);
	
	struct VirtualList* virtual_list=M_VIRTUAL_LIST(tree_model);
	
	if (parent || virtual_list->rows==0) return FALSE;
	
	iter->stamp=0;
	iter->user_data=(void*)(uintptr_t)0;
	
	return TRUE;
}

static gboolean virtual_list_iter_has_child(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
	return FALSE;
}

static gint virtual_list_iter_n_children(GtkTreeModel* tree_model, GtkTreeIter* iter)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), -1);
	g_return_val_if_fail(iter==NULL || iter->user_data!=NULL, FALSE);
	
	struct VirtualList* virtual_list=M_VIRTUAL_LIST(tree_model);
	
	if (iter) return 0;
	return virtual_list->rows;
}

static gboolean virtual_list_iter_nth_child(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* parent, gint n)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), FALSE);
	
	struct VirtualList* virtual_list=M_VIRTUAL_LIST(tree_model);
	
	if (parent) return FALSE;
	if (n<0) return FALSE;
	if ((unsigned int)n>=virtual_list->rows) return FALSE;
	
	iter->stamp=0;
	iter->user_data=(void*)(uintptr_t)n;
	
	return TRUE;
}

static gboolean virtual_list_iter_parent(GtkTreeModel* tree_model, GtkTreeIter* iter, GtkTreeIter* child)
{
	return FALSE;
}

static void virtual_list_tree_model_init (GtkTreeModelIface* iface)
{
	iface->get_flags=virtual_list_get_flags;
	iface->get_n_columns=virtual_list_get_n_columns;
	iface->get_column_type=virtual_list_get_column_type;
	iface->get_iter=virtual_list_get_iter;
	iface->get_path=virtual_list_get_path;
	iface->get_value=virtual_list_get_value;
	iface->iter_next=virtual_list_iter_next;
	iface->iter_children=virtual_list_iter_children;
	iface->iter_has_child=virtual_list_iter_has_child;
	iface->iter_n_children=virtual_list_iter_n_children;
	iface->iter_nth_child=virtual_list_iter_nth_child;
	iface->iter_parent=virtual_list_iter_parent;
}

static void virtual_list_register_type()
{
	if (!M_VIRTUAL_TYPE)
	{
		static const GTypeInfo virtual_list_info={
			sizeof(struct VirtualListClass),
			NULL, NULL, NULL, NULL, NULL,
			sizeof(struct VirtualList), 0, NULL };
		static const GInterfaceInfo tree_model_info={ (GInterfaceInitFunc)virtual_list_tree_model_init, NULL, NULL };
		
		M_VIRTUAL_TYPE=g_type_register_static (G_TYPE_OBJECT, "MinirVirtualList", &virtual_list_info, (GTypeFlags)0);
		g_type_add_interface_static(M_VIRTUAL_TYPE, GTK_TYPE_TREE_MODEL, &tree_model_info);
	}
}





struct widget_listbox_gtk3 {
	struct widget_listbox i;
	
	GtkTreeView* tree;
	gint borderheight;
	gint cellheight;
	
	struct VirtualList* vlist;
	
	void (*ontoggle)(struct widget_listbox * subject, size_t row, void * userdata);
	void* toggle_userdata;
	
	void (*onfocus)(struct widget_listbox * subject, size_t row, void * userdata);
	void* focus_userdata;
	
	void (*onactivate)(struct widget_listbox * subject, size_t row, void * userdata);
	void* activate_userdata;
};

static void listbox_refresh_row(struct widget_listbox_gtk3 * this, size_t row)
{
	GtkTreeIter iter;
	gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(this->vlist), &iter, NULL, row);
	GtkTreePath* path=gtk_tree_path_new_from_indices(row, -1);
	gtk_tree_model_row_changed(GTK_TREE_MODEL(this->vlist), path, &iter);
	gtk_tree_path_free(path);
}

static void listbox__free(struct widget_base * this_)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	free(this);
}

static void listbox_set_enabled(struct widget_listbox * this_, bool enable)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	gtk_widget_set_sensitive(GTK_WIDGET(this->tree), enable);
}

static void listbox_set_contents(struct widget_listbox * this_,
                                 const char * (*get_cell)(struct widget_listbox * subject, size_t row, int column, void * userdata),
                                 size_t (*search)(struct widget_listbox * subject,
                                                  const char * prefix, size_t start, bool up, void * userdata),
                                 void * userdata)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	
	//we ignore the search function, there is no valid way for gtk+ to use it
	this->vlist->get_cell=get_cell;
	this->vlist->get_userdata=userdata;
}

static void listbox_set_num_rows(struct widget_listbox * this_, size_t rows)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	
	if (rows > MAX_ROWS) rows=MAX_ROWS+1;
	
	GtkAdjustment* adj=gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(this->tree));
	double scrollfrac=gtk_adjustment_get_value(adj) / this->vlist->rows;
	
	//this is piss slow for some reason I can't figure out
	gtk_tree_view_set_model(this->tree, GTK_TREE_MODEL(NULL));
	this->vlist->rows=rows;
	gtk_tree_view_set_model(this->tree, GTK_TREE_MODEL(this->vlist));
	
	if (scrollfrac==scrollfrac)
	{
		gtk_adjustment_set_upper(adj, scrollfrac * this->vlist->rows + gtk_adjustment_get_page_size(adj));
		gtk_adjustment_changed(adj);
		gtk_adjustment_set_value(adj, scrollfrac * this->vlist->rows);
		gtk_adjustment_value_changed(adj);//shouldn't it do this by itself?
	}
}

static void listbox_refresh(struct widget_listbox * this_, size_t row)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	if (row==(size_t)-1) gtk_widget_queue_draw(GTK_WIDGET(this->tree));
	else listbox_refresh_row(this, row);
}

static size_t listbox_get_active_row(struct widget_listbox * this_)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	GList* list=gtk_tree_selection_get_selected_rows(gtk_tree_view_get_selection(this->tree), NULL);
	size_t ret;
	if (list) ret=gtk_tree_path_get_indices((GtkTreePath*)list->data)[0];
	else ret=(size_t)-1;
	if (ret==MAX_ROWS) ret=(size_t)-1;
	g_list_free_full(list, (GDestroyNotify)gtk_tree_path_free);
	return ret;
}

static void listbox_on_focus_change(GtkTreeView* tree_view, gpointer user_data)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)user_data;
	if (this->onfocus)
	{
		GtkTreePath* path;
		gtk_tree_view_get_cursor(this->tree, &path, NULL);
		size_t item=(size_t)-1;
		if (path) item=gtk_tree_path_get_indices(path)[0];
		if (item==MAX_ROWS) item=(size_t)-1;
		this->onfocus((struct widget_listbox*)this, item, this->focus_userdata);
		if (path) gtk_tree_path_free(path);
	}
}

static void listbox_set_on_focus_change(struct widget_listbox * this_,
                                        void (*onchange)(struct widget_listbox * subject, size_t row, void * userdata),
                                        void* userdata)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	this->onfocus=onchange;
	this->focus_userdata=userdata;
}

static void listbox_onactivate(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* column, gpointer user_data)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)user_data;
	if (this->onactivate)
	{
		int item=gtk_tree_path_get_indices(path)[0];
		if (item!=MAX_ROWS)
		{
			this->onactivate((struct widget_listbox*)this, item, this->activate_userdata);
		}
	}
}

static void listbox_set_onactivate(struct widget_listbox * this_,
                                   void (*onactivate)(struct widget_listbox * subject, size_t row, void * userdata),
                                   void* userdata)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	this->onactivate=onactivate;
	this->activate_userdata=userdata;
}

static void listbox_set_size(struct widget_listbox * this_, unsigned int height, const unsigned int * widths, int expand)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	if (widths)
	{
		PangoLayout* layout=pango_layout_new(gtk_widget_get_pango_context(GTK_WIDGET(this->tree)));
		for (unsigned int i=0;i<this->vlist->columns;i++)
		{
			const char xs[]="XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
			pango_layout_set_text(layout, xs, widths[i]);
			int width;
			pango_layout_get_pixel_size(layout, &width, NULL);
			GtkTreeViewColumn* col=gtk_tree_view_get_column(this->tree, i);
			//if (i==expand) width=-1;
			gtk_tree_view_column_set_fixed_width(col, width + 10);
		}
		g_object_unref(layout);
	}
	if (height)
	{
		//this->cellheight
	}
	//gtktreeview height
	//TODO: figure out height
	if (expand!=-1)
	{
		for (unsigned int i=0;i<this->vlist->columns;i++)
		{
			gtk_tree_view_column_set_expand(gtk_tree_view_get_column(this->tree, i), (i==(unsigned int)expand));
			//gtk_tree_view_column_set_expand(gtk_tree_view_get_column(this->tree, i), 1);
		}
	}
}

static void listbox_checkbox_toggle(GtkCellRendererToggle* cell_renderer, gchar* path, gpointer user_data)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)user_data;
	unsigned int row=atoi(path);
	if (this->ontoggle) this->ontoggle((struct widget_listbox*)this, row, this->toggle_userdata);
	listbox_refresh_row(this, row);
}

static void listbox_add_checkboxes(struct widget_listbox * this_,
                                   void (*ontoggle)(struct widget_listbox * subject, size_t id, void * userdata),
                                   void * userdata)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	
	this->vlist->checkboxes=true;
	
	GtkCellRenderer* render=gtk_cell_renderer_toggle_new();
	gtk_tree_view_insert_column_with_attributes(this->tree, 0, "", render, "active", this->vlist->columns, NULL);
	
	gint checkheight;
	g_object_get(render, "height", &checkheight, NULL);
	if (checkheight>this->cellheight) this->cellheight=checkheight;
	
	this->ontoggle=ontoggle;
	this->toggle_userdata=userdata;
	g_signal_connect(render, "toggled", G_CALLBACK(listbox_checkbox_toggle), this);
	
	GtkRequisition size;
	gtk_widget_show_all(GTK_WIDGET(this->i._base.widget));
	gtk_widget_get_preferred_size(GTK_WIDGET(this->tree), &size, NULL);
	gtk_widget_set_size_request(GTK_WIDGET(this->i._base.widget), size.width, -1);
}

struct widget_listbox * widget_create_listbox_l(unsigned int numcolumns, const char * * columns)
{
	struct widget_listbox_gtk3 * this=malloc(sizeof(struct widget_listbox_gtk3));
	this->i._base.widget=gtk_scrolled_window_new(NULL, NULL);
	this->i._base.widthprio=3;
	this->i._base.heightprio=3;
	this->i._base.free=listbox__free;
	
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(this->i._base.widget), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	this->tree=GTK_TREE_VIEW(gtk_tree_view_new());
	gtk_container_add(GTK_CONTAINER(this->i._base.widget), GTK_WIDGET(this->tree));
	
	this->i.set_enabled=listbox_set_enabled;
	this->i.set_contents=listbox_set_contents;
	this->i.set_num_rows=listbox_set_num_rows;
	this->i.refresh=listbox_refresh;
	this->i.get_active_row=listbox_get_active_row;
	this->i.set_on_focus_change=listbox_set_on_focus_change;
	this->i.set_onactivate=listbox_set_onactivate;
	this->i.set_size=listbox_set_size;
	this->i.add_checkboxes=listbox_add_checkboxes;
	
	virtual_list_register_type();
	this->vlist=(VirtualList*)g_object_new(M_VIRTUAL_TYPE, NULL);
	this->vlist->subject=(struct widget_listbox*)this;
	
	this->vlist->columns=numcolumns;
	this->vlist->rows=0;
	this->vlist->checkboxes=false;
	
	g_signal_connect(this->tree, "row-activated", G_CALLBACK(listbox_onactivate), this);
	this->onactivate=NULL;
	g_signal_connect(this->tree, "cursor-changed", G_CALLBACK(listbox_on_focus_change), this);
	this->onfocus=NULL;
	
	GtkCellRenderer* render=gtk_cell_renderer_text_new();
	g_object_get(render, "height", &this->cellheight, NULL);
	for (unsigned int i=0;i<numcolumns;i++)
	{
		gtk_tree_view_insert_column_with_attributes(this->tree, -1, columns[i], render, "text", i, NULL);
		GtkTreeViewColumn* col=gtk_tree_view_get_column(this->tree, i);
		gtk_tree_view_column_set_expand(col, true);
		gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
	}
	
	//gtk_widget_set_hexpand(this->i._base.widget, true);
	//gtk_widget_set_vexpand(this->i._base.widget, true);
	gtk_tree_view_set_fixed_height_mode(this->tree, true);
	
	gtk_widget_show_all(GTK_WIDGET(this->tree));
	GtkRequisition size;
	gtk_widget_get_preferred_size(GTK_WIDGET(this->tree), NULL, &size);
	this->borderheight=size.height;
	
	return (struct widget_listbox*)this;
}
#endif
#endif
