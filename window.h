struct window;
struct windowmenu;
struct widget_base;
struct widget_padding;
struct widget_label;
struct widget_button;
struct widget_checkbox;
struct widget_radio;
struct widget_textbox;
struct widget_canvas;
struct widget_viewport;
struct widget_listbox;
struct widget_frame;
struct widget_layout;

//This must be called before calling any other window_*, before creating any interface that does any I/O, before calling anything from
// the malloc() family, and before using argc/argv; basically, before doing anything else. It should be the first thing main() does.
//It does the following actions, in whatever order makes sense:
//- Initialize the window system, if needed
//- Read off any arguments it recognizes (if any), and delete them (for example, it takes care of --display and a few others on GTK+)
//- Convert argv[0] to the standard path format, if needed (hi Windows)
void window_init(int * argc, char * * argv[]);

//window toolkit is not choosable at runtime
//It is safe to interact with this window while inside its callbacks, with the exception that you may not free it.
//You may also not use window_run_*().
struct window {
	//Marks the window as a popup dialog. This makes it act differently in some ways.
	//For example, poking Escape will close it, and it may or may not get a thinner window border.
	//Must be called before the first call to show(). Can't be undone, and can't be called multiple times.
	void (*set_is_dialog)(struct window * this);
	
	//Sets which window created this one. This can, for example, center it on top of the parent.
	//Should generally be combined with set_is_popup.
	//Must be called before the first call to show(). Can't be undone, and can't be called multiple times.
	void (*set_parent)(struct window * this, struct window * parent);
	
	//newwidth and newheight are the content size, excluding menues/toolbars/etc.
	//If there is any widget whose size is unknown inside, then the sizes may only be used in resize(), and for relative measurements.
	//It is allowed to call resize() on unresizable windows, but changing the size of
	// any contents (changing a label text, for example) will resize it to minimum.
	//If resizable, the resize callback is called after the window is resized and everything is set to the new sizes.
	void (*resize)(struct window * this, unsigned int width, unsigned int height);
	void (*set_resizable)(struct window * this, bool resizable,
	                      void (*onresize)(struct window * subject, unsigned int newwidth, unsigned int newheight, void* userdata),
	                      void* userdata);
	
	void (*set_title)(struct window * this, const char * title);
	
	//The callback tells whether the close request should be honored; true for close, false for keep.
	//The window is only hidden, not deleted; you can use show() again later.
	void (*onclose)(struct window * this, bool (*function)(struct window * subject, void* userdata), void* userdata);
	
	//Appends a menu bar to the top of the window. If the window has a menu already, it's replaced. NULL removes the menu.
	//There's no real reason to replace it, though. Just change it.
	//The given widget must be a topmenu.
	void (*set_menu)(struct window * this, struct windowmenu * menu);
	
	//Creates a status bar at the bottom of the window. It is undefined what happens if numslots equals or exceeds 32.
	//align is how each string is aligned; 0 means touch the left side, 1 means centered, 2 means touch the right side.
	//dividerpos is in 240ths of the window size. Values 0 and 240, as well as
	// a divider position to the left of the previous one, yield undefined behaviour.
	//dividerpos[numslots-1] is ignored; the status bar always covers the entire width of the window.
	//It is implementation defined whether the previous status bar strings remain, or if you must use statusbar_set again.
	//It is implementation defined whether dividers will be drawn. However, it is guaranteed
	// that the implementation will look like the rest of the operating system, as far as that's feasible.
	//It is implementation defined what exactly happens if a string is too
	// long to fit; however, it is guaranteed to show as much as it can.
	//To remove the status bar, set numslots to 0.
	void (*statusbar_create)(struct window * this, int numslots, const int * align, const int * dividerpos);
	//Sets a string on the status bar. The index is zero-based. All strings are initially blank.
	void (*statusbar_set)(struct window * this, int slot, const char * text);
	
