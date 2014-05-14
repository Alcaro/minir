#include "minir.h"
#ifdef WINDOW_WIN32
#define _WIN32_WINNT 0x0501
#define _WIN32_IE 0x0600
#include <windows.h>
#include <commctrl.h>
#include <ctype.h>
#include<stdio.h>

//TODO:
//menu_create: check where it's resized if size changes

#define WS_BASE WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX//okay microsoft, did I miss anything?
#define WS_RESIZABLE (WS_BASE|WS_MAXIMIZEBOX|WS_THICKFRAME)
#define WS_NONRESIZ (WS_BASE|WS_BORDER)

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void _reflow(struct window * this_);

static bool isxp;

void _window_init_shell()
{
	WNDCLASS wc;
	wc.style=0;
	wc.lpfnWndProc=WindowProc;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=GetModuleHandle(NULL);
	wc.hIcon=LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(0));
	wc.hCursor=LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground=(HBRUSH)(COLOR_3DFACE + 1);
	wc.lpszMenuName=NULL;
	wc.lpszClassName="minir";
	RegisterClass(&wc);
	
	DWORD version=GetVersion();
	DWORD trueversion=(LOBYTE(LOWORD(version))<<8 | HIBYTE(LOWORD(version)));
	isxp=(trueversion<=0x0501);
}



enum { menu_item, menu_check, menu_radio, menu_separator, menu_submenu, menu_topmenu };
struct windowmenu_win32 {
	struct windowmenu i;
	
	uint8_t type;
	
	uint8_t thispos;//position in the menu, native position (radios count as multiple)
	//To get its ID, harass 'parent'. (topmenu has no parent, but it doesn't have an ID either.)
	//To get from an ID to these structures, use 'menumap'.
	
	UNION_BEGIN
		uint8_t numchildren;//used only for topmenu and childmenu; the odd location is to reduce padding
		uint8_t state;
	UNION_END
	
	//char padding[1];
	
	UNION_BEGIN
		HMENU parent;//the menu it is in; NULL for the topmenu
		const char * text;//used before this menu is given a parent
		const char * * texts;//used instead of the above for radio items (length is below)
	UNION_END
	
	UNION_BEGIN
		HMENU childmenu;//for topmenu and childmenu, this one represents each submenu item
		unsigned int radio_length;//this can't go above one byte, but it's unioned with a pointer, so the space would've been used anyways.
	UNION_END
	
	UNION_BEGIN
		struct windowmenu_win32 * * childlist;
		void (*onactivate_item)(struct windowmenu * subject, void* userdata);
		void (*onactivate_check)(struct windowmenu * subject, bool checked, void* userdata);
		void (*onactivate_radio)(struct windowmenu * subject, unsigned int state, void* userdata);
	UNION_END
	void* userdata;
};

//indexed by WM_COMMAND wParam
static struct windowmenu_win32 * * menumap=NULL;
static WORD * menumap_radiostate=NULL;
static DWORD menudatafirstfree=1;//the pointed to item is not necessarily free, but all before are guaranteed to be used; it's DWORD to terminate a loop
static DWORD menudatabuflen=1;//this is DWORD only to allow exactly 0x10000

struct window_win32 {
	struct window i;
	
	HWND hwnd;
	struct widget_base * contents;
	unsigned int numchildwin;
	
	DWORD lastmousepos;
	
	struct windowmenu_win32 * menu;
	
	HWND status;
	int * status_align;
	int * status_div;
	unsigned short status_count;
	unsigned char status_resizegrip_width;
	bool status_extra_spacing;
	
	bool resizable;
	bool isdialog;
	
	bool menuactive;//odd position to reduce padding
	uint8_t delayfree;//0=normal, 1=can't free now, 2=free at next opportunity
	
	bool (*onclose)(struct window * subject, void* userdata);
	void* oncloseuserdata;
};

static HWND activedialog=NULL;

