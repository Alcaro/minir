#include "minir.h"
#ifdef WINDOW_GTK3
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gtk/gtk.h>
#ifdef WNDPROT_X11
#include <gdk/gdkx.h>
#endif

void
window_firstrun
()
{
GtkWidget*dialog=gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
	"This piece of software is far from finished.\r\n"
	"One missing feature is that there is no configuration panel, so most options require "
	  "pointing a text editor at minir.cfg (it'll appear once you close this program).\r\n"
	"Error messages if the given ROMs or cores weren't understood are not guaranteed, either.\r\n"
	"\r\n"
	"Finally, please do not redistribute minir without the author's permission. (This rule will be relaxed once it's less unfinished.)"
);
gtk_dialog_run(GTK_DIALOG(dialog));
gtk_widget_destroy(dialog);
}

//static GdkFilterReturn scanfilter(GdkXEvent* xevent, GdkEvent* event, gpointer data)
//{
//	XEvent* ev=(XEvent*)xevent;
//	if (ev->type==Expose) printf("ex=%lu\n", ev->xexpose.window);
//	return GDK_FILTER_CONTINUE;
//}

#include<sys/resource.h>
void window_init(int * argc, char * * argv[])
{
//struct rlimit core_limits;core_limits.rlim_cur=core_limits.rlim_max=64*1024*1024;setrlimit(RLIMIT_CORE,&core_limits);
g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING);
#ifdef WNDPROT_X11
	XInitThreads();
#endif
	gtk_init(argc, argv);
	//gdk_window_add_filter(NULL,scanfilter,NULL);
#ifndef NO_ICON
	struct image img;
	png_decode(icon_minir_64x64_png,sizeof(icon_minir_64x64_png), &img, 33);
	//we could tell it how to free this, but the default icon will exist until replaced, and it won't be replaced.
	gtk_window_set_default_icon(gdk_pixbuf_new_from_data(img.pixels, GDK_COLORSPACE_RGB, true, 8, 64,64, 64*4, NULL, NULL));
#endif
}

static void * mem_from_g_alloc(void * mem, size_t size)
{
	if (g_mem_is_system_malloc()) return mem;
	
	if (!size) size=strlen((char*)mem)+1;
	
	void * ret=malloc(size);
	memcpy(ret, mem, size);
	g_free(ret);
	return ret;
}

const char * const * window_file_picker(struct window * parent,
                                        const char * title,
                                        const char * const * extensions,
                                        const char * extdescription,
                                        bool dylib,
                                        bool multiple)
{
	static char * * ret=NULL;
	if (ret)
	{
		char * * delete=ret;
		while (*delete)
		{
			free(*delete);
			delete++;
		}
		free(ret);
		ret=NULL;
	}
	
	GtkFileChooser* dialog=GTK_FILE_CHOOSER(
	                         gtk_file_chooser_dialog_new(
	                           title,
	                           (parent?parent->_get_handle(parent):NULL),
	                           GTK_FILE_CHOOSER_ACTION_OPEN,
	                           GTK_STOCK_CANCEL,
	                           GTK_RESPONSE_CANCEL,
	                           GTK_STOCK_OPEN,
	                           GTK_RESPONSE_ACCEPT,
	                           NULL));
	gtk_file_chooser_set_select_multiple(dialog, multiple);
	gtk_file_chooser_set_local_only(dialog, dylib);
	
	GtkFileFilter* filter;
	
	if (*extensions)
	{
		filter=gtk_file_filter_new();
		gtk_file_filter_set_name(filter, extdescription);
		char extstr[64];
		extstr[0]='*';
		extstr[1]='.';
		while (*extensions)
		{
			strcpy(extstr+2, *extensions+(**extensions=='.'));
			gtk_file_filter_add_pattern(filter, extstr);
			extensions++;
		}
		gtk_file_chooser_add_filter(dialog, filter);
	}
	
	filter=gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "All files");
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	
	if (gtk_dialog_run(GTK_DIALOG(dialog))!=GTK_RESPONSE_ACCEPT)
	{
		gtk_widget_destroy(GTK_WIDGET(dialog));
		return NULL;
	}
	
	GSList * list=gtk_file_chooser_get_uris(dialog);
	gtk_widget_destroy(GTK_WIDGET(dialog));
	unsigned int listlen=g_slist_length(list);
	if (!listlen)
	{
		g_slist_free(list);
		return NULL;
	}
	ret=malloc(sizeof(char*)*(listlen+1));
	
	char * * retcopy=ret;
	GSList * listcopy=list;
	while (listcopy)
	{
		*retcopy=window_get_absolute_path(listcopy->data);
		g_free(listcopy->data);
		retcopy++;
		listcopy=listcopy->next;
	}
	ret[listlen]=NULL;
	g_slist_free(list);
	return (const char * const *)ret;
}

