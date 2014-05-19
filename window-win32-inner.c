#include "minir.h"
#ifdef WINDOW_WIN32
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#define _WIN32_IE 0x0600
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdlib.h>

//controls HIG and screenshots of them http://msdn.microsoft.com/en-us/library/aa511482.aspx
//controls docs http://msdn.microsoft.com/en-us/library/windows/desktop/bb773169%28v=vs.85%29.aspx
//alternatively http://msdn.microsoft.com/en-us/library/aa368039%28v=vs.85%29.aspx for some widgets

#define dpi_vert 96
#define dpi_horz 96

#define btn_width 75
#define btn_height 23
#define btn_padding 3

#define frame_top 16
#define frame_left 4
#define frame_right 4
#define frame_bottom 4

#define TIMER_MOUSEHIDE 1

//Keep noninteractive at 0 and defbutton at button+1.
//Otherwise, no rules for which values mean what; the IDs only mean anything inside this file.
#define CTID_NONINTERACTIVE 0
#define CTID_BUTTON 1
#define CTID_DEFBUTTON 2
//#define CTID_CHECK 3
#define CTID_RADIO 4
#define CTID_LISTVIEW 5
#define CTID_TEXTBOX 6

static LRESULT CALLBACK viewport_WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static HFONT dlgfont;
static unsigned int xwidth;

static HBRUSH bg_invalid;

//static bool recursive=false;

static HFONT try_create_font(const char * name, int size)
{
	return CreateFont(-MulDiv(size, dpi_vert, 72), 0, 0, 0, FW_NORMAL,
	                  FALSE, FALSE, FALSE, DEFAULT_CHARSET,
	                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE,
	                  name);
}

static void measure_text(const char * text, unsigned int * width, unsigned int * height)
{
	HDC hdc=GetDC(NULL);
	SelectObject(hdc, dlgfont);
	RECT rc={0, 0, 0, 0};
	DrawText(hdc, text, -1, &rc, DT_CALCRECT);
	ReleaseDC(NULL, hdc);
	if (width) *width=rc.right;
	if (height) *height=rc.bottom;
}

void _window_init_inner()
{
	//HDC hdc=GetDC(NULL);
	//dpiheight=GetDeviceCaps(hdc, LOGPIXELSY);
	
	dlgfont=try_create_font("Segoe UI", 9);
	if (!dlgfont) dlgfont=try_create_font("MS Shell Dlg 2", 8);//I'm just gonna expect this one not to fail.
	                                                           //If it does, we'll get the bad font; no real catastrophe.
	
//	HGDIOBJ prevfont=SelectObject(hdc, dlgfont);
//	TEXTMETRIC metrics;
//	if (GetTextMetrics(hdc, &metrics))
//	{
////for dialogs: GetDialogBaseUnits http://msdn.microsoft.com/en-us/library/ms645475%28VS.85%29.aspx
////button height is 14/8 times the font height
////button width is 50/4 times the average character width
////can be extracted from http://msdn.microsoft.com/en-us/library/dd144941%28v=vs.85%29.aspx tmHeight and tmAveCharWidth
//		dlgbuttonwidth=MulDiv(metrics.tmAveCharWidth, 50, 4);
//		dlgbuttonheight=MulDiv(metrics.tmHeight, 14, 8);
//	}
//	else
//	{
//		dlgbuttonwidth=80;
//		dlgbuttonheight=20;
//	}
	//"tmAveCharWidth is not precise". fuck that shit, I do the right thing if it's not a pain.
	//actually, the right thing would probably be "fuck Microsoft, more Linux" and delete this file,
	// because that's even less of a pain.
	
	//SelectObject(hdc, prevfont);
	//ReleaseDC(NULL, hdc);
	
	WNDCLASS wc;
	wc.style=0;
	wc.lpfnWndProc=viewport_WindowProc;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=GetModuleHandle(NULL);
	wc.hIcon=LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(0));
	wc.hCursor=LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground=GetSysColorBrush(COLOR_3DFACE);//(HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName=NULL;
	wc.lpszClassName="minir_viewport";
	RegisterClass(&wc);
	
	measure_text("xxxxxxxxxxxx", &xwidth, NULL);
	
	INITCOMMONCONTROLSEX initctrls;
	initctrls.dwSize=sizeof(initctrls);
	initctrls.dwICC=ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&initctrls);
	
	bg_invalid=CreateSolidBrush(RGB(0xFF,0x66,0x66));
}

static void place_window(HWND hwnd, void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	HDWP* hdwp=(HDWP*)resizeinf;
	*hdwp=DeferWindowPos(*hdwp, hwnd, NULL, x, y, width, height, SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOOWNERZORDER|SWP_NOZORDER);
}



struct widget_label_win32 {
	struct widget_label i;
	
	char* text;
	struct window * parent;
	HWND hwnd;
};