static void getBorderSizes(struct window_win32 * this, unsigned int * width, unsigned int * height)
{
	RECT inner;
	RECT outer;
	GetClientRect(this->hwnd, &inner);
	GetWindowRect(this->hwnd, &outer);
	if (width) *width=(outer.right-outer.left)-(inner.right);
	if (height) *height=(outer.bottom-outer.top)-(inner.bottom);
	
	if (height && this->status)
	{
		RECT statsize;
		GetClientRect(this->status, &statsize);
		*height+=(statsize.bottom-statsize.top);
	}
}

static void _reflow(struct window * this_);

static void resize_stbar(struct window_win32 * this, unsigned int width)
{
	if (this->status)
	{
		SendMessage(this->status, WM_SIZE, 0,0);
		
		unsigned int statuswidth=width;
		if (this->resizable)
		{
			if (!this->status_resizegrip_width)
			{
				RECT rect;
				SendMessage(this->status, SB_GETRECT, 0, (LPARAM)&rect);
				this->status_resizegrip_width=rect.bottom-rect.top-8;//assume the size grip has the same width as height
			}
			statuswidth-=this->status_resizegrip_width;
		}
		int * statuspositions=malloc(sizeof(int)*this->status_count);
		for (int i=0;i<this->status_count;i++)
		{
			statuspositions[i]=statuswidth*(this->status_div[i])/240;
		}
		//statuspositions[this->status_count-1]=width;
		SendMessage(this->status, SB_SETPARTS, (WPARAM)this->status_count, (LPARAM)statuspositions);
		free(statuspositions);
	}
}

static void set_is_dialog(struct window * this_)
{
	struct window_win32 * this=(struct window_win32*)this_;
	this->isdialog=true;
}

static void set_parent(struct window * this_, struct window * parent_)
{
	struct window_win32 * this=(struct window_win32*)this_;
	struct window_win32 * parent=(struct window_win32*)parent_;
	SetWindowLongPtr(this->hwnd, GWLP_HWNDPARENT, (LONG_PTR)parent->hwnd);
}