void window_run_iter()
{
	gtk_main_iteration_do(false);
	
	//Workaround for GTK thinking we're processing events slower than they come in. We're busy waiting
	// for the AV drivers, and waiting less costs us nothing.
	gtk_main_iteration_do(false);
}

void window_run_wait()
{
	gtk_main_iteration_do(true);
}

#ifdef WNDPROT_X11
static struct window_x11_display display={NULL,0};

const struct window_x11_display * window_x11_get_display()
{
	if (!display.display)
	{
		display.display=gdk_x11_get_default_xdisplay();
		display.screen=gdk_x11_get_default_screen();
	}
	return &display;
}
#endif

char * window_get_absolute_path(const char * path)
{
	if (!path) return NULL;
	GFile* file=g_file_new_for_commandline_arg(path);
	gchar * ret;
	if (g_file_is_native(file)) ret=g_file_get_path(file);
	else ret=g_file_get_uri(file);
	g_object_unref(file);
	if (!ret) return NULL;
	return mem_from_g_alloc(ret, 0);
}

char * window_get_native_path(const char * path)
{
	if (!path) return NULL;
	GFile* file=g_file_new_for_commandline_arg(path);
	gchar * ret=g_file_get_path(file);
	g_object_unref(file);
	if (!ret) return NULL;
	return mem_from_g_alloc(ret, 0);
}

uint64_t window_get_time()
{
	return g_get_monotonic_time();
}



bool file_read(const char * filename, char* * data, size_t * len)
{
	if (!filename) return false;
	GFile* file=g_file_new_for_commandline_arg(filename);
	if (!file) return false;
	
	char* ret;
	gsize glen;
	if (!g_file_load_contents(file, NULL, &ret, &glen, NULL, NULL))
	{
		g_object_unref(file);
		return false;
	}
	g_object_unref(file);
	if (len) *len=glen;
	*data=mem_from_g_alloc(ret, glen);
	return true;
}

bool file_write(const char * filename, const char * data, size_t len)
{
	if (!filename) return false;
	if (!len) return true;
	GFile* file=g_file_new_for_commandline_arg(filename);
	if (!file) return false;
	bool success=g_file_replace_contents(file, data, len, NULL, false, G_FILE_CREATE_NONE, NULL, NULL, NULL);
	g_object_unref(file);
	return success;
}

bool file_read_to(const char * filename, char * data, size_t len)
{
	if (!filename) return false;
	if (!len) return true;
	GFile* file=g_file_new_for_commandline_arg(filename);
	if (!file) return false;
	GFileInputStream* io=g_file_read(file, NULL, NULL);
	if (!io)
	{
		g_object_unref(file);
		return false;
	}
	GFileInfo* info=g_file_input_stream_query_info(io, G_FILE_ATTRIBUTE_STANDARD_SIZE, NULL, NULL);
	gsize size=g_file_info_get_size(info);
	if (size!=len) return false;
	gsize actualsize;
	bool success=g_input_stream_read_all(G_INPUT_STREAM(io), data, size, &actualsize, NULL, NULL);
	g_input_stream_close(G_INPUT_STREAM(io), NULL, NULL);
	g_object_unref(file);
	g_object_unref(io);
	g_object_unref(info);
	if (!success || size!=actualsize)
	{
		memset(data, 0, len);
		return false;
	}
	return true;
}


void* file_find_create(const char * path)
{
	if (!path) return NULL;
	GFile* parent=g_file_new_for_path(path);
	GFileEnumerator* children=g_file_enumerate_children(parent, G_FILE_ATTRIBUTE_STANDARD_NAME","G_FILE_ATTRIBUTE_STANDARD_TYPE,
	                                                    G_FILE_QUERY_INFO_NONE, NULL, NULL);
	g_object_unref(parent);
	return children;
}

bool file_find_next(void* find, char * * path, bool * isdir)
{
	if (!find) return false;
	GFileEnumerator* children=(GFileEnumerator*)find;
	
	GFileInfo* child=g_file_enumerator_next_file(children, NULL, NULL);
	if (!child) return false;
	
	*path=strdup(g_file_info_get_name(child));
	*isdir=(g_file_info_get_file_type(child)==G_FILE_TYPE_DIRECTORY);
	g_object_unref(child);
	return true;
}

void file_find_close(void* find)
{
	if (!find) return;
	g_object_unref((GFileEnumerator*)find);
}
#endif