static unsigned int label__init(struct widget_base * this_, struct window * parent, uintptr_t parenthandle)
{
	struct widget_label_win32 * this=(struct widget_label_win32*)this_;
	this->parent=parent;
	char* text=(char*)this->hwnd;
	this->hwnd=CreateWindow(WC_STATIC, text, WS_CHILD|WS_VISIBLE|SS_NOPREFIX,
	                        0, 0, 16, 16, // just some random sizes, we'll resize it in _place()
	                        (HWND)parenthandle, NULL, GetModuleHandle(NULL), NULL);
	measure_text(text, &this->i.base._width, &this->i.base._height);
	free(text);
	SendMessage(this->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	return 1;
}

static void label__measure(struct widget_base * this_) {}

static void label__place(struct widget_base * this_, void* resizeinf,
                         unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	struct widget_label_win32 * this=(struct widget_label_win32*)this_;
	place_window(this->hwnd, resizeinf, x,y+(height-this->i.base._height)/2, width,this->i.base._height);
}

static void label__free(struct widget_base * this_)
{
	struct widget_label_win32 * this=(struct widget_label_win32*)this_;
	free(this);
}

static void label_set_enabled(struct widget_label * this_, bool enable)
{
	struct widget_label_win32 * this=(struct widget_label_win32*)this_;
	EnableWindow(this->hwnd, enable);
}

static void label_set_text(struct widget_label * this_, const char * text)
{
	struct widget_label_win32 * this=(struct widget_label_win32*)this_;
	SetWindowText(this->hwnd, text);
	measure_text(text, &this->i.base._width, &this->i.base._height);
	this->parent->_reflow(this->parent);
}

struct widget_label * widget_create_label(const char * text)
{
	struct widget_label_win32 * this=malloc(sizeof(struct widget_label_win32));
	this->i.base._init=label__init;
	this->i.base._measure=label__measure;
	this->i.base._widthprio=0;
	this->i.base._heightprio=0;
	this->i.base._place=label__place;
	this->i.base._free=label__free;
	
	this->i.set_enabled=label_set_enabled;
	this->i.set_text=label_set_text;
	
	this->hwnd=(HWND)strdup(text);
	
	return (struct widget_label*)this;
}



struct widget_button_win32 {
	struct widget_button i;
	
	//struct window * parent;
	HWND hwnd;
	
	void (*onclick)(struct widget_button * subject, void* userdata);
	void* userdata;
};

static unsigned int button__init(struct widget_base * this_, struct window * parent, uintptr_t parenthandle)
{
	struct widget_button_win32 * this=(struct widget_button_win32*)this_;
	//this->parent=parent;
	this->hwnd=CreateWindow(WC_BUTTON, (char*)this->hwnd, WS_CHILD|WS_VISIBLE|WS_TABSTOP, 0, 0, 16, 16,
	                        (HWND)parenthandle, (HMENU)CTID_BUTTON, GetModuleHandle(NULL), NULL);
	SetWindowLongPtr(this->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SendMessage(this->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	return 1;
}

static void button__measure(struct widget_base * this_) {}

static void button__place(struct widget_base * this_, void* resizeinf,
                          unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	struct widget_button_win32 * this=(struct widget_button_win32*)this_;
	place_window(this->hwnd, resizeinf, x, y, width, height);
}

static void button__free(struct widget_base * this_)
{
	struct widget_button_win32 * this=(struct widget_button_win32*)this_;
	free(this);
}

static void button_set_enabled(struct widget_button * this_, bool enable)
{
	struct widget_button_win32 * this=(struct widget_button_win32*)this_;
	EnableWindow(this->hwnd, enable);
}

static void button_set_text(struct widget_button * this_, const char * text)
{
	struct widget_button_win32 * this=(struct widget_button_win32*)this_;
	SetWindowText(this->hwnd, text);
}

static void button_set_onclick(struct widget_button * this_,
                               void (*onclick)(struct widget_button * subject, void* userdata),
                               void* userdata)
{
	struct widget_button_win32 * this=(struct widget_button_win32*)this_;
	this->onclick=onclick;
	this->userdata=userdata;
}

struct widget_button * widget_create_button(const char * text)
{
	struct widget_button_win32 * this=malloc(sizeof(struct widget_button_win32));
	this->i.base._init=button__init;
	this->i.base._measure=button__measure;
	this->i.base._widthprio=0;
	this->i.base._heightprio=0;
	this->i.base._place=button__place;
	this->i.base._free=button__free;
	
	this->i.set_enabled=button_set_enabled;
	this->i.set_text=button_set_text;
	this->i.set_onclick=button_set_onclick;
	
	this->i.base._width=btn_width;
	this->i.base._height=btn_height;
	
	this->onclick=NULL;
	
	this->hwnd=(HWND)text;
	
	return (struct widget_button*)this;
}



struct widget_checkbox_win32;



struct widget_radio_win32 {
	struct widget_radio i;
	
	struct window * parent;
	HWND hwnd;
	
	//this can point to itself
	struct widget_radio_win32 * leader;
	unsigned int id;
	unsigned int active;
	
	struct widget_radio_win32 * * group;
	
	void (*onclick)(struct widget_radio * subject, unsigned int state, void* userdata);
	void* userdata;
};

static unsigned int radio__init(struct widget_base * this_, struct window * parent, uintptr_t parenthandle)
{
	struct widget_radio_win32 * this=(struct widget_radio_win32*)this_;
	this->parent=parent;
	this->hwnd=CreateWindow(WC_BUTTON, this->userdata, WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_RADIOBUTTON, 0, 0, 16, 16,
	                        (HWND)parenthandle, (HMENU)CTID_RADIO, GetModuleHandle(NULL), NULL);
	SetWindowLongPtr(this->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SendMessage(this->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	if (this->leader && this->id==0) Button_SetCheck(this->hwnd, BST_CHECKED);
	return 1;
}

static void radio__measure(struct widget_base * this_) {}

static void radio__place(struct widget_base * this_, void* resizeinf,
                         unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	struct widget_radio_win32 * this=(struct widget_radio_win32*)this_;
	place_window(this->hwnd, resizeinf, x, y, width, height);
}

static void radio__free(struct widget_base * this_)
{
	struct widget_radio_win32 * this=(struct widget_radio_win32*)this_;
	free(this->group);
	free(this);
}

static void radio_set_enabled(struct widget_radio * this_, bool enable)
{
	struct widget_radio_win32 * this=(struct widget_radio_win32*)this_;
	EnableWindow(this->hwnd, enable);
}

static void radio_set_text(struct widget_radio * this_, const char * text)
{
	struct widget_radio_win32 * this=(struct widget_radio_win32*)this_;
	SetWindowText(this->hwnd, text);
	measure_text(text, &this->i.base._width, &this->i.base._height);
	this->i.base._width+=this->i.base._height;
	this->parent->_reflow(this->parent);
}

static void radio_set_state(struct widget_radio * this_, unsigned int state);
static void radio_group(struct widget_radio * this_, unsigned int numitems, struct widget_radio * * group_)
{
	struct widget_radio_win32 * this=(struct widget_radio_win32*)this_;
	struct widget_radio_win32 * * group=(struct widget_radio_win32**)group_;
	for (unsigned int i=0;i<numitems;i++)
	{
		group[i]->leader=this;
		group[i]->id=i;
	}
	this->active=0;
	this->group=malloc(sizeof(struct widget_radio_win32*)*numitems);
	memcpy(this->group, group, sizeof(struct widget_radio_win32*)*numitems);
	if (this->hwnd) Button_SetCheck(this->hwnd, BST_CHECKED);
}

static unsigned int radio_get_state(struct widget_radio * this_)
{
	struct widget_radio_win32 * this=(struct widget_radio_win32*)this_;
	return this->active;
}

static void radio_set_state(struct widget_radio * this_, unsigned int state)
{
	struct widget_radio_win32 * this=(struct widget_radio_win32*)this_;
	Button_SetCheck(this->leader->group[this->leader->active]->hwnd, BST_UNCHECKED);
	Button_SetCheck(this->leader->group[state]->hwnd, BST_CHECKED);
	this->leader->active=state;
}

static void radio_set_onclick(struct widget_radio * this_,
                    void (*onclick)(struct widget_radio * subject, unsigned int state, void* userdata), void* userdata)
{
	struct widget_radio_win32 * this=(struct widget_radio_win32*)this_;
	this->onclick=onclick;
	this->userdata=userdata;
}

struct widget_radio * widget_create_radio(const char * text)
{
	struct widget_radio_win32 * this=malloc(sizeof(struct widget_radio_win32));
	this->i.base._init=radio__init;
	this->i.base._measure=radio__measure;
	this->i.base._widthprio=0;
	this->i.base._heightprio=0;
	this->i.base._place=radio__place;
	this->i.base._free=radio__free;
	
	this->i.set_enabled=radio_set_enabled;
	this->i.set_text=radio_set_text;
	this->i.group=radio_group;
	this->i.get_state=radio_get_state;
	this->i.set_state=radio_set_state;
	this->i.set_onclick=radio_set_onclick;
	
	measure_text(text, &this->i.base._width, &this->i.base._height);
	this->i.base._width+=this->i.base._height;
	
	this->leader=NULL;
	this->hwnd=NULL;
	this->userdata=(void*)text;
	this->onclick=NULL;
	
	return (struct widget_radio*)this;
}



struct widget_textbox_win32 {
	struct widget_textbox i;
	
	HWND hwnd;
	
	char * text;
	
	bool invalid;
	
	void (*onchange)(struct widget_textbox * subject, const char * text, void* userdata);
	void* ch_userdata;
	void (*onactivate)(struct widget_textbox * subject, const char * text, void* userdata);
	void* ac_userdata;
};

static LRESULT CALLBACK textbox_SubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

static unsigned int textbox__init(struct widget_base * this_, struct window * parent, uintptr_t parenthandle)
{
	struct widget_textbox_win32 * this=(struct widget_textbox_win32*)this_;
	this->hwnd=CreateWindow(WC_EDIT, "", WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL, 0, 0, 16, 16,
	                        (HWND)parenthandle, (HMENU)CTID_TEXTBOX, GetModuleHandle(NULL), NULL);
	SetWindowLongPtr(this->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SendMessage(this->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	SetWindowSubclass(this->hwnd, textbox_SubclassProc, 0, 0);
	return 1;
}

static void textbox__measure(struct widget_base * this_)
{
	struct widget_textbox_win32 * this=(struct widget_textbox_win32*)this_;
	measure_text("xxxxx", &this->i.base._width, &this->i.base._height);
	this->i.base._width+=6;
	this->i.base._height+=6;
}

static void textbox__place(struct widget_base * this_, void* resizeinf,
                           unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	struct widget_textbox_win32 * this=(struct widget_textbox_win32*)this_;
	place_window(this->hwnd, resizeinf, x+1,y+1, width-2,height-2);//this math is to get the border to the inside of the widget
}

static void textbox__free(struct widget_base * this_)
{
	struct widget_textbox_win32 * this=(struct widget_textbox_win32*)this_;
	free(this->text);
	free(this);
}

static void textbox_set_enabled(struct widget_textbox * this_, bool enable)
{
	struct widget_textbox_win32 * this=(struct widget_textbox_win32*)this_;
	EnableWindow(this->hwnd, enable);
}

static void textbox_focus(struct widget_textbox * this_)
{
	struct widget_textbox_win32 * this=(struct widget_textbox_win32*)this_;
	SetFocus(this->hwnd);
}

static const char * textbox_get_text(struct widget_textbox * this_)
{
	struct widget_textbox_win32 * this=(struct widget_textbox_win32*)this_;
	free(this->text);
	unsigned int len=GetWindowTextLength(this->hwnd);
	this->text=malloc(len+1);
	GetWindowText(this->hwnd, this->text, len+1);
	return this->text;
}

static void textbox_set_text(struct widget_textbox * this_, const char * text, unsigned int maxlen)
{
	struct widget_textbox_win32 * this=(struct widget_textbox_win32*)this_;
	free(this->text);
	this->text=strdup(text);
	SetWindowText(this->hwnd, this->text);
	Edit_LimitText(this->hwnd, maxlen);//conveniently, we both chose 0 to mean unlimited
}

static void textbox_set_invalid(struct widget_textbox * this_, bool invalid)
{
	struct widget_textbox_win32 * this=(struct widget_textbox_win32*)this_;
	this->invalid=invalid;
	InvalidateRect(this->hwnd, NULL, FALSE);
	if (invalid) SetFocus(this->hwnd);
}

static void textbox_set_onchange(struct widget_textbox * this_,
                                 void (*onchange)(struct widget_textbox * subject, const char * text, void* userdata),
                                 void* userdata)
{
	struct widget_textbox_win32 * this=(struct widget_textbox_win32*)this_;
	this->onchange=onchange;
	this->ch_userdata=userdata;
}

static void textbox_set_onactivate(struct widget_textbox * this_,
                                   void (*onactivate)(struct widget_textbox * subject, const char * text, void* userdata),
                                   void* userdata)
{
	struct widget_textbox_win32 * this=(struct widget_textbox_win32*)this_;
	this->onactivate=onactivate;
	this->ac_userdata=userdata;
}

struct widget_textbox * widget_create_textbox()
{
	struct widget_textbox_win32 * this=malloc(sizeof(struct widget_textbox_win32));
	this->i.base._init=textbox__init;
	this->i.base._measure=textbox__measure;
	this->i.base._widthprio=3;
	this->i.base._heightprio=0;
	this->i.base._place=textbox__place;
	this->i.base._free=textbox__free;
	
	this->i.set_enabled=textbox_set_enabled;
	this->i.focus=textbox_focus;
	this->i.get_text=textbox_get_text;
	this->i.set_text=textbox_set_text;
	this->i.set_invalid=textbox_set_invalid;
	this->i.set_onchange=textbox_set_onchange;
	this->i.set_onactivate=textbox_set_onactivate;
	
	this->text=NULL;
	this->onchange=NULL;
	this->invalid=false;
	
	return (struct widget_textbox*)this;
}


static LRESULT CALLBACK textbox_SubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                             UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	struct widget_textbox_win32 * this=(struct widget_textbox_win32*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_KEYDOWN:
			if (wParam==VK_RETURN && !(lParam & 0x40000000) && this->onactivate)
			{
				this->onactivate((struct widget_textbox*)this, textbox_get_text((struct widget_textbox*)this), this->ac_userdata);
				return 0;
			}
			break;
		case WM_NCDESTROY:
			RemoveWindowSubclass(hwnd, textbox_SubclassProc, 0);
			break;
	}
	return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}



struct widget_canvas_win32;



struct widget_viewport_win32 {
	struct widget_viewport i;
	
	struct window * parent;
	HWND hwnd;
	
	bool hide_cursor_user;
	bool hide_cursor_timer;
	DWORD lastmousepos;
	
	void (*on_file_drop)(struct widget_viewport * subject, const char * const * filenames, void* userdata);
	void* dropuserdata;
};

static unsigned int viewport__init(struct widget_base * this_, struct window * parent, uintptr_t parenthandle)
{
	struct widget_viewport_win32 * this=(struct widget_viewport_win32*)this_;
	this->parent=parent;
	this->hwnd=CreateWindow("minir_viewport", "", WS_CHILD|WS_VISIBLE, 0, 0, 16, 16,
	                        (HWND)parenthandle, NULL, GetModuleHandle(NULL), NULL);
	SetWindowLongPtr(this->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	return 1;
}

static void viewport__measure(struct widget_base * this_) {}

static void viewport__place(struct widget_base * this_, void* resizeinf,
                            unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	struct widget_viewport_win32 * this=(struct widget_viewport_win32*)this_;
	place_window(this->hwnd, resizeinf, x, y, width, height);
}

static void viewport__free(struct widget_base * this_)
{
	struct widget_viewport_win32 * this=(struct widget_viewport_win32*)this_;
	free(this);
}

static void viewport_resize(struct widget_viewport * this_, unsigned int width, unsigned int height)
{
	struct widget_viewport_win32 * this=(struct widget_viewport_win32*)this_;
	this->i.base._width=width;
	this->i.base._height=height;
	this->parent->_reflow(this->parent);
}

static uintptr_t viewport_get_window_handle(struct widget_viewport * this_)
{
	struct widget_viewport_win32 * this=(struct widget_viewport_win32*)this_;
	return (uintptr_t)this->hwnd;
}

static void viewport_set_hide_cursor(struct widget_viewport * this_, bool hide)
{
	struct widget_viewport_win32 * this=(struct widget_viewport_win32*)this_;
	this->hide_cursor_user=hide;
	
	if (this->hide_cursor_user && this->hide_cursor_timer) SetCursor(NULL);
	else SetCursor(LoadCursor(NULL, IDC_ARROW));
}

static void viewport_set_support_drop(struct widget_viewport * this_,
                             void (*on_file_drop)(struct widget_viewport * subject, const char * const * filenames, void* userdata),
                             void* userdata)
{
	struct widget_viewport_win32 * this=(struct widget_viewport_win32*)this_;
	
	DragAcceptFiles(this->hwnd, TRUE);
	
	this->on_file_drop=on_file_drop;
	this->dropuserdata=userdata;
}

struct widget_viewport * widget_create_viewport(unsigned int width, unsigned int height)
{
	struct widget_viewport_win32 * this=malloc(sizeof(struct widget_viewport_win32));
	this->i.base._init=viewport__init;
	this->i.base._measure=viewport__measure;
	this->i.base._widthprio=0;
	this->i.base._heightprio=0;
	this->i.base._place=viewport__place;
	this->i.base._free=viewport__free;
	
	this->i.resize=viewport_resize;
	this->i.get_window_handle=viewport_get_window_handle;
	this->i.set_hide_cursor=viewport_set_hide_cursor;
	this->i.set_support_drop=viewport_set_support_drop;
	
	this->i.base._width=width;
	this->i.base._height=height;
	
	this->hide_cursor_user=false;
	this->hide_cursor_timer=true;
	this->lastmousepos=-1;//random value, just so Valgrind-like tools won't throw.
	this->on_file_drop=NULL;
	
	return (struct widget_viewport*)this;
}

static LRESULT CALLBACK viewport_WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	struct widget_viewport_win32 * this=(struct widget_viewport_win32*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	switch (uMsg)
	{
	case WM_MOUSEMOVE:
		{
			//windows, what exactly is the point behind the bogus mouse move messages
			//you clearly know they exist (http://blogs.msdn.com/b/oldnewthing/archive/2009/06/17/9763416.aspx), why not block them?
			if (lParam==this->lastmousepos) break;
			this->lastmousepos=lParam;
			
			SetTimer(this->hwnd, TIMER_MOUSEHIDE, 1000, NULL);
			TRACKMOUSEEVENT tme={ sizeof(tme), TME_LEAVE, this->hwnd, HOVER_DEFAULT };
			TrackMouseEvent(&tme);
			this->hide_cursor_timer=false;
		}
		break;
	case WM_MOUSELEAVE:
	case WM_NCMOUSEMOVE:
		{
			KillTimer(this->hwnd, TIMER_MOUSEHIDE);
			this->hide_cursor_timer=false;
		}
		break;
	case WM_TIMER:
		{
			if (wParam==TIMER_MOUSEHIDE)
			{
				this->hide_cursor_timer=true;
				if (this->hide_cursor_user) SetCursor(NULL);
				KillTimer(this->hwnd, TIMER_MOUSEHIDE);
			}
		}
		break;
	case WM_SETCURSOR:
		{
			if (this->hide_cursor_user && this->hide_cursor_timer) SetCursor(NULL);
			else goto _default;
		}
		break;
	case WM_DROPFILES:
		{
			HDROP hdrop=(HDROP)wParam;
			UINT numfiles=DragQueryFile(hdrop, 0xFFFFFFFF, NULL, 0);//but what if I drop four billion files?
			char * filenames[numfiles+1];
			for (UINT i=0;i<numfiles;i++)
			{
				UINT len=DragQueryFile(hdrop, i, NULL, 0);
				filenames[i]=malloc(len+1);
				DragQueryFile(hdrop, i, filenames[i], len+1);
				for (unsigned int j=0;filenames[i][j];j++)
				{
					if (filenames[i][j]=='\\') filenames[i][j]='/';
				}
			}
			filenames[numfiles]=NULL;
			DragFinish(hdrop);
			this->on_file_drop((struct widget_viewport*)this, (const char * const *)filenames, this->dropuserdata);
			for (UINT i=0;i<numfiles;i++) free(filenames[i]);
		}
		break;
		_default:
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}



//http://msdn.microsoft.com/en-us/library/windows/desktop/bb774737(v=vs.85).aspx
//http://www.codeproject.com/Articles/7891/Using-virtual-lists - codeproject being useful for once
struct widget_listbox_win32 {
	struct widget_listbox i;
	
	struct window * parent;
	HWND hwnd;
	
	unsigned int rows;
	unsigned int columns;
	
	unsigned int columnwidthsum;
	unsigned int * columnwidths;
	
	const char * (*get_cell)(struct widget_listbox * subject, unsigned int row, unsigned int column, void* userdata);
	int (*search)(struct widget_listbox * subject, unsigned int start, const char * str, void* userdata);
	void* virt_userdata;
	
	struct _widget_listbox_devirt * devirt;
	
	void (*onactivate)(struct widget_listbox * subject, unsigned int row, void* userdata);
	void* act_userdata;
	
	bool* checkboxes;
	void (*ontoggle)(struct widget_listbox * subject, unsigned int row, bool state, void* userdata);
	void* tg_userdata;
};

static unsigned int listbox__init(struct widget_base * this_, struct window * parent, uintptr_t parenthandle)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	this->parent=parent;
	const char * * columns=(const char**)this->hwnd;
	this->hwnd=CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, "", WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_OWNERDATA,
	                          0, 0, 16, 16, (HWND)parenthandle, (HMENU)CTID_LISTVIEW, GetModuleHandle(NULL), NULL);
	
	SetWindowLongPtr(this->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SendMessage(this->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	
	this->i.base._width=1+19+1+this->columns*20;
	this->i.base._height=1+25+19*0+2;
	LVCOLUMN col;
	col.mask=/*LVCF_FMT|*/LVCF_WIDTH|LVCF_TEXT;
	col.fmt=LVCFMT_LEFT|256/*LVCFMT_FIXED_WIDTH*/;
	col.cx=20;
	for (unsigned int i=0;i<this->columns;i++)
	{
		//measure_text(columns[i], (unsigned int*)&col.cx, NULL);
		//col.cx+=12;
		col.pszText=(char*)columns[i];
		ListView_InsertColumn(this->hwnd, i, &col);
		this->columnwidths[i]=1;
		//this->i.base._width+=col.cx;
	}
	this->columnwidthsum=this->columns;
	
	this->checkboxes=NULL;
	
	free(columns);
	//pretty sure the listbox has children, but I can't ask how many of those there are. Probably varies if I add checkboxes, too.
	//But I'm not the one resizing them, so whatever.
	return 1;
}

static void listbox__measure(struct widget_base * this_) {}

static void listbox__place(struct widget_base * this_, void* resizeinf,
                           unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	place_window(this->hwnd, resizeinf, x, y, width, height);
	
	//microsoft, which drugs did you take this time
	LVCOLUMN mkrz;
	mkrz.mask=LVCF_FMT;
	mkrz.fmt=LVCFMT_LEFT;
	LVCOLUMN mknrz;
	mknrz.mask=LVCF_FMT;
	mknrz.fmt=LVCFMT_LEFT|256;
	
	width-=GetSystemMetrics(SM_CXVSCROLL)+4;
	unsigned char width_div=this->columnwidthsum;
	unsigned int width_frac=0;
	for (unsigned int i=0;i<this->columns;i++)
	{
		width_frac+=this->columnwidths[i]*width;
		unsigned int yourwidth=width_frac/width_div;
		width_frac%=width_div;
		ListView_SetColumn(this->hwnd, i, &mkrz);
		ListView_SetColumnWidth(this->hwnd, i, yourwidth);
		ListView_SetColumn(this->hwnd, i, &mknrz);
//printf("%i[%i:%i],",yourwidth,q,GetLastError());
	}
//puts("");
}

static void listbox__free(struct widget_base * this_)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	_widget_listbox_devirt_free(this->devirt);
	free(this->columnwidths);
	free(this->checkboxes);
	free(this);
}

static void listbox_set_enabled(struct widget_listbox * this_, bool enable)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	EnableWindow(this->hwnd, enable);
}

static void listbox_set_contents(struct widget_listbox * this_, unsigned int rows, const char * * contents)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	this->devirt=_widget_listbox_devirt_create(this_, rows, this->columns, contents);
}

static void listbox_set_contents_virtual(struct widget_listbox * this_, unsigned int rows,
                                         const char * (*get_cell)(struct widget_listbox * subject,
                                                                  unsigned int row, unsigned int column,
                                                                  void* userdata),
                                         int (*search)(struct widget_listbox * subject, unsigned int start,
                                                       const char * str, void* userdata),
                                         void* userdata)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	_widget_listbox_devirt_free(this->devirt);
	this->devirt=NULL;
	this->rows=rows;
	this->get_cell=get_cell;
	this->search=search;
	this->virt_userdata=userdata;
	
	if (this->checkboxes)
	{
		free(this->checkboxes);
		this->checkboxes=calloc(sizeof(bool), rows);
	}
	
	ListView_SetItemCountEx(this->hwnd, rows, 0);
	ListView_RedrawItems(this->hwnd, 0, rows-1);
}

static void listbox_set_row(struct widget_listbox * this_, unsigned int row, const char * * contents)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	if (this->devirt) _widget_listbox_devirt_set_row(this->devirt, row, contents);
	
	ListView_RedrawItems(this->hwnd, row, row);
}

static void listbox_refresh(struct widget_listbox * this_)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	ListView_RedrawItems(this->hwnd, 0, this->rows-1);
}

static void listbox_set_size(struct widget_listbox * this_, unsigned int height, const unsigned int * widths)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	
	int widthpx=-1;
	int heightpx=1+25+height*19+2;
	
	if (widths)
	{
		widthpx=1+19+1;
		this->columnwidthsum=0;
		for (unsigned int i=0;i<this->columns;i++)
		{
			ListView_SetColumnWidth(this->hwnd, i, widths[i]*xwidth/12+2);
			widthpx+=widths[i]*xwidth/12+2;
			this->columnwidths[i]=widths[i];
			this->columnwidthsum+=widths[i];
		}
	}
	
	DWORD widthheight=ListView_ApproximateViewRect(this->hwnd, widthpx, heightpx, height);
	this->i.base._width=LOWORD(widthheight);
	this->i.base._height=HIWORD(widthheight)-GetSystemMetrics(SM_CYHSCROLL)+2;//microsoft really aren't making this easy for me
//printf("%u->%u %u->%u\n",widthpx,this->i.base._width,heightpx,this->i.base._height);
	
	this->parent->_reflow(this->parent);
}

static int listbox_get_active_row(struct widget_listbox * this_)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	return ListView_GetSelectionMark(this->hwnd);
}

static void listbox_set_onactivate(struct widget_listbox * this_,
                                   void (*onactivate)(struct widget_listbox * subject, unsigned int row, void* userdata),
                                   void* userdata)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	this->onactivate=onactivate;
	this->act_userdata=userdata;
}

static void listbox_add_checkboxes(struct widget_listbox * this_,
                                   void (*ontoggle)(struct widget_listbox * subject, unsigned int row, bool state, void* userdata),
                                   void* userdata)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	ListView_SetExtendedListViewStyleEx(this->hwnd, LVS_EX_CHECKBOXES, LVS_EX_CHECKBOXES);
	this->ontoggle=ontoggle;
	this->tg_userdata=userdata;
	this->checkboxes=calloc(sizeof(bool), this->rows);
}

static bool listbox_get_checkbox_state(struct widget_listbox * this_, unsigned int row)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	return this->checkboxes[row];
}

static void listbox_set_checkbox_state(struct widget_listbox * this_, unsigned int row, bool state)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	this->checkboxes[row]=state;
	ListView_RedrawItems(this->hwnd, row, row);
}

static void listbox_set_all_checkboxes(struct widget_listbox * this_, bool * states)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	memcpy(this->checkboxes, states, sizeof(bool)*this->rows);
	ListView_RedrawItems(this->hwnd, 0, this->rows-1);
}

struct widget_listbox * widget_create_listbox_l(unsigned int numcolumns, const char * * columns)
{
	struct widget_listbox_win32 * this=malloc(sizeof(struct widget_listbox_win32));
	this->i.base._init=listbox__init;
	this->i.base._measure=listbox__measure;
	this->i.base._widthprio=3;
	this->i.base._heightprio=3;
	this->i.base._place=listbox__place;
	this->i.base._free=listbox__free;
	
	this->i.set_enabled=listbox_set_enabled;
	this->i.set_contents=listbox_set_contents;
	this->i.set_contents_virtual=listbox_set_contents_virtual;
	this->i.set_row=listbox_set_row;
	this->i.refresh=listbox_refresh;
	this->i.set_size=listbox_set_size;
	this->i.get_active_row=listbox_get_active_row;
	this->i.set_onactivate=listbox_set_onactivate;
	this->i.add_checkboxes=listbox_add_checkboxes;
	this->i.get_checkbox_state=listbox_get_checkbox_state;
	this->i.set_checkbox_state=listbox_set_checkbox_state;
	this->i.set_all_checkboxes=listbox_set_all_checkboxes;
	
	this->columnwidths=malloc(sizeof(unsigned int)*numcolumns);
	this->columns=numcolumns;
	this->devirt=NULL;
	this->onactivate=NULL;
	this->checkboxes=NULL;
	
	this->hwnd=malloc(sizeof(const char*)*numcolumns);
	memcpy(this->hwnd, columns, sizeof(const char*)*numcolumns);
	
	return (struct widget_listbox*)this;
}

static uintptr_t listbox_notify(NMHDR* nmhdr)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
	if (nmhdr->code==LVN_ITEMCHANGED)
	{
		//ignore, seems only useful for non-virtual listboxes
		/*
puts("itc");
		if (recursive) return 0;
		if (!this->ontoggle) return 0;
		//this is probably not guaranteed by docs, but it works. Windows 7 ain't gonna change, and fuck everything else.
		const unsigned int LBST_CHECKED=2<<12;
		const unsigned int LBST_UNCHECKED=1<<12;
		NMLISTVIEW* nm=(NMLISTVIEW*)nmhdr;
		if ((nm->uNewState&LVIS_STATEIMAGEMASK)==LBST_CHECKED && (nm->uOldState&LVIS_STATEIMAGEMASK)==LBST_UNCHECKED)
		{
puts("itc tg");
			this->ontoggle((struct widget_listbox*)this, nm->iItem, true, this->tg_userdata);
		}
		if ((nm->uNewState&LVIS_STATEIMAGEMASK)==LBST_UNCHECKED && (nm->uOldState&LVIS_STATEIMAGEMASK)==LBST_CHECKED)
		{
puts("itc tg");
			this->ontoggle((struct widget_listbox*)this, nm->iItem, false, this->tg_userdata);
		}
		*/
	}
	if (nmhdr->code==LVN_GETDISPINFO)
	{
		LV_DISPINFO* info=(LV_DISPINFO*)nmhdr;
		
		unsigned int row=info->item.iItem;
		unsigned int column=info->item.iSubItem;
		
		if (info->item.mask & LVIF_TEXT)
		{
			const char * str=this->get_cell((struct widget_listbox*)this, row, column, this->virt_userdata);
			unsigned int len=strlen(str);
			if (len > info->item.cchTextMax-1) len=info->item.cchTextMax-1;
			memcpy(info->item.pszText, str, len);
			info->item.pszText[len]='\0';
		}
		
		if (info->item.mask & LVIF_IMAGE)
		{
			info->item.iImage=0;
			info->item.mask|=LVIF_STATE;
			info->item.stateMask=LVIS_STATEIMAGEMASK;
			info->item.state=INDEXTOSTATEIMAGEMASK(this->checkboxes ? this->checkboxes[row] ? 2 : 1 : 0);
		}
	}
	if (nmhdr->code==LVN_ODCACHEHINT)
	{
		//ignore
	}
	if (nmhdr->code==LVN_ODFINDITEM)
	{
		NMLVFINDITEM* find=(NMLVFINDITEM*)nmhdr;
		if (!(find->lvfi.flags&LVFI_STRING)) return 0;
		
		if (this->search)
		{
			return (uintptr_t)this->search((struct widget_listbox*)this, find->iStart, find->lvfi.psz, this->virt_userdata);
		}
		else
		{
			return _widget_listbox_search((struct widget_listbox*)this, this->rows, this->get_cell,
			                              find->iStart, find->lvfi.psz, this->virt_userdata);
		}
	}
	if (nmhdr->code==LVN_KEYDOWN)
	{
		NMLVKEYDOWN* keydown=(NMLVKEYDOWN*)nmhdr;
		if (keydown->wVKey==VK_SPACE && this->checkboxes)
		{
			int row=ListView_GetSelectionMark(this->hwnd);
			if (row!=-1)
			{
				this->checkboxes[row]^=1;
				ListView_RedrawItems(this->hwnd, row, row);
				if (this->ontoggle) this->ontoggle((struct widget_listbox*)this, row, this->checkboxes[row], this->tg_userdata);
			}
		}
		if (keydown->wVKey==VK_RETURN)
		{
			int row=ListView_GetSelectionMark(this->hwnd);
			if (row!=-1 && this->onactivate) this->onactivate((struct widget_listbox*)this, row, this->act_userdata);
		}
	}
	if (nmhdr->code==NM_CLICK)
	{
		NMITEMACTIVATE* click=(NMITEMACTIVATE*)nmhdr;
		LVHITTESTINFO hitinfo;
		hitinfo.pt=click->ptAction;
		int row=ListView_HitTest(this->hwnd, &hitinfo);
		if (row!=-1 && (hitinfo.flags & LVHT_ONITEMSTATEICON))
		{
			this->checkboxes[row]^=1;
			ListView_RedrawItems(this->hwnd, row, row);
			if (this->ontoggle) this->ontoggle((struct widget_listbox*)this, row, this->checkboxes[row], this->tg_userdata);
		}
	}
	if (nmhdr->code==NM_DBLCLK)
	{
		NMITEMACTIVATE* click=(NMITEMACTIVATE*)nmhdr;
		LVHITTESTINFO hitinfo;
		hitinfo.pt=click->ptAction;
		int row=ListView_HitTest(this->hwnd, &hitinfo);
		if (row!=-1 && (hitinfo.flags & LVHT_ONITEMLABEL))
		{
			if (this->onactivate) this->onactivate((struct widget_listbox*)this, row, this->act_userdata);
		}
	}
	return 0;
}



struct widget_frame_win32 {
	struct widget_frame i;
	
	//struct window * parent;
	HWND hwnd;
	
	struct widget_base * child;
	
	void (*onclick)(struct widget_frame * frame, void* userdata);
	void* userdata;
};

static unsigned int frame__init(struct widget_base * this_, struct window * parent, uintptr_t parenthandle)
{
	struct widget_frame_win32 * this=(struct widget_frame_win32*)this_;
	//this->parent=parent;
	this->hwnd=CreateWindow(WC_BUTTON, (char*)this->hwnd, WS_CHILD|WS_VISIBLE|WS_GROUP|BS_GROUPBOX, 0, 0, 16, 16,
	                        (HWND)parenthandle, (HMENU)CTID_NONINTERACTIVE, GetModuleHandle(NULL), NULL);
	//SetWindowLongPtr(this->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SendMessage(this->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	return 1+this->child->_init(this->child, parent, parenthandle);
}

static void frame__measure(struct widget_base * this_)
{
	struct widget_frame_win32 * this=(struct widget_frame_win32*)this_;
	this->child->_measure(this->child);
	this->i.base._width=this->child->_width + frame_left+frame_right;
	this->i.base._height=this->child->_height + frame_top+frame_bottom;
	this->i.base._widthprio=this->child->_widthprio;
	this->i.base._heightprio=this->child->_heightprio;
}

static void frame__place(struct widget_base * this_, void* resizeinf,
                         unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	struct widget_frame_win32 * this=(struct widget_frame_win32*)this_;
	this->child->_place(this->child, resizeinf, x+frame_left, y+frame_top,
	                    width-frame_left-frame_right, height-frame_top-frame_bottom);
	place_window(this->hwnd, resizeinf, x, y, width, height);
}

static void frame__free(struct widget_base * this_)
{
	struct widget_frame_win32 * this=(struct widget_frame_win32*)this_;
	free(this);
}

static void frame_set_text(struct widget_frame * this_, const char * text)
{
	struct widget_frame_win32 * this=(struct widget_frame_win32*)this_;
	SetWindowText(this->hwnd, text);
}

struct widget_frame * widget_create_frame(const char * text, void* contents)
{
	struct widget_frame_win32 * this=malloc(sizeof(struct widget_frame_win32));
	this->i.base._init=frame__init;
	this->i.base._measure=frame__measure;
	this->i.base._place=frame__place;
	this->i.base._free=frame__free;
	
	this->i.set_text=frame_set_text;
	
	this->child=contents;
	
	this->hwnd=(HWND)text;
	
	return (struct widget_frame*)this;
}



uintptr_t _window_notify_inner(void* notification)
{
	NMHDR* nmhdr=notification;
	switch (nmhdr->idFrom)
	{
		case CTID_NONINTERACTIVE: break;
		case CTID_BUTTON:
		case CTID_DEFBUTTON:
		{
			if (nmhdr->code==BN_CLICKED)
			{
				struct widget_button_win32 * this=(struct widget_button_win32*)GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
				if (this->onclick)
				{
					this->onclick((struct widget_button*)this, this->userdata);
				}
			}
			break;
		}
		case CTID_RADIO:
		{
			if (nmhdr->code==BN_CLICKED)
			{
				struct widget_radio_win32 * this=(struct widget_radio_win32*)GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
				radio_set_state((struct widget_radio*)this->leader, this->id);
				if (this->leader->onclick)
				{
					this->leader->onclick((struct widget_radio*)this->leader, this->id, this->leader->userdata);
				}
			}
			break;
		}
		case CTID_TEXTBOX:
		{
			if (nmhdr->code==EN_CHANGE)
			{
				struct widget_textbox_win32 * this=(struct widget_textbox_win32*)GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
				if (this->invalid)
				{
					this->invalid=false;
					InvalidateRect(this->hwnd, NULL, FALSE);
				}
				if (this->onchange)
				{
					this->onchange((struct widget_textbox*)this, textbox_get_text((struct widget_textbox*)this), this->ch_userdata);
				}
			}
			break;
		}
		case CTID_LISTVIEW:
		{
			return listbox_notify(nmhdr);
			break;
		}
	}
	return 0;
}

uintptr_t _window_get_widget_color(unsigned int type, void* handle, void* draw, void* parent)
{
	switch (GetDlgCtrlID(handle))
	{
		case CTID_TEXTBOX:
		{
			struct widget_textbox_win32 * this=(struct widget_textbox_win32*)GetWindowLongPtr(handle, GWLP_USERDATA);
			if (this->invalid)
			{
				SetBkMode((HDC)draw, TRANSPARENT);
				return (LRESULT)bg_invalid;
			}
			break;
		}
	}
	return DefWindowProcA(parent, type, (WPARAM)draw, (LPARAM)handle);
}
#endif
