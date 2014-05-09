//#include<stdint.h>
//#include<time.h>
//static uint64_t nanotime(){
//struct timespec tv;
//clock_gettime(CLOCK_MONOTONIC, &tv);
//return tv.tv_sec*(uint64_t)1000000000 + tv.tv_nsec;}
#include "minir.h"
#ifdef WINDOW_GTK3
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

//http://scentric.net/tutorial/
static GType M_VIRTUAL_TYPE=0;
#define M_VIRTUAL_LIST(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), M_VIRTUAL_TYPE, struct VirtualList))
#define M_IS_VIRTUAL_LIST(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), M_VIRTUAL_TYPE))

struct VirtualList
{
	GObject parent;//leave this one on top
	
	unsigned int rows;
	unsigned int columns;
	
	struct widget_listbox * subject;
	const char * (*get_cell)(struct widget_listbox * subject, unsigned int row, unsigned int column, void * userdata);
	void * userdata;
	
	bool* checkboxes;
};

struct VirtualListClass
{
	GObjectClass parent_class;
};

static GtkTreeModelFlags virtual_list_get_flags(GtkTreeModel* tree_model)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), (GtkTreeModelFlags)0);
	return (GTK_TREE_MODEL_LIST_ONLY|GTK_TREE_MODEL_ITERS_PERSIST);
}

static gint virtual_list_get_n_columns(GtkTreeModel* tree_model)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), 0);
	return M_VIRTUAL_LIST(tree_model)->columns;
}

static GType virtual_list_get_column_type(GtkTreeModel* tree_model, gint index)
{
	g_return_val_if_fail(M_IS_VIRTUAL_LIST(tree_model), G_TYPE_INVALID);
	g_return_val_if_fail(index>=0 && index<M_VIRTUAL_LIST(tree_model)->columns, G_TYPE_INVALID);
	
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
	
	struct VirtualList* virtual_list=M_VIRTUAL_LIST(tree_model);
	uintptr_t row=(uintptr_t)iter->user_data;
	
	if (column == virtual_list->columns)
	{
		g_value_init(value, G_TYPE_BOOLEAN);
		g_value_set_boolean(value, virtual_list->checkboxes[row]);
		return;
	}
	
	g_return_if_fail(column<virtual_list->columns);
	
	
	if (row>=virtual_list->rows) g_return_if_reached();
	
	g_value_init(value, G_TYPE_STRING);
	const char * ret=virtual_list->get_cell(virtual_list->subject, row, column, virtual_list->userdata);
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
	if (n>=virtual_list->rows) return FALSE;
	
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
	unsigned int rows;
	unsigned int columns;
	
	struct VirtualList* vlist;
	struct _widget_listbox_devirt * devirt;
	
	bool* checkboxes;
	void (*ontoggle)(struct widget_listbox * subject, unsigned int row, bool state, void * userdata);
	void* toggle_userdata;
	
	void (*onactivate)(struct widget_listbox * subject, unsigned int row, void * userdata);
	void* activate_userdata;
};

static void listbox_set_checkbox_state(struct widget_listbox * this_, unsigned int row, bool state);

static void listbox_refresh_row(struct widget_listbox_gtk3 * this, unsigned int row)
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
	_widget_listbox_devirt_free(this->devirt);
	free(this->checkboxes);
	free(this);
}

static void listbox_set_enabled(struct widget_listbox * this_, bool enable)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	gtk_widget_set_sensitive(GTK_WIDGET(this->tree), enable);
}

static void listbox_set_contents(struct widget_listbox * this_, unsigned int rows, const char * * contents)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	this->devirt=_widget_listbox_devirt_create(this_, rows, this->columns, contents);
}

static void listbox_set_contents_virtual(struct widget_listbox * this_, unsigned int rows,
                    const char * (*get_cell)(struct widget_listbox * subject, unsigned int row, unsigned int column, void * userdata),
                    int (*search)(struct widget_listbox * subject, unsigned int start, const char * str, void * userdata),
                    void * userdata)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	
	_widget_listbox_devirt_free(this->devirt);
	this->devirt=NULL;
	
	double scrollpos=gtk_adjustment_get_value(gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(this->tree)));
	
	if (this->vlist) g_object_unref(this->vlist);
	
	virtual_list_register_type();
	
	//do not keep the same vlist, the treemodel won't notice that it changed
	//(we could temporarily set it to null, but I don't think there's any point.)
	this->vlist=g_object_new(M_VIRTUAL_TYPE, NULL);
	this->vlist->subject=this_;
	this->vlist->columns=this->columns;
	this->vlist->rows=rows;
	//we ignore the search function, there is no valid way for gtk+ to use it
	this->vlist->get_cell=get_cell;
	this->vlist->userdata=userdata;
	
	this->rows=rows;
	
	//this is piss slow for some reason I can't figure out
	gtk_tree_view_set_model(this->tree, GTK_TREE_MODEL(this->vlist));
	
	GtkAdjustment* adj=gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(this->tree));
	gtk_adjustment_set_value(adj, scrollpos);
	gtk_adjustment_value_changed(adj);//shouldn't it do this by itself?
	
	if (this->checkboxes && rows!=this->rows)
	{
		free(this->checkboxes);
		this->checkboxes=calloc(rows, sizeof(bool));
		this->vlist->checkboxes=this->checkboxes;
	}
}

static void listbox_set_row(struct widget_listbox * this_, unsigned int row, const char * * contents)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	if (this->devirt) _widget_listbox_devirt_set_row(this->devirt, row, contents);
	listbox_refresh_row(this, row);
}

static void listbox_refresh(struct widget_listbox * this_)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	gtk_widget_queue_draw(GTK_WIDGET(this->tree));
}