	//This replaces the contents of a window.
	void (*replace_contents)(struct window * this, void * contents);
	
	//Setting a window visible while it already is will do nothing.
	void (*set_visible)(struct window * this, bool visible);
	bool (*is_visible)(struct window * this);
	
	//Call only after making the window visible.
	void (*focus)(struct window * this);
	
	//If the menu is active, the window is considered not active.
	//If the menu doesn't exist, it is considered not active.
	//If the window is hidden, results are undefined.
	bool (*is_active)(struct window * this);
	bool (*menu_active)(struct window * this);
	
	//This will also remove the window from the screen, if it's visible.
	void (*free)(struct window * this);
	
	
	//Returns a native handle to the window. It is implementation defined what this native handle is, or if it's implemented at all.
	void* (*_get_handle)(struct window * this);
	//Repositions the window contents. May not necessarily be implemented, if reflow requests are detected in other ways.
	void (*_reflow)(struct window * this);
};
struct window * window_create(void * contents);


//No state may be set on any menu item until it's been added into a parent menu; however, adding or removing children is allowed.
struct windowmenu {
	void (*set_enabled)(struct windowmenu * this, bool enable);
	UNION_BEGIN
		//This applies to check and radio.
		STRUCT_BEGIN
			//For check, 0 is unchecked and 1 is checked. For radio, it's the 0-based index of the checked item.
			//Setting a state to something out of range is undefined behaviour.
			unsigned int (*get_state)(struct windowmenu * this);
			void (*set_state)(struct windowmenu * this, unsigned int state);
		STRUCT_END
		//This applies to submenu and topmenu.
		STRUCT_BEGIN
			//The positions are 0-based.
			void (*insert_child)(struct windowmenu * this, struct windowmenu * child, unsigned int pos);
			void (*remove_child)(struct windowmenu * this, struct windowmenu * child);
		STRUCT_END
		//If anything is not covered by the above, none of them are allowed.
		//Additionally, it's not allowed to disable separators and the topmenu.
	UNION_END
	//To free these structures, remove them from their parent.
	//You can not remove a menu or submenu and attach it elsewhere.
};
//If the text starts with an underscore, the next underscore will indicate the keyboard shortcut for this item.
struct windowmenu * windowmenu_create_item(const char * text,
                                           void (*onactivate)(struct windowmenu * subject, void* userdata),
                                           void* userdata);
struct windowmenu * windowmenu_create_check(const char * text,
                                            void (*onactivate)(struct windowmenu * subject, bool checked, void* userdata),
                                            void* userdata);
//A radio item counts as one item internally, but looks like multiple.
struct windowmenu * windowmenu_create_radio(void (*onactivate)(struct windowmenu * subject, unsigned int state, void* userdata),
                                            void* userdata, const char * firsttext, ...);
struct windowmenu * windowmenu_create_radio_l(unsigned int numitems, const char * * texts,
                                              void (*onactivate)(struct windowmenu * subject, unsigned int state, void* userdata),
                                              void* userdata);
struct windowmenu * windowmenu_create_separator();
struct windowmenu * windowmenu_create_submenu(const char * text, struct windowmenu * firstchild, ...);
struct windowmenu * windowmenu_create_submenu_l(const char * text, unsigned int numchildren, struct windowmenu * * children);
struct windowmenu * windowmenu_create_topmenu(struct windowmenu * firstchild, ...);
struct windowmenu * windowmenu_create_topmenu_l(unsigned int numchildren, struct windowmenu * * children);



//Each widget is owned by the layout or window it's put in (layouts own each other). Deleting the window deletes the widget.
//Each widget has a few shared base functions that can be called without knowing what
// type of widget this is. However, they should all be seen as implementation details.
//It is undefined behaviour to set a callback to NULL. (It is, however, allowed to not set it.)
//It is undefined behaviour to set the callback twice.
//It is undefined behaviour to interact with any widget, except by putting it inside another widget, before it's placed inside a window.
//Any pointers given during widget creation must be valid until the widget is placed inside a window.
struct widget_base {
#ifdef NEED_MANUAL_LAYOUT
	//measure() returns no value, but sets the width and height. The sizes are undefined if the last
	// function call on the widget was not measure(); widgets may choose to update their sizes in
	// response to anything that resizes their size requests.
	//If multiple widgets want the space equally much, they get equal fractions, in addition to their base demand.
	//If a widget gets extra space and doesn't want it, it shouldadd some padding in any direction.
	//The widget should, if needed by the window manager, forward all plausible events to its parent window,
	// unless the widget wants the events. (For example, a button will want mouse events, but not file drop events.)
	//The window handles passed around are implementation defined.
	//The return value is the number of windows involved, from the window manager's point of view.
	unsigned int (*_init)(struct widget_base * this, struct window * parent, uintptr_t parenthandle);
	void (*_measure)(struct widget_base * this);
	unsigned int _width;
	unsigned int _height;
	void (*_place)(struct widget_base * this, void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height);
#else
	void * _widget;
#endif
	//The priorities mean:
	//0 - Widget has been assigned a certain size; it must get exactly that. (Canvas, viewport)
	//1 - Widget wants a specific size; will only grudgingly accept more. (Most of them)
	//2 - Widget has orders to consume extra space if there's any left over and nothing really wants it. (Padding)
	//3 - Will work better if given extra space. (Listbox)
	//4 - Widget is ordered to be resizable. (Canvas, viewport)
	unsigned char _widthprio;
	unsigned char _heightprio;
	void (*_free)(struct widget_base * this);
};


struct widget_padding {
	struct widget_base base;
	//can't disable this
};
struct widget_padding * widget_create_padding_horz();
struct widget_padding * widget_create_padding_vert();


struct widget_label {
	struct widget_base base;
	//Disabling a label does nothing, but may change how it looks. Use it if it's attached to another widget, and this widget is disabled.
	void (*set_enabled)(struct widget_label * this, bool enable);
	
	void (*set_text)(struct widget_label * this, const char * text);
};
struct widget_label * widget_create_label(const char * text);


struct widget_button {
	struct widget_base base;
	void (*set_enabled)(struct widget_button * this, bool enable);
	
	void (*set_text)(struct widget_button * this, const char * text);
	void (*set_onclick)(struct widget_button * this, void (*onclick)(struct widget_button * subject, void* userdata), void* userdata);
};
struct widget_button * widget_create_button(const char * text);


struct widget_checkbox {
	struct widget_base base;
	void (*set_enabled)(struct widget_checkbox * this, bool enable);
	
	void (*set_text)(struct widget_checkbox * this, const char * text);
	bool (*get_state)(struct widget_checkbox * this);
	void (*set_state)(struct widget_checkbox * this, bool checked);
	void (*set_onclick)(struct widget_checkbox * this,
	                    void (*onclick)(struct widget_checkbox * subject, bool checked, void* userdata), void* userdata);
};
struct widget_checkbox * widget_create_checkbox(const char * text);


struct widget_radio {
	struct widget_base base;
	void (*set_enabled)(struct widget_radio * this, bool enable);
	
	void (*set_text)(struct widget_radio * this, const char * text);
	
	//Radio buttons must be grouped before you're allowed to use them.
	//The one this function is called on becomes the group leader. The leader must be the first in the group.
	//It is undefined behaviour to attempt to redefine a group.
	//It is undefined behaviour to set the onclick handler, or set or get the state, for anything except the group leader.
	//It is undefined behaviour to do anything with a radio button before grouping them, except put them in a window.
	//However, the window may not be shown before grouping them.
	void (*group)(struct widget_radio * this, unsigned int numitems, struct widget_radio * * group);
	
	//Returns which one is active. The group leader is 0.
	unsigned int (*get_state)(struct widget_radio * this);
	
	//State values are the same as get_state().
	void (*set_state)(struct widget_radio * this, unsigned int state);
	
	//Called whenever the state changes. It is allowed to set the state in response to this.
	//For a grouped radio box, the callback contains the group leader.
	//It is undefined whether clicking on the same radio button twice makes the callback fire twice.
	void (*set_onclick)(struct widget_radio * this,
	                    void (*onclick)(struct widget_radio * subject, unsigned int state, void* userdata),
	                    void* userdata);
};
struct widget_radio * widget_create_radio(const char * text);
//This one automates radio button grouping.
//It wraps them in a horizontal or vertical layout, and returns the group leader in the out parameter.
//It's just a convenience; you can create them and group them manually and get the same results.
struct widget_layout * widget_create_radio_group(struct widget_radio * * leader, bool vertical, const char * firsttext, ...);


struct widget_textbox {
	struct widget_base base;
	void (*set_enabled)(struct widget_textbox * this, bool enable);
	
	//The return value is guaranteed valid until the next call to a function on this object, or window_run[_iter], whichever comes first.
	const char * (*get_text)(struct widget_textbox * this);
	//If the length is 0, it's unlimited.
	void (*set_text)(struct widget_textbox * this, const char * text, unsigned int maxlen);
	
	//Called whenever the text changes.
	//Note that it is not guaranteed to fire only if the text has changed; it may, for example,
	// fire if the user selects an E and types another E on top. Or for no reason at all.
	//Also note that 'text' is invalidated under the same conditions as get_text is.
	void (*set_onchange)(struct widget_textbox * this, void (*callback)(struct widget_textbox * subject, const char * text, void * userdata), void * userdata);
	//Called if the user hits Enter while this widget is focused. [TODO: Doesn't that activate the default button instead?]
	void (*set_onactivate)(struct widget_textbox * this, void (*callback)(struct widget_textbox * subject, void * userdata), void * userdata);
};
struct widget_textbox * widget_create_textbox();


//A canvas is a simple image. It's easy to work with, but performance is poor and it can't vsync, so it shouldn't be used for video.
struct widget_canvas {
	struct widget_base base;
	//can't disable this
	
	void (*resize)(struct widget_canvas * this, unsigned int width, unsigned int height);
	unsigned int * (*draw_begin)(struct widget_canvas * this);
	void (*draw_end)(struct widget_canvas * this);
	
	//Whether to hide the cursor while it's on top of this widget.
	//The mouse won't instantly hide; if it's moving, it will be visible. The exact details are up to the implementation,
	// but it will be similar to "the mouse is visible if it has moved within the last 1000 milliseconds".
	void (*set_hide_cursor)(struct widget_canvas * this, bool hide);
	
	//This must be called before the window is shown, and only exactly once.
	//All given filenames are invalidated once the callback returns.
	void (*set_support_drop)(struct widget_canvas * this,
	                         void (*on_file_drop)(struct widget_canvas * subject, const char * const * filenames, void* userdata),
	                         void* userdata);
};
struct widget_canvas * widget_create_canvas(unsigned int width, unsigned int height);


//A viewport fills the same purpose as a canvas, but the tradeoffs go the opposite way.
struct widget_viewport {
	struct widget_base base;
	//can't disable this
	
	void (*resize)(struct widget_viewport * this, unsigned int width, unsigned int height);
	uintptr_t (*get_window_handle)(struct widget_viewport * this);
	
	//See documentation of canvas for these.
	void (*set_hide_cursor)(struct widget_viewport * this, bool hide);
	void (*set_support_drop)(struct widget_viewport * this,
	                         void (*on_file_drop)(struct widget_viewport * subject, const char * const * filenames, void* userdata),
	                         void* userdata);
	
	//Keycodes are from libretro; 0 if unknown. Scancodes are implementation defined, but if there is no libretro translation, then none is returned.
	//void (*set_kb_callback)(struct widget_viewport * this,
	//                        void (*keyboard_cb)(struct window * subject, unsigned int keycode, unsigned int scancode, void* userdata), void* userdata);
};
struct widget_viewport * widget_create_viewport(unsigned int width, unsigned int height);


struct widget_listbox {
	struct widget_base base;
	void (*set_enabled)(struct widget_listbox * this, bool enable);
	
	//It is undefined behaviour to not set the contents before showing the window.
	//If the length changes, call this again.
	void (*set_contents)(struct widget_listbox * this, unsigned int rows, const char * * contents);
	//Calling this makes the listbox 'virtual'. A virtual listbox calls the callback every time an item is to be displayed.
	//It is not guaranteed when, and in which order, the items are requested. The function must, at
	// all times, be ready to be asked for any value, including while inside a function call on this
	// widget, including set_contents_virtual().
	//It is, however, allowed to make a cache based on the assumption that the request order is
	// row 1 column 1, row 1 column 2, row 2 column 1, etc.
	//It is allowed to implement one of set_contents[_virtual] in terms of the other (though
	// implementing set_contents_virtual as set_contents wastes a lot of memory and is therefore
	// unwise).
	//Because this one may be called very often, it is very strongly recommended to not do anything
	// except access memory during this callback; for example, do not query radio button states.
	//Additionally, you should estimate the maximal runtime.
	//The return value can be freed the next time the anything is requested from the same column
	// (whether the same row or another one), or once whatever the callback is called from returns,
	// whichever comes first.
	//It is allowed to call set_contents[_virtual] multiple times, including calling both of them
	// multiple times each.
	//
	//The search callback allows quicker searching for items in the list than generating all items. It
	// will search through all rows for something where the first column starts with the given string
	// (case insensitive), making use of what it knows about the nature of its data. For example, if
	// only every tenth row starts with another letter than the previous row, it won't need to examine
	// the remaining nine; or if the data is sorted, it can use a binary search.
	//If there is no match after the given row, it will restart at row zero. If there is no match before that either, it will return -1.
	//It's optional to implement; if NULL is given, the implementation will provide one based on get_cell. The implementation is
	// not obliged to use this callback, either.
	//The same userdata will be given to both callbacks.
	//
	//Note that this widget is rather slow on GTK+ on large lists.
	void (*set_contents_virtual)(struct widget_listbox * this, unsigned int rows,
	                             const char * (*get_cell)(struct widget_listbox * subject, unsigned int row, unsigned int column,
	                                                      void * userdata),
	                             int (*search)(struct widget_listbox * subject, unsigned int start, const char * str, void * userdata),
	                             void * userdata);
	//This changes only one row. It can be used even if the listbox is virtual, though in this case, the row contents must be NULL.
	void (*set_row)(struct widget_listbox * this, unsigned int row, const char * * contents);
	//Tells the widget that the contents have changed and that it should redraw all rows.
	//If the widget is not virtual, it does nothing except waste time.
	void (*refresh)(struct widget_listbox * this);
	
	//The active row can change without activating the new item.
	//The exact conditions under which a listbox entry is activated is platform dependent, but
	// double click and Enter are common. It is guaranteed to be possible.
	unsigned int (*get_active_row)(struct widget_listbox * this);
	void (*set_onactivate)(struct widget_listbox * this,
	                       void (*onactivate)(struct widget_listbox * subject, unsigned int row, void * userdata),
	                       void* userdata);
	
	//This is the size on the screen. The height is how many items show up; the widths are how many
	// instances of the letter 'x' must fit in the column.
	//It is allowed to use 0 (height) or NULL (widths) to mean "keep current or use the default".
	void (*set_size)(struct widget_listbox * this, unsigned int height, const unsigned int * widths);
	
	//You may only add checkboxes once.
	//Calling set_contents[_virtual] keeps the checkboxes, but resets them all. You can use set_all_checkboxes to put the values back.
	//You may not use the other three checkbox functions without adding checkboxes first.
	//It is implementation defined how the checkboxes are represented. They can be prepended to the
	// first column, on a column of their own, or something weirder. The position relative to the other columns is not guaranteed.
	void (*add_checkboxes)(struct widget_listbox * this,
	                       void (*ontoggle)(struct widget_listbox * subject, unsigned int row, bool state, void * userdata),
	                       void * userdata);
	bool (*get_checkbox_state)(struct widget_listbox * this, unsigned int row);
	void (*set_checkbox_state)(struct widget_listbox * this, unsigned int row, bool state);
	void (*set_all_checkboxes)(struct widget_listbox * this, bool * states);
};
struct widget_listbox * widget_create_listbox_l(unsigned int numcolumns, const char * * columns);
struct widget_listbox * widget_create_listbox(const char * firstcol, ...);


//A decorative frame around a widget, to group them together. The widget can be a layout (and probably should, otherwise you're adding a box to a single widget).
struct widget_frame {
	struct widget_base base;
	//can't disable this (well okay, we can, but it's pointless to disable a widget that does nothing)
	
	void (*set_text)(struct widget_frame * this, const char * text);
};
struct widget_frame * widget_create_frame(const char * text, void* contents);


//A horizontal layout puts its child widgets beside each other; a vertical layout puts its children on top of each other.
//It is safe to put layouts inside each other, though the types should alternate.
//It is not necessary for the root widget to be a layout.
struct widget_layout {
	struct widget_base base;
	//can't disable this widget - disable its contents instead
};
//The lists are terminated with a NULL. It shouldn't be empty.
#define widget_create_layout_horz(...) widget_create_layout(false, false, __VA_ARGS__)
#define widget_create_layout_vert(...) widget_create_layout(true, false, __VA_ARGS__)
struct widget_layout * widget_create_layout(bool vertical, bool uniform, void * firstchild, ...);
//The widgets are stored row by row.
struct widget_layout * widget_create_layout_grid(unsigned int width, unsigned int height,
                                                 void * firstchild, ...);
//numchildren can be 0. In this case, the array is assumed terminated with a NULL.
struct widget_layout * widget_create_layout_l(bool vertical, bool uniform, unsigned int numchildren, void * * children);
struct widget_layout * widget_create_layout_grid_l(unsigned int width, unsigned int height,
                                                   unsigned int numchildren, void * * children);



//Tells the window manager to handle recent events and fire whatever callbacks are relevant.
//Neither of them are allowed while inside any callback of any kind.
void window_run_iter();//Returns as soon as possible. Use while playing.
void window_run_wait();//Returns only after doing something. Use while idling. It will return if any
                       // state (other than the time) has changed or if any callback has fired.
                       // It may also return due to uninteresting events, as often as it wants;
                       // however, repeatedly calling it will leave the CPU mostly idle.

//Usable for both ROMs and dylibs. If dylib is true, the returned filenames are for the system's
// dynamic linker; this will disable gvfs-like systems the dynamic linker can't understand, and may
// hide files not marked executable, if this makes sense. If false, only file_read/etc is guaranteed
// to work.
//If multiple is true, multiple files may be picked; if not, only one can be picked. Should
// generally be true for dylibs and false for ROMs, but not guaranteed.
//The parent window will be disabled while the dialog is active.
//Both extensions and return value have the format { "smc", ".sfc", NULL }. Extensions may or may not
// include the dot; if it's not there, it's implied.
//Return value is full paths, zero or more. Duplicates are allowed in both input and output.
//The return value is valid until the next call to window_file_picker() or window_run_*(), whichever comes first.
const char * const * window_file_picker(struct window * parent,
                                        const char * title,
                                        const char * const * extensions,
                                        const char * extdescription,
                                        bool dylib,
                                        bool multiple);

//Returns the process path, without the filename. Multiple calls will return the same pointer.
const char * window_get_proc_path();
//Converts a relative path (../roms/mario.smc) to an absolute path (/home/admin/roms/mario.smc).
// Implemented by the window manager, so gvfs can be supported. If the file doesn't exist, it is
// implementation defined whether the return value is a nonexistent path, or if it's NULL.
//Send it to free() once it's done.
char * window_get_absolute_path(const char * path);
//Converts any file path to something accessible on the local file system. The resulting path can
// be both ugly and temporary, so only use it for file I/O, and store the absolute path instead.
//It is not guaranteed that window_get_absolute_path can return the original path, or anything useful at all, if given the output of this.
//It can return NULL, even for paths which file_read understands. If it doesn't, use free() when you're done.
char * window_get_native_path(const char * path);

//Returns the number of microseconds since an undefined start time.
//The start point doesn't change while the program is running, but need not be the same across reboots, nor between two processes.
//It can be program launch, system boot, the Unix epoch, or whatever.
uint64_t window_get_time();

#ifdef WNDPROT_X11
//Returns the display and screen we should use.
//The concept of screens only exists on X11, so this should not be used elsewhere.
//Only I/O drivers should have any reason to use this.
struct window_x11_display {
	void* display; //The real type is Display*.
	unsigned long screen; //The real type is Window aka XID.
};
const struct window_x11_display * window_x11_get_display();
#endif

//These are implemented by the window manager, despite looking somewhat unrelated.
//Can be just fopen, but may additionally support something implementation-defined, like gvfs;
// however, filename support is guaranteed, both relative and absolute.
//Directory separator is '/', extension separator is '.'.
//file_read appends a '\0' to the output; this is not reported in the length.
//Use free() on the return value from file_read().
bool file_read(const char * filename, char* * data, size_t * len);
bool file_write(const char * filename, const char * data, size_t len);
bool file_read_to(const char * filename, char * data, size_t len);//If size differs, this one fails.

//These will list the contents of a directory. The returned paths from window_find_next should be
// sent to free(). The . and .. components will not be included; however, symlinks and other loops
// are not guarded against. It is implementation defined whether hidden files are included. The
// returned filenames are relative to the original path and contain no path information nor leading
// or trailing slashes.
void* file_find_create(const char * path);
bool file_find_next(void* find, char* * path, bool * isdir);
void file_find_close(void* find);

//The different components may want to initialize various parts each. It is very likely that only two of them exist.
void _window_init_inner();
void _window_init_misc();
void _window_init_shell();
//If interaction with a widget is sent to the outer window, this sends it back to the inner area.
uintptr_t _window_notify_inner(void* notification);
//If the window manager does not implement any non-native paths (like gvfs), it can use this one;
// it's implemented by something that knows the local file system, but not the window manager.
//There is no _window_native_get_native_path; since the local file system doesn't understand whatever the window
// manager is doing, such a thing would be equivalent to window_native_get_absolute_path and therefore useless.
char * _window_native_get_absolute_path(const char * path);

//Devirtualizing a listbox means implementing widget_listbox->set_contents in terms of set_contents_virtual.
//To use, make set_contents call _widget_listbox_devirt_create and store the pointer.
//Free this pointer on set_contents_virtual and free. Initialize it to NULL.
struct _widget_listbox_devirt;
struct _widget_listbox_devirt * _widget_listbox_devirt_create(struct widget_listbox * outer,
                                                              unsigned int rows, unsigned int columns,
                                                              const char * * contents);
void _widget_listbox_devirt_set_row(struct _widget_listbox_devirt * this, unsigned int row, const char * * contents);
void _widget_listbox_devirt_free(struct _widget_listbox_devirt * this);
//This one can be used if the one calling widget_listbox->set_contents_virtual doesn't provide a search function.
int _widget_listbox_search(struct widget_listbox * subject, unsigned int rows,
                           const char * (*get_cell)(struct widget_listbox * subject,
                                                    unsigned int row, unsigned int column,
                                                    void * userdata),
                           unsigned int start, const char * str, void * userdata);