static void resize(struct window * this_, unsigned int width, unsigned int height)
{
	struct window_win32 * this=(struct window_win32*)this_;
	
	unsigned int padx;
	unsigned int pady;
	getBorderSizes(this, &padx, &pady);
	
	SetWindowPos(this->hwnd, NULL, 0, 0, width+padx, height+pady,
	             SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
	
	//because we're setting the window position ourselves, reflow is likely to think the status bar is correct, so we need to fix it ourselves.
	resize_stbar(this, width);
	
	_reflow((struct window*)this);
}

static void set_resizable(struct window * this_, bool resizable,
                          void (*onresize)(struct window * subject, unsigned int newwidth, unsigned int newheight, void* userdata), void* userdata)
{
	struct window_win32 * this=(struct window_win32*)this_;
	if (this->resizable != resizable)
	{
		this->resizable=resizable;
		SetWindowLong(this->hwnd, GWL_STYLE, GetWindowLong(this->hwnd, GWL_STYLE) ^ WS_RESIZABLE^WS_NONRESIZ);
		_reflow(this_);
	}
}

static void set_title(struct window * this_, const char * title)
{
	struct window_win32 * this=(struct window_win32*)this_;
	if (!isxp) SetWindowText(this->hwnd, title);
}

static void onclose(struct window * this_, bool (*function)(struct window * subject, void* userdata), void* userdata)
{
	struct window_win32 * this=(struct window_win32*)this_;
	this->onclose=function;
	this->oncloseuserdata=userdata;
}



static void menu_delete(struct windowmenu_win32 * this);

static WORD menu_alloc_id()
{
	while (menudatafirstfree<menudatabuflen)
	{
		if (!menumap[menudatafirstfree]) return menudatafirstfree;
		menudatafirstfree++;
	}
	if (menudatafirstfree>0xFFFF) abort();
	size_t buflenbytes=sizeof(struct windowmenu_win32*)*menudatabuflen;
	menudatabuflen*=2;
	menumap=realloc(menumap, buflenbytes*2);
	menumap_radiostate=realloc(menumap_radiostate, sizeof(WORD)*menudatabuflen);
	if (!menumap || !menumap_radiostate) abort();//high index possible - verify success
	memset((char*)menumap+buflenbytes, 0, buflenbytes);
	return menudatafirstfree;
}

static void menu_dealloc_id(WORD id)
{
	menumap[id]=NULL;
	if (id < menudatafirstfree) menudatafirstfree=id;
}

static char * menu_transform_name(const char * name)
{
	bool useaccel=(*name=='_');
	if (useaccel) name++;
	unsigned int accelpos=0;
	unsigned int len=0;
	for (int i=0;name[i];i++)
	{
		if (name[i]=='&') len++;
		if (!accelpos && name[i]=='_') accelpos=i;
		len++;
	}
	char * ret=malloc(len+2);//NUL, extra &
	char * at=ret;
	for (int i=0;name[i];i++)
	{
		if (name[i]=='&') *(at++)='&';
		if (useaccel && i==accelpos) *(at++)='&';
		else *(at++)=name[i];
	}
	*at='\0';
	return ret;
}

static unsigned int menu_get_native_length(struct windowmenu_win32 * this)
{
	return (this->type==menu_radio ? this->radio_length : 1);
}

static unsigned int menu_get_native_start(struct windowmenu_win32 * this, unsigned int pos)
{
	return (pos ? this->childlist[pos-1]->thispos+menu_get_native_length(this->childlist[pos-1]) : 0);
}

static void menu_add_item(struct windowmenu_win32 * this, unsigned int pos, struct windowmenu_win32 * child)
{
	HMENU parent=this->childmenu;
	unsigned int menupos=menu_get_native_start(this, pos);
	if (child->type==menu_radio)
	{
		child->thispos=menupos;
		for (unsigned int i=0;i<child->radio_length;i++)
		{
			WORD id=menu_alloc_id();
			char * name=menu_transform_name(child->texts[i]);
			InsertMenu(parent, menupos, MF_BYPOSITION|MF_STRING, id, name);
			free(name);
			menumap[id]=child;
			menumap_radiostate[id]=i;
			menupos++;
		}
		free(child->texts);
		child->parent=parent;
	}
	else if (child->type==menu_separator)
	{
		child->thispos=menupos;
		InsertMenu(parent, menupos, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
		child->parent=parent;
	}
	else if (child->type==menu_submenu)
	{
		child->thispos=menupos;
		char * name=menu_transform_name(child->text);
		InsertMenu(parent, menupos, MF_BYPOSITION|MF_POPUP, (UINT_PTR)child->childmenu, name);
		free(name);
		child->parent=parent;
	}
	else
	{
		child->thispos=menupos;
		WORD id=menu_alloc_id();
		menumap[id]=child;
		char * name=menu_transform_name(child->text);
		InsertMenu(parent, menupos, MF_BYPOSITION|MF_STRING, id, name);
		free(name);
		child->parent=parent;
	}
}

static void menu_delete(struct windowmenu_win32 * this)
{
	if (this->type==menu_radio)
	{
		for (unsigned int i=0;i<this->radio_length;i++)
		{
			menu_dealloc_id(GetMenuItemID(this->parent, this->thispos));
		}
	}
	else if (this->type==menu_submenu || this->type==menu_topmenu)
	{
		for (unsigned int i=0;i<this->numchildren;i++)
		{
			menu_delete(this->childlist[i]);
		}
		free(this->childlist);
	}
	else if (this->type==menu_separator) {}
	else
	{
		menu_dealloc_id(GetMenuItemID(this->parent, this->thispos));
	}
	free(this);
}

static void menu_set_enabled(struct windowmenu * this_, bool enabled)
{
	struct windowmenu_win32 * this=(struct windowmenu_win32*)this_;
	unsigned int pos=this->thispos;
	unsigned int left=menu_get_native_length(this);
	while (left--)
	{
		MENUITEMINFO info;
		info.cbSize=sizeof(MENUITEMINFO);
		info.fMask=MIIM_STATE;
		GetMenuItemInfo(this->parent, pos, TRUE, &info);
		if (enabled) info.fState&=~MFS_GRAYED;
		else info.fState|=MFS_GRAYED;
		SetMenuItemInfo(this->parent, pos, TRUE, &info);
		pos++;
	}
}

struct windowmenu * windowmenu_create_item(const char * text,
                                           void (*onactivate)(struct windowmenu * subject, void* userdata),
                                           void* userdata)
{
	struct windowmenu_win32 * this=malloc(sizeof(struct windowmenu_win32));
	this->type=menu_item;
	this->text=text;
	this->onactivate_item=onactivate;
	this->userdata=userdata;
	this->i.set_enabled=menu_set_enabled;
	return (struct windowmenu*)this;
}

static unsigned int menu_get_state(struct windowmenu * this_)
{
	struct windowmenu_win32 * this=(struct windowmenu_win32*)this_;
	return this->state;
}

static void menu_set_state(struct windowmenu * this_, unsigned int state)
{
	struct windowmenu_win32 * this=(struct windowmenu_win32*)this_;
	this->state=state;
	if (this->type==menu_check) CheckMenuItem(this->parent, this->thispos, MF_BYPOSITION | (state?MF_CHECKED:MF_UNCHECKED));
	if (this->type==menu_radio) CheckMenuRadioItem(this->parent, this->thispos, this->thispos+this->radio_length-1, this->thispos+state, MF_BYPOSITION);
}

struct windowmenu * windowmenu_create_check(const char * text,
                                            void (*onactivate)(struct windowmenu * subject, bool checked, void* userdata),
                                            void* userdata)
{
	struct windowmenu_win32 * this=malloc(sizeof(struct windowmenu_win32));
	this->type=menu_check;
	this->text=text;
	this->state=0;
	this->onactivate_check=onactivate;
	this->userdata=userdata;
	this->i.set_enabled=menu_set_enabled;
	this->i.get_state=menu_get_state;
	this->i.set_state=menu_set_state;
	return (struct windowmenu*)this;
}

struct windowmenu * windowmenu_create_radio_l(unsigned int numitems, const char * const * labels,
                                              void (*onactivate)(struct windowmenu * subject, unsigned int state, void* userdata),
                                              void* userdata)
{
	struct windowmenu_win32 * this=malloc(sizeof(struct windowmenu_win32));
	this->type=menu_radio;
	this->texts=malloc(sizeof(const char*)*numitems);
	memcpy(this->texts, labels, sizeof(const char*)*numitems);
	this->radio_length=numitems;
	this->state=0;
	this->onactivate_radio=onactivate;
	this->userdata=userdata;
	this->i.set_enabled=menu_set_enabled;
	this->i.get_state=menu_get_state;
	this->i.set_state=menu_set_state;
	return (struct windowmenu*)this;
}

struct windowmenu * windowmenu_create_separator()
{
	struct windowmenu_win32 * this=malloc(sizeof(struct windowmenu_win32));
	this->type=menu_separator;
	return (struct windowmenu*)this;
}

static void menu_insert_child(struct windowmenu * this_, unsigned int pos, struct windowmenu * child_)
{
	struct windowmenu_win32 * this=(struct windowmenu_win32*)this_;
	struct windowmenu_win32 * child=(struct windowmenu_win32*)child_;
	this->numchildren++;
	this->childlist=realloc(this->childlist, sizeof(struct windowmenu_win32*)*this->numchildren);
	if (!this->childlist) abort();
	memmove(this->childlist+pos+1, this->childlist+pos, sizeof(struct windowmenu_win32*)*(this->numchildren-pos-1));
	this->childlist[pos]=(struct windowmenu_win32*)child;
	menu_add_item(this, pos, child);
	unsigned int menupos=menu_get_native_start(this, pos);
	for (unsigned int i=pos;i<this->numchildren;i++)
	{
		struct windowmenu_win32 * child=this->childlist[i];
		child->thispos=menupos;
		menupos+=menu_get_native_length(child);
	}
}

static void menu_remove_child(struct windowmenu * this_, struct windowmenu * child_)
{
	struct windowmenu_win32 * this=(struct windowmenu_win32*)this_;
	struct windowmenu_win32 * child=(struct windowmenu_win32*)child_;
	unsigned int menupos=child->thispos;
	unsigned int remcount=menu_get_native_length(child);
	menu_delete(child);
	while (remcount--) DeleteMenu(this->childmenu, menupos, MF_BYPOSITION);
	this->numchildren--;
	unsigned int i=0;
	while (this->childlist[i]!=child) i++;
	while (i<this->numchildren)
	{
		child=this->childlist[i+1];
		this->childlist[i]=child;
		child->thispos=menupos;
		menupos+=menu_get_native_length(child);
		i++;
	}
}

static void menu_connect(struct windowmenu_win32 * this, unsigned int numchildren, struct windowmenu_win32 * * children)
{
	this->childlist=malloc(sizeof(struct windowmenu_win32*)*numchildren);
	memcpy(this->childlist, children, sizeof(struct windowmenu_win32*)*numchildren);
	this->numchildren=numchildren;
	
	for (unsigned int i=0;i<numchildren;i++)
	{
		menu_add_item(this, i, children[i]);
	}
}

struct windowmenu * windowmenu_create_submenu_l(const char * text, unsigned int numchildren, struct windowmenu * * children)
{
	struct windowmenu_win32 * this=malloc(sizeof(struct windowmenu_win32));
	this->type=menu_submenu;
	this->childmenu=CreatePopupMenu();
	menu_connect(this, numchildren, (struct windowmenu_win32**)children);
	this->text=text;
	this->i.set_enabled=menu_set_enabled;
	this->i.insert_child=menu_insert_child;
	this->i.remove_child=menu_remove_child;
	return (struct windowmenu*)this;
}

struct windowmenu * windowmenu_create_topmenu_l(unsigned int numchildren, struct windowmenu * * children)
{
	struct windowmenu_win32 * this=malloc(sizeof(struct windowmenu_win32));
	this->type=menu_topmenu;
	this->childmenu=CreateMenu();
	menu_connect(this, numchildren, (struct windowmenu_win32**)children);
	this->i.set_enabled=menu_set_enabled;
	this->i.insert_child=menu_insert_child;
	this->i.remove_child=menu_remove_child;
	return (struct windowmenu*)this;
}

static void set_menu(struct window * this_, struct windowmenu * menu_)
{
	struct window_win32 * this=(struct window_win32*)this_;
	struct windowmenu_win32 * menu=(struct windowmenu_win32*)menu_;
	SetMenu(this->hwnd, menu->childmenu);
	if (this->menu) menu_delete(this->menu);
	this->menu=menu;
}

static void menu_activate(WORD id)
{
	struct windowmenu_win32 * item=menumap[id];
	if (item->type==menu_item)
	{
		if (item->onactivate_item) item->onactivate_item((struct windowmenu*)item, item->userdata);
		return;
	}
	if (item->type==menu_check)
	{
		item->state^=1;
		CheckMenuItem(item->parent, id, item->state?MF_CHECKED:MF_UNCHECKED);
		if (item->onactivate_check) item->onactivate_check((struct windowmenu*)item, item->state, item->userdata);
		return;
	}
	if (item->type==menu_radio)
	{
		item->state=menumap_radiostate[id];
		CheckMenuRadioItem(item->parent, item->thispos, item->thispos+item->radio_length-1, item->thispos+item->state, MF_BYPOSITION);
		if (item->onactivate_radio) item->onactivate_radio((struct windowmenu*)item, item->state, item->userdata);
		return;
	}
}



static void statusbar_create(struct window * this_, int numslots, const int * align, const int * dividerpos)
{
	struct window_win32 * this=(struct window_win32*)this_;
	
	if (!numslots)
	{
		if (this->status)
		{
			RECT barsize;
			GetWindowRect(this->status, &barsize);
			RECT mainsize;
			GetWindowRect(this->hwnd, &mainsize);
			SetWindowPos(this->hwnd, NULL, 0, 0, mainsize.right, mainsize.bottom-(barsize.bottom-barsize.top),
			             SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
			DestroyWindow(this->status);
			this->status=NULL;
		}
		return;
	}
	if (!this->status)
	{
		INITCOMMONCONTROLSEX initctrls;
		initctrls.dwSize=sizeof(initctrls);
		initctrls.dwICC=ICC_BAR_CLASSES;
		InitCommonControlsEx(&initctrls);
		
		this->status=CreateWindow(STATUSCLASSNAME, "", WS_CHILD|WS_VISIBLE|WS_DISABLED, 0,0,0,0, this->hwnd, NULL, GetModuleHandle(NULL), NULL);
		//SetWindowLong(this->status, GWL_STYLE, GetWindowLong(this->status, GWL_STYLE)|WS_DISABLED);//phoenix says this is needed, or it can get tab focus
		//EnableWindow(this->status, false);//phoenix says this is needed, or it can get tab focus
	}
	
	free(this->status_align);
	free(this->status_div);
	
	this->status_count=numslots;
	this->status_align=malloc(sizeof(int)*numslots);
	this->status_div=malloc(sizeof(int)*numslots);
	for (int i=0;i<numslots;i++)
	{
		this->status_align[i]=align[i];
		if (i<numslots-1) this->status_div[i]=dividerpos[i];
		else this->status_div[i]=240;
	}
	
	RECT barsize;
	GetWindowRect(this->status, &barsize);
	RECT mainsize;
	GetWindowRect(this->hwnd, &mainsize);
	SetWindowPos(this->hwnd, NULL, 0, 0, mainsize.right, mainsize.bottom+(barsize.bottom-barsize.top),
	             SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
	
	resize_stbar(this, barsize.right);
}

static void statusbar_set(struct window * this_, int slot, const char * text)
{
	struct window_win32 * this=(struct window_win32*)this_;
	int align=this->status_align[slot];
	if (align==0)
	{
		SendMessage(this->status, SB_SETTEXT, (WPARAM)slot, (LPARAM)text);
	}
	else
	{
		int textlen=strlen(text);
		char * newtext=malloc(1+1+textlen+1+1);
		newtext[0]='\t';
		newtext[1]='\t';
		memcpy(newtext+2, text, textlen);
		newtext[2+textlen]=((align==2)?' ':'\0');
		newtext[2+textlen+1]='\0';
		SendMessage(this->status, SB_SETTEXT, (WPARAM)slot, (LPARAM)(newtext + ((align==1)?1:0)));
		free(newtext);
	}
}

static void replace_contents(struct window * this_, void * contents)
{
	struct window_win32 * this=(struct window_win32*)this_;
	this->contents->_free(this->contents);
	this->contents=(struct widget_base*)contents;
	this->numchildwin=this->contents->_init(this->contents, (struct window*)this, (uintptr_t)this->hwnd);
	_reflow(this_);
}

static void set_visible(struct window * this_, bool visible)
{
	struct window_win32 * this=(struct window_win32*)this_;
	if (visible)
	{
		ShowWindow(this->hwnd, SW_SHOWNORMAL);
		_reflow(this_);
	}
	else
	{
		ShowWindow(this->hwnd, SW_HIDE);
	}
}

static bool is_visible(struct window * this_)
{
	struct window_win32 * this=(struct window_win32*)this_;
	return IsWindowVisible(this->hwnd);
}

static void focus(struct window * this_)
{
	struct window_win32 * this=(struct window_win32*)this_;
	SetForegroundWindow(this->hwnd);
}

static bool is_active(struct window * this_)
{
	struct window_win32 * this=(struct window_win32*)this_;
	return (GetForegroundWindow()==this->hwnd);
}

static bool menu_active(struct window * this_)
{
	struct window_win32 * this=(struct window_win32*)this_;
	return (this->menuactive);
}

static void free_(struct window * this_)
{
	struct window_win32 * this=(struct window_win32*)this_;
	if (this->delayfree)
	{
		this->delayfree=2;
		return;
	}
	this->contents->_free(this->contents);
	if (this->menu) menu_delete(this->menu);
	DestroyWindow(this->hwnd);
	free(this);
}

static void* _get_handle(struct window * this_)
{
	struct window_win32 * this=(struct window_win32*)this_;
	return this->hwnd;
}

static void _reflow(struct window * this_)
{
	struct window_win32 * this=(struct window_win32*)this_;
	
	if (!IsWindowVisible(this->hwnd)) return;
	
	//Resizing our window seems to call the resize callback again. We're not interested, it'll just recurse in irritating ways.
	static bool recursive=false;
	if (recursive) return;
	recursive=true;
	
	RECT size;
	GetClientRect(this->hwnd, &size);
	
	RECT statsize;
	if (this->status)
	{
		GetClientRect(this->status, &statsize);
		size.bottom-=statsize.bottom;
	}
	
	this->contents->_measure(this->contents);
	
	bool badx=(this->contents->_width  > size.right  || (!this->resizable && this->contents->_width  != size.right));
	bool bady=(this->contents->_height > size.bottom || (!this->resizable && this->contents->_height != size.bottom));
	
//printf("want=%u,%u have=%u,%u",this->contents->_width,this->contents->_height,size.right,size.bottom);
	if (badx) size.right=this->contents->_width;
	if (bady) size.bottom=this->contents->_height;
	
	if (badx || bady)
	{
		unsigned int outerw;
		unsigned int outerh;
		getBorderSizes(this, &outerw, &outerh);
		//we can't defer this, or GetClientRect will get stale data, and we need the actual window size to move the rest of the windows
		SetWindowPos(this->hwnd, NULL, 0, 0, size.right+outerw, size.bottom+outerh,
		             SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER);
		
		GetClientRect(this->hwnd, &size);
		if (this->status) size.bottom-=statsize.bottom;
//printf(" resz=%u,%u",size.right,size.bottom);
		resize_stbar(this, size.right);
	}
//puts("");
	
	HDWP hdwp=BeginDeferWindowPos(this->numchildwin);
	this->contents->_place(this->contents, &hdwp, 0,0, size.right, size.bottom);
	EndDeferWindowPos(hdwp);
	recursive=false;
}

const struct window_win32 window_win32_base = {{
	set_is_dialog, set_parent, resize, set_resizable, set_title, onclose,
	set_menu, statusbar_create, statusbar_set,
	replace_contents, set_visible, is_visible, focus, is_active, menu_active, free_, _get_handle, _reflow
}};

struct window * window_create(void * contents)
{
	struct window_win32 * this=malloc(sizeof(struct window_win32));
	memcpy(this, &window_win32_base, sizeof(struct window_win32));
	
	this->contents=contents;
	this->contents->_measure(this->contents);
	//the 6 and 28 are arbitrary; we'll set ourselves to a better size later. Windows' default placement algorithm sucks, anyways.
	this->hwnd=CreateWindow("minir", isxp?"Bug reports from XP users will be ignored. Get a new computer.":"", WS_NONRESIZ, CW_USEDEFAULT, CW_USEDEFAULT,
	                        this->contents->_width+6, this->contents->_height+28, NULL, NULL, GetModuleHandle(NULL), NULL);
	SetWindowLongPtr(this->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	this->numchildwin=this->contents->_init(this->contents, (struct window*)this, (uintptr_t)this->hwnd);
	
	this->status=NULL;
	this->menu=NULL;
	this->menuactive=false;
	this->resizable=false;
	this->onclose=NULL;
	this->lastmousepos=-1;
	this->status_resizegrip_width=0;
	this->delayfree=0;
	
	resize((struct window*)this, this->contents->_width, this->contents->_height);
	//_reflow((struct window*)this);
	
	return (struct window*)this;
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	struct window_win32 * this=(struct window_win32*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	switch (uMsg)
	{
	case WM_CTLCOLOREDIT: return _window_get_widget_color(uMsg, (HWND)lParam, (HDC)wParam, hwnd);
	case WM_GETMINMAXINFO:
		{
			if (this)
			{
				MINMAXINFO* mmi=(MINMAXINFO*)lParam;
				
				unsigned int padx;
				unsigned int pady;
				getBorderSizes(this, &padx, &pady);
				
				mmi->ptMinTrackSize.x=padx+this->contents->_width;
				mmi->ptMinTrackSize.y=pady+this->contents->_height;
			}
		}
		break;
	case WM_ACTIVATE:
		if (LOWORD(WM_ACTIVATE) && this->isdialog) activedialog=hwnd;
		else activedialog=NULL;
		break;
	case WM_CLOSE:
	case WM_ENDSESSION://this isn't really the most elegant solution, but it should work.
		{
			if (this->onclose)
			{
				this->delayfree=1;
				if (!this->onclose((struct window*)this, this->oncloseuserdata)) break;
				if (this->delayfree==2)
				{
					this->delayfree=0;
					free_((struct window*)this);
					break;
				}
				this->delayfree=0;
			}
			ShowWindow(hwnd, SW_HIDE);
		}
		break;
	case WM_COMMAND:
		{
			if (lParam==0) menu_activate(LOWORD(wParam));
			else
			{
				NMHDR nmhdr={(HWND)lParam, LOWORD(wParam), HIWORD(wParam)};
				return _window_notify_inner(&nmhdr);
			}
		}
		break;
	case WM_NOTIFY:
		{
			return _window_notify_inner((LPNMHDR)lParam);
		}
		break;
	case WM_DESTROY:
		break;
	case WM_SYSCOMMAND:
		{
//printf("SC=%.4X\n",wParam&0xFFFF);
			if ((wParam&0xFFF0)==SC_KEYMENU) break;//go away, we don't want automenues. Alt could be hit by accident.
			goto _default;
		}
	//check WM_CONTEXTMENU
	case WM_SIZE:
		{
			//not sure which of the windows get this
			if (!this) break;//this one seems to hit only on Wine, but whatever, worth checking.
			_reflow((struct window*)this);
			if (this->status) PostMessage(this->status, WM_SIZE, wParam, lParam);
		}
		break;
	case WM_ENTERMENULOOP:
		{
			this->menuactive=true;
		}
		break;
	case WM_EXITMENULOOP:
		{
			this->menuactive=false;
		}
		break;
	_default:
	default:
		return DefWindowProcA(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

static void handlemessage(MSG * msg)
{
	if (activedialog && IsDialogMessage(activedialog, msg)) return;
	TranslateMessage(msg);
	DispatchMessage(msg);
}

void window_run_iter()
{
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) handlemessage(&msg);
}

void window_run_wait()
{
	MSG msg;
	GetMessage(&msg, NULL, 0, 0);
	handlemessage(&msg);
	window_run_iter();
}
#endif