static void listbox_onactivate(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* column, gpointer user_data)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)user_data;
	if (this->onactivate)
	{
		this->onactivate((struct widget_listbox*)this, gtk_tree_path_get_indices(path)[0], this->activate_userdata);
	}
}

static unsigned int listbox_get_active_row(struct widget_listbox * this_)
{
	return 0;
}

static void listbox_set_onactivate(struct widget_listbox * this_,
                                   void (*onactivate)(struct widget_listbox * subject, unsigned int row, void * userdata),
                                   void* userdata)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	this->onactivate=onactivate;
	this->activate_userdata=userdata;
}

static void listbox_set_size(struct widget_listbox * this_, unsigned int height, const unsigned int * widths)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	if (widths)
	{
		PangoLayout* layout=pango_layout_new(gtk_widget_get_pango_context(GTK_WIDGET(this->tree)));
		for (unsigned int i=0;i<this->columns;i++)
		{
			const char xs[]="xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
			pango_layout_set_text(layout, xs, widths[i]);
			int width;
			pango_layout_get_pixel_size(layout, &width, NULL);
			GtkTreeViewColumn* col=gtk_tree_view_get_column(this->tree, i);
			gtk_tree_view_column_set_fixed_width(col, width);
		}
		g_object_unref(layout);
	}
}

static void listbox_checkbox_toggle(GtkCellRendererToggle* cell_renderer, gchar* path, gpointer user_data)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)user_data;
	unsigned int id=atoi(path);
	listbox_set_checkbox_state((struct widget_listbox*)this, id, this->checkboxes[id]^1);
	if (this->ontoggle) this->ontoggle((struct widget_listbox*)this, id, this->checkboxes[id], this->toggle_userdata);
}

static void listbox_add_checkboxes(struct widget_listbox * this_,
                    void (*ontoggle)(struct widget_listbox * subject, unsigned int id, bool state, void * userdata), void * userdata)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	
	this->checkboxes=calloc(this->rows, sizeof(bool));
	if (this->vlist) this->vlist->checkboxes=this->checkboxes;
	
	GtkCellRenderer* render=gtk_cell_renderer_toggle_new();
	gtk_tree_view_insert_column_with_attributes(this->tree, 0, "", render, "active", this->columns, NULL);
	
	this->ontoggle=ontoggle;
	this->toggle_userdata=userdata;
	g_signal_connect(render, "toggled", G_CALLBACK(listbox_checkbox_toggle), this);
	
	GtkRequisition size;
	gtk_widget_show_all(this->i.base._widget);
	gtk_widget_get_preferred_size(GTK_WIDGET(this->tree), &size, NULL);
	gtk_widget_set_size_request(this->i.base._widget, size.width, -1);
}

static bool listbox_get_checkbox_state(struct widget_listbox * this_, unsigned int id)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	return this->checkboxes[id];
}

static void listbox_set_checkbox_state(struct widget_listbox * this_, unsigned int row, bool state)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	this->checkboxes[row]=state;
	listbox_refresh_row(this, row);
}

static void listbox_set_all_checkboxes(struct widget_listbox * this_, bool * states)
{
	struct widget_listbox_gtk3 * this=(struct widget_listbox_gtk3*)this_;
	memcpy(this->checkboxes, states, sizeof(bool)*this->rows);
	listbox_refresh(this_);
}

struct widget_listbox * widget_create_listbox_l(unsigned int numcolumns, const char * * columns)
{
	struct widget_listbox_gtk3 * this=malloc(sizeof(struct widget_listbox_gtk3));
	this->i.base._widget=gtk_scrolled_window_new(NULL, NULL);
	this->i.base._widthprio=3;
	this->i.base._heightprio=3;
	this->i.base._free=listbox__free;
	
	gtk_scrolled_window_set_policy(this->i.base._widget, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	this->tree=GTK_TREE_VIEW(gtk_tree_view_new());
	gtk_container_add(GTK_CONTAINER(this->i.base._widget), GTK_WIDGET(this->tree));
	
	this->i.set_enabled=listbox_set_enabled;
	this->i.set_contents=listbox_set_contents;
	this->i.set_contents_virtual=listbox_set_contents_virtual;
	this->i.set_row=listbox_set_row;
	this->i.refresh=listbox_refresh;
	this->i.get_active_row=listbox_get_active_row;
	this->i.set_onactivate=listbox_set_onactivate;
	this->i.set_size=listbox_set_size;
	this->i.add_checkboxes=listbox_add_checkboxes;
	this->i.get_checkbox_state=listbox_get_checkbox_state;
	this->i.set_checkbox_state=listbox_set_checkbox_state;
	this->i.set_all_checkboxes=listbox_set_all_checkboxes;
	
	this->rows=0;
	this->checkboxes=NULL;
	
	this->vlist=NULL;
	this->devirt=NULL;
	
	g_signal_connect(this->tree, "row-activated", G_CALLBACK(listbox_onactivate), this);
	this->onactivate=NULL;
	
	this->columns=numcolumns;
	
	GtkCellRenderer* render=gtk_cell_renderer_text_new();
	for (unsigned int i=0;i<numcolumns;i++)
	{
		gtk_tree_view_insert_column_with_attributes(this->tree, -1, columns[i], render, "text", i, NULL);
		GtkTreeViewColumn* col=gtk_tree_view_get_column(this->tree, i);
		gtk_tree_view_column_set_expand(col, true);
		gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
	}
	
	gtk_widget_set_hexpand(this->i.base._widget, true);
	gtk_widget_set_vexpand(this->i.base._widget, true);
	gtk_tree_view_set_fixed_height_mode(this->tree, true);
	
	return (struct widget_listbox*)this;
}
#endif