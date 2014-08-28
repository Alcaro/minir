#include "minir.h"
#ifdef WINDOW_WIN32
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#define _WIN32_IE 0x0600
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdlib.h>
#undef this

//controls HIG and screenshots of them http://msdn.microsoft.com/en-us/library/dn742399.aspx
//controls docs http://msdn.microsoft.com/en-us/library/windows/desktop/bb773169%28v=vs.85%29.aspx
//alternatively http://msdn.microsoft.com/en-us/library/aa368039%28v=vs.85%29.aspx for some widgets

//NOTE: Widgets must respond to measure() before init() is called. Do not measure them in init.

#define dpi_vert 96
#define dpi_horz 96

#define btn_width 75
#define btn_height 23

#define checkbox_width 17
#define checkbox_height 17

#define radio_width 17
#define radio_height 17

#define frame_top 16
#define frame_left 4
#define frame_right 4
#define frame_bottom 4

#define g_padding 3

#define TIMER_MOUSEHIDE 1

//Keep noninteractive at 0 and defbutton at button+1.
//Otherwise, no rules for which values mean what; the IDs only mean anything inside this file.
#define CTID_NONINTERACTIVE 0
#define CTID_BUTTON 1
#define CTID_DEFBUTTON 2
#define CTID_CHECK 3
#define CTID_RADIO 4
#define CTID_LISTVIEW 5
#define CTID_TEXTBOX 6

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

//static LRESULT CALLBACK viewport_WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
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
	
	//WNDCLASS wc;
	//wc.style=0;
	//wc.lpfnWndProc=viewport_WindowProc;
	//wc.cbClsExtra=0;
	//wc.cbWndExtra=0;
	//wc.hInstance=GetModuleHandle(NULL);
	//wc.hIcon=LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(0));
	//wc.hCursor=LoadCursor(NULL, IDC_ARROW);
	//wc.hbrBackground=GetSysColorBrush(COLOR_3DFACE);//(HBRUSH)(COLOR_WINDOW + 1);
	//wc.lpszMenuName=NULL;
	//wc.lpszClassName="minir_viewport";
	//RegisterClass(&wc);
	
	measure_text("XXXXXXXXXXXX", &xwidth, NULL);
	
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



struct widget_label::impl {
	struct window * parent;
	HWND hwnd;
	uint8_t state;
	//char padding[7];
};

widget_label::widget_label(const char * text) : m(new impl)
{
	this->widthprio=1;
	this->heightprio=1;
	
	m->hwnd=(HWND)strdup(text);
	measure_text(text, &this->width, &this->height);
	this->width+=g_padding*2; this->height+=g_padding*2;
	m->state=0;
}

unsigned int widget_label::init(struct window * parent, uintptr_t parenthandle)
{
	m->parent=parent;
	char* text=(char*)m->hwnd;
	m->hwnd=CreateWindow(WC_STATIC, text, WS_CHILD|WS_VISIBLE|SS_NOPREFIX,
	                     0, 0, 16, 16, // just some random sizes, we'll resize it in _place()
	                     (HWND)parenthandle, NULL, GetModuleHandle(NULL), NULL);
	free(text);
	if (m->state==1) EnableWindow(m->hwnd, false);
	m->state=2;
	SendMessage(m->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	return 1;
}

void widget_label::measure() {}

void widget_label::place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	x+=g_padding; y+=g_padding; width-=g_padding*2; /*height-=g_padding*2;*/
	place_window(m->hwnd, resizeinf, x,y+(height-this->height)/2, width,this->height);
}

widget_label::~widget_label()
{
	delete m;
}

widget_label* widget_label::set_enabled(bool enable)
{
	if (m->state==2) EnableWindow(m->hwnd, enable);
	else m->state=!enable;
	return this;
}

widget_label* widget_label::set_text(const char * text)
{
	measure_text(text, &this->width, &this->height);
	this->width+=g_padding*2; this->height+=g_padding*2;
	m->parent->_reflow(m->parent);
	return this;
}

widget_label* widget_label::set_ellipsize(bool ellipsize)
{
//puts("FIXME: label_set_ellipsize");
	return this;
}

widget_label* widget_label::set_alignment(int alignment)
{
//puts("FIXME: label_set_alignment");
	return this;
}



struct widget_button::impl {
	//struct window * parent;
	HWND hwnd;
	
	void (*onclick)(struct widget_button * subject, void* userdata);
	void* userdata;
	
	uint8_t state;
	//char padding[7];
};

widget_button::widget_button(const char * text) : m(new impl)
{
	this->widthprio=1;
	this->heightprio=1;
	
	this->width=btn_width+g_padding*2;
	this->height=btn_height+g_padding*2;
	
	m->onclick=NULL;
	m->state=0;
	m->hwnd=(HWND)strdup(text);
}

unsigned int widget_button::init(struct window * parent, uintptr_t parenthandle)
{
	char* text=(char*)m->hwnd;
	m->hwnd=CreateWindow(WC_BUTTON, text, WS_CHILD|WS_VISIBLE|WS_TABSTOP, 0, 0, 16, 16,
	                     (HWND)parenthandle, (HMENU)CTID_BUTTON, GetModuleHandle(NULL), NULL);
	free(text);
	if (m->state==1) EnableWindow(m->hwnd, false);
	m->state=2;
	SetWindowLongPtr(m->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SendMessage(m->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	return 1;
}

void widget_button::measure() {}

void widget_button::place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	x+=g_padding; y+=g_padding; width-=g_padding*2; height-=g_padding*2;
	place_window(m->hwnd, resizeinf, x, y, width, height);
}

widget_button::~widget_button()
{
	delete m;
}

widget_button* widget_button::set_enabled(bool enable)
{
	if (m->state&2) EnableWindow(m->hwnd, enable);
	else m->state=!enable;
	return this;
}

widget_button* widget_button::set_text(const char * text)
{
	SetWindowText(m->hwnd, text);
	return this;
}

widget_button* widget_button::set_onclick(void (*onclick)(struct widget_button * subject, void* userdata), void* userdata)
{
	m->onclick=onclick;
	m->userdata=userdata;
	return this;
}



struct widget_checkbox::impl {
	struct window * parent;
	HWND hwnd;
	
	void (*onclick)(struct widget_checkbox * subject, bool checked, void* userdata);
	void* userdata;
	
	uint8_t state;
	//char padding[7];
};

widget_checkbox::widget_checkbox(const char * text) : m(new impl)
{
	this->widthprio=1;
	this->heightprio=1;
	
	this->width=btn_width+g_padding*2;
	this->height=btn_height+g_padding*2;
	
	m->onclick=NULL;
	m->state=0;
	m->hwnd=(HWND)strdup(text);
}

unsigned int widget_checkbox::init(struct window * parent, uintptr_t parenthandle)
{
	m->parent=parent;
	char* text=(char*)m->hwnd;
	m->hwnd=CreateWindow(WC_BUTTON, "", WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_CHECKBOX, 0, 0, 16, 16,
	                     (HWND)parenthandle, (HMENU)CTID_CHECK, GetModuleHandle(NULL), NULL);
	if (m->state&2) EnableWindow(m->hwnd, false);
	if (m->state&4) Button_SetCheck(m->hwnd, BST_CHECKED);
	m->state=0x01;
	SetWindowLongPtr(m->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SendMessage(m->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	
	this->set_text(text);
	free(text);
	return 1;
}

void widget_checkbox::measure() {}

void widget_checkbox::place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	x+=g_padding; y+=g_padding; width-=g_padding*2; height-=g_padding*2;
	place_window(m->hwnd, resizeinf, x, y, width, height);
}

widget_checkbox::~widget_checkbox()
{
	delete m;
}

widget_checkbox* widget_checkbox::set_enabled(bool enable)
{
	if (m->state==1) EnableWindow(m->hwnd, enable);
	else m->state=((m->state&0xFD)|(!enable<<1));
	return this;
}

widget_checkbox* widget_checkbox::set_text(const char * text)
{
	if (m->state==1)
	{
		measure_text(text, &this->width, NULL);
		this->width+=checkbox_width + g_padding*2;
		this->height=checkbox_height + g_padding*2;
		
		SetWindowText(m->hwnd, text);
		m->parent->_reflow(m->parent);
	}
	else
	{
		free((char*)m->hwnd);
		m->hwnd=(HWND)strdup(text);
	}
	return this;
}

bool widget_checkbox::get_state()
{
	if (m->state==1) return (Button_GetCheck(m->hwnd)==BST_CHECKED);
	else return (m->state&2);
}

widget_checkbox* widget_checkbox::set_state(bool checked)
{
	if (m->state==1) Button_SetCheck(m->hwnd, checked ? BST_CHECKED : BST_UNCHECKED);
	else m->state=((m->state&0xFB)|(checked<<2));
	return this;
}

widget_checkbox* widget_checkbox::set_onclick(void (*onclick)(struct widget_checkbox * subject, bool checked, void* userdata),
                                              void* userdata)
{
	m->onclick=onclick;
	m->userdata=userdata;
	return this;
}



struct widget_radio::impl {
	UNION_BEGIN
		STRUCT_BEGIN//active if !initialized (if init() has not been called)
			char* text;//strdup'd
			bool disabled;
		STRUCT_END
		STRUCT_BEGIN//active otherwise
			struct window * parent;
			HWND hwnd;
		STRUCT_END
	UNION_END
	
	bool initialized;
	//char padding[3];
	
	//this can point to itself
	unsigned int id;
	widget_radio* leader;
	
	widget_radio* * group;
	
	UNION_BEGIN
		//which one is active depends on 'id'; if it's nonzero, 'last' is valid
		STRUCT_BEGIN//active if id==0
			void (*onclick)(struct widget_radio * subject, unsigned int state, void* userdata);
			void* userdata;
			unsigned int active;
		STRUCT_END
		STRUCT_BEGIN//active otherwise
			bool last;//the 'first' flag is known by checking for id==0
			//char padding[19];
		STRUCT_END
	UNION_END
	//char padding[4];
};

widget_radio::widget_radio(const char * text) : m(new impl)
{
	this->widthprio=1;
	this->heightprio=1;
	
	measure_text(text, &this->width, &this->height);
	this->width+=this->height;
	this->width+=g_padding*2;
	
	m->leader=NULL;
	m->onclick=NULL;
	m->group=NULL;
	m->active=0;
	
	m->initialized=false;
	m->disabled=false;
	m->text=strdup(text);
}

unsigned int widget_radio::init(struct window * parent, uintptr_t parenthandle)
{
	bool disabled=m->disabled;
	char* text=m->text;
	
	m->parent=parent;
	m->hwnd=CreateWindow(WC_BUTTON, text, WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_RADIOBUTTON, 0, 0, 16, 16,
	                     (HWND)parenthandle, (HMENU)CTID_RADIO, GetModuleHandle(NULL), NULL);
	SetWindowLongPtr(m->hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SendMessage(m->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	if (m->leader && m->id==m->leader->m->active) Button_SetCheck(m->hwnd, BST_CHECKED);
	if (disabled) EnableWindow(m->hwnd, false);
	
	m->initialized=true;
	this->set_text(text);
	free(text);
	
	return 1;
}

void widget_radio::measure() {}

void widget_radio::place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	x+=g_padding; width-=g_padding*2;
	if (m->id==0)
	{
		y+=g_padding;
		height-=g_padding;
	}
	else if (m->last)
	{
		height-=g_padding;
	}
	place_window(m->hwnd, resizeinf, x, y, width, height);
}

widget_radio::~widget_radio()
{
	free(m->group);//this frees NULL on anything except the leader
	delete m;
}

widget_radio* widget_radio::set_enabled(bool enable)
{
	if (m->initialized) EnableWindow(m->hwnd, enable);
	else m->disabled=!enable;
	return this;
}

widget_radio* widget_radio::set_text(const char * text)
{
	if (m->initialized)
	{
		measure_text(text, &this->width, NULL);
		this->width+=checkbox_width + g_padding*2;
		this->height=checkbox_height;
		
		if (m->id==0 || m->last) this->height+=g_padding;
		
		SetWindowText(m->hwnd, text);
		m->parent->_reflow(m->parent);
	}
	else
	{
		free(m->text);
		m->text=strdup(text);
	}
	return this;
}

widget_radio* widget_radio::group(unsigned int numitems, widget_radio* * group)
{
	for (unsigned int i=0;i<numitems;i++)
	{
		group[i]->m->leader=this;
		group[i]->m->id=i;
		if (i)
		{
			group[i]->m->last=false;
			if (i==numitems-1)
			{
				group[i]->m->last=true;
				group[i]->height+=g_padding;
			}
		}
	}
	
	m->group=malloc(sizeof(widget_radio*)*numitems);
	memcpy(m->group, group, sizeof(widget_radio*)*numitems);
	if (m->initialized) Button_SetCheck(m->hwnd, BST_CHECKED);
	this->height+=g_padding;
	if (m->initialized && m->parent) m->parent->_reflow(m->parent);
	
	return this;
}

unsigned int widget_radio::get_state()
{
	return m->active;
}

widget_radio* widget_radio::set_state(unsigned int state)
{
	Button_SetCheck(m->group[m->active]->m->hwnd, BST_UNCHECKED);
	Button_SetCheck(m->group[state]->m->hwnd, BST_CHECKED);
	m->active=state;
	return this;
}

widget_radio* widget_radio::set_onclick(void (*onclick)(struct widget_radio * subject, unsigned int state, void* userdata),
                                        void* userdata)
{
	m->onclick=onclick;
	m->userdata=userdata;
	
	return this;
}



struct widget_textbox::impl {
	UNION_BEGIN
		STRUCT_BEGIN
			struct window * parent;
			HWND hwnd;
		STRUCT_END
		STRUCT_BEGIN
			bool disabled;
			bool focus;
			//char padding[10];
			
			unsigned int maxchars;
		STRUCT_END
	UNION_END
	
	char * text;
	
	bool initialized;
	bool invalid;
	//char padding[6];
	
	void (*onchange)(struct widget_textbox * subject, const char * text, void* userdata);
	void* ch_userdata;
	void (*onactivate)(struct widget_textbox * subject, const char * text, void* userdata);
	void* ac_userdata;
};

static LRESULT CALLBACK textbox_SubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                             UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

widget_textbox::widget_textbox() : m(new impl)
{
	m->initialized=false;
	m->disabled=false;
	m->focus=false;
	m->maxchars=0;
	
	m->text=NULL;
	m->onchange=NULL;
	m->onactivate=NULL;
	m->invalid=false;
	
	measure_text("XXXXXXXXXXXX", NULL, &this->height);
	this->width=5*xwidth/12 + 6 + g_padding*2;
	this->height+=6 + g_padding*2;
}

unsigned int widget_textbox::init(struct window * parent, uintptr_t parenthandle)
{
	HWND hwnd=CreateWindow(WC_EDIT, "", WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL, 0, 0, 16, 16,
	                       (HWND)parenthandle, (HMENU)CTID_TEXTBOX, GetModuleHandle(NULL), NULL);
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
	SendMessage(hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	SetWindowSubclass(hwnd, textbox_SubclassProc, 0, 0);
	if (m->disabled) EnableWindow(hwnd, false);
	if (m->focus) SetFocus(hwnd);
	if (m->text) SetWindowText(hwnd, m->text);
	
	this->set_length(m->maxchars);
	this->set_invalid(m->invalid);
	
	m->parent=parent;
	m->hwnd=hwnd;
	return 1;
}

void widget_textbox::measure() {}

void widget_textbox::place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	x+=g_padding; y+=g_padding; width-=g_padding*2; height-=g_padding*2;
	place_window(m->hwnd, resizeinf, x+1,y+1, width-2,height-2);//this math is to get the border to the inside of the widget
}

widget_textbox::~widget_textbox()
{
	free(m->text);
	delete m;
}

widget_textbox* widget_textbox::set_enabled(bool enable)
{
	if (m->initialized) EnableWindow(m->hwnd, enable);
	else m->disabled=!enable;
	return this;
}

widget_textbox* widget_textbox::focus()
{
	SetFocus(m->hwnd);
	return this;
}

const char * widget_textbox::get_text()
{
	free(m->text);
	unsigned int len=GetWindowTextLength(m->hwnd);
	m->text=malloc(len+1);
	GetWindowText(m->hwnd, m->text, len+1);
	return m->text;
}

widget_textbox* widget_textbox::set_text(const char * text)
{
	free(m->text);
	m->text=strdup(text);
	if (m->initialized) SetWindowText(m->hwnd, m->text);
	return this;
}

widget_textbox* widget_textbox::set_length(unsigned int maxlen)
{
	Edit_LimitText(m->hwnd, maxlen);//conveniently, we both chose 0 to mean unlimited
	return this;
}

widget_textbox* widget_textbox::set_width(unsigned int xs)
{
	this->width = xs*xwidth/12 + 6 + g_padding*2;
	m->parent->_reflow(m->parent);
	return this;
}

widget_textbox* widget_textbox::set_invalid(bool invalid)
{
	m->invalid=invalid;
	if (m->initialized)
	{
		InvalidateRect(m->hwnd, NULL, FALSE);
		if (invalid) SetFocus(m->hwnd);
	}
	return this;
}

widget_textbox* widget_textbox::set_onchange(void (*onchange)(struct widget_textbox * subject, const char * text, void* userdata),
                                             void* userdata)
{
	m->onchange=onchange;
	m->ch_userdata=userdata;
	return this;
}

widget_textbox* widget_textbox::set_onactivate(void (*onactivate)(widget_textbox* subject, const char * text, void* userdata),
                                               void* userdata)
{
	m->onactivate=onactivate;
	m->ac_userdata=userdata;
	return this;
}


static LRESULT CALLBACK textbox_SubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                             UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	widget_textbox* obj=(widget_textbox*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_KEYDOWN:
			if (wParam==VK_RETURN && !(lParam & 0x40000000) && obj->m->onactivate)
			{
				obj->m->onactivate(obj, obj->get_text(), obj->m->ac_userdata);
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



#if 0
struct widget_viewport_win32 {
	//struct widget_viewport i;
	
	struct window * parent;
	HWND hwnd;
	
	bool hide_cursor_user;
	bool hide_cursor_timer;
	LPARAM lastmousepos;
	
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
	this->i._base.width=width;
	this->i._base.height=height;
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
	this->i._base.init=viewport__init;
	this->i._base.measure=viewport__measure;
	this->i._base.widthprio=0;
	this->i._base.heightprio=0;
	this->i._base.place=viewport__place;
	this->i._base.free=viewport__free;
	
	this->i.resize=viewport_resize;
	this->i.get_window_handle=viewport_get_window_handle;
	this->i.set_hide_cursor=viewport_set_hide_cursor;
	this->i.set_support_drop=viewport_set_support_drop;
	
	this->i._base.width=width;
	this->i._base.height=height;
	//no padding here
	
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

#ifndef LVCFMT_FIXED_WIDTH
#define LVCFMT_FIXED_WIDTH 0x100
#endif

struct widget_listbox_win32 {
	//struct widget_listbox i;
	
	struct window * parent;
	HWND hwnd;
	
	size_t rows;
	unsigned int columns;
	
	unsigned int columnwidthsum;
	unsigned int * columnwidths;
	
	const char * (*get_cell)(struct widget_listbox * subject, size_t row, int column, void * userdata);
	size_t (*search)(struct widget_listbox * subject, const char * prefix, size_t start, bool up, void * userdata);
	void* virt_userdata;
	
	void (*onfocuschange)(struct widget_listbox * subject, size_t row, void* userdata);
	void* foc_userdata;
	void (*onactivate)(struct widget_listbox * subject, size_t row, void* userdata);
	void* act_userdata;
	
	bool checkboxes;
	void (*ontoggle)(struct widget_listbox * subject, size_t row, void* userdata);
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
	
	LVCOLUMN col;
	col.mask=LVCF_FMT|LVCF_WIDTH|LVCF_TEXT;
	col.fmt=LVCFMT_LEFT|LVCFMT_FIXED_WIDTH;
	col.cx=20;
	for (unsigned int i=0;i<this->columns;i++)
	{
		//measure_text(columns[i], (unsigned int*)&col.cx, NULL);
		//col.cx+=12;
		col.pszText=(char*)columns[i];
		(void)ListView_InsertColumn(this->hwnd, i, &col);
		this->columnwidths[i]=1;
		//this->i._base.width+=col.cx;
	}
	this->columnwidthsum=this->columns;
	
	this->checkboxes=NULL;
	
	free(columns);
	//pretty sure the listbox has children, but I can't ask how many of those there are. Probably varies if I add checkboxes, too.
	//But I'm not the one resizing them, so whatever.
	return 1;
}

static void listbox__measure(struct widget_base * this_) {}

static void listbox_resize_column(HWND hwnd, size_t/*TODO: check type*/ col, unsigned int width)
{
	//microsoft, which drugs did you take this time? LVCFMT_FIXED_WIDTH blocks me from resizing from code too!
	LVCOLUMN mkrz;
	mkrz.mask=LVCF_FMT;
	mkrz.fmt=LVCFMT_LEFT;
	LVCOLUMN mknrz;
	mknrz.mask=LVCF_FMT;
	mknrz.fmt=LVCFMT_LEFT|LVCFMT_FIXED_WIDTH;
	
	(void)ListView_SetColumn(hwnd, col, &mkrz);
	(void)ListView_SetColumnWidth(hwnd, col, width);
	(void)ListView_SetColumn(hwnd, col, &mknrz);
}

static void listbox__place(struct widget_base * this_, void* resizeinf,
                           unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	x+=g_padding; y+=g_padding; width-=g_padding*2; height-=g_padding*2;
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	place_window(this->hwnd, resizeinf, x, y, width, height);
	
	width-=GetSystemMetrics(SM_CXVSCROLL)+4;
	unsigned char width_div=this->columnwidthsum;
	unsigned int width_frac=0;
	for (unsigned int i=0;i<this->columns;i++)
	{
		width_frac+=this->columnwidths[i]*width;
		unsigned int yourwidth=width_frac/width_div;
		width_frac%=width_div;
		listbox_resize_column(this->hwnd, i, yourwidth);
	}
}

static void listbox__free(struct widget_base * this_)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	free(this->columnwidths);
	free(this);
}

static void listbox_set_enabled(struct widget_listbox * this_, bool enable)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	EnableWindow(this->hwnd, enable);
}

static void listbox_set_num_rows(struct widget_listbox * this_, size_t rows)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	if (rows > 100000000) rows=100000000;//Windows "only" supports 100 million (0x05F5E100); more than that and it gets empty.
	this->rows=rows;
	ListView_SetItemCountEx(this->hwnd, rows, 0);
	(void)ListView_RedrawItems(this->hwnd, 0, rows-1);
}

static void listbox_set_contents(struct widget_listbox * this_,
                                 const char * (*get_cell)(struct widget_listbox * subject,
                                                          size_t row, int column,
                                                          void* userdata),
                                 size_t (*search)(struct widget_listbox * subject,
                                                  const char * prefix, size_t start, bool up, void* userdata),
                                 void* userdata)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	this->get_cell=get_cell;
	this->search=search;
	this->virt_userdata=userdata;
}

static void listbox_refresh(struct widget_listbox * this_, size_t row)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	if (row==(size_t)-1) (void)ListView_RedrawItems(this->hwnd, 0, this->rows-1);
	else (void)ListView_RedrawItems(this->hwnd, row, row);
}

static void listbox_set_size(struct widget_listbox * this_, unsigned int height, const unsigned int * widths, int expand)
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
			listbox_resize_column(this->hwnd, i, widths[i]*xwidth/12+12+5+(i==0));//why are these columns not resized to the sizes I tell them to resize to
			widthpx+=widths[i]*xwidth/12+12+5;
			this->columnwidths[i]=widths[i];
			this->columnwidthsum+=widths[i];
		}
	}
	
	if (expand!=-1)
	{
		LVCOLUMN mkrz;
		mkrz.mask=LVCF_FMT;
		mkrz.fmt=LVCFMT_LEFT;
		(void)ListView_SetColumn(this->hwnd, expand, &mkrz);
	}
	
	DWORD widthheight=ListView_ApproximateViewRect(this->hwnd, widthpx, heightpx, height);
	this->i._base.width=LOWORD(widthheight);
	this->i._base.height=HIWORD(widthheight)-GetSystemMetrics(SM_CYHSCROLL)+2;//microsoft really isn't making this easy for me
	this->i._base.width+=g_padding*2; this->i._base.height+=g_padding*2;
//printf("%u->%u %u->%u\n",widthpx,this->i._base.width,heightpx,this->i._base.height);
	
	this->parent->_reflow(this->parent);
}

static size_t listbox_get_active_row(struct widget_listbox * this_)
//TODO: check retval from ListView_GetSelectionMark if there is no selection
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	return ListView_GetSelectionMark(this->hwnd);
}

static void listbox_set_on_focus_change(struct widget_listbox * this_,
                                        void (*onchange)(struct widget_listbox * subject, size_t row, void * userdata),
                                        void* userdata)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	this->onfocuschange=onchange;
	this->foc_userdata=userdata;
}

static void listbox_set_onactivate(struct widget_listbox * this_,
                                   void (*onactivate)(struct widget_listbox * subject, size_t row, void* userdata),
                                   void* userdata)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	this->onactivate=onactivate;
	this->act_userdata=userdata;
}

static void listbox_add_checkboxes(struct widget_listbox * this_,
                                   void (*ontoggle)(struct widget_listbox * subject, size_t row, void* userdata),
                                   void* userdata)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)this_;
	(void)ListView_SetExtendedListViewStyleEx(this->hwnd, LVS_EX_CHECKBOXES, LVS_EX_CHECKBOXES);
	this->ontoggle=ontoggle;
	this->tg_userdata=userdata;
	this->checkboxes=true;
}

struct widget_listbox * widget_create_listbox_l(unsigned int numcolumns, const char * * columns)
{
	struct widget_listbox_win32 * this=malloc(sizeof(struct widget_listbox_win32));
	this->i._base.init=listbox__init;
	this->i._base.measure=listbox__measure;
	this->i._base.widthprio=3;
	this->i._base.heightprio=3;
	this->i._base.place=listbox__place;
	this->i._base.free=listbox__free;
	
	this->i.set_enabled=listbox_set_enabled;
	this->i.set_contents=listbox_set_contents;
	this->i.set_num_rows=listbox_set_num_rows;
	this->i.refresh=listbox_refresh;
	this->i.get_active_row=listbox_get_active_row;
	this->i.set_on_focus_change=listbox_set_on_focus_change;
	this->i.set_onactivate=listbox_set_onactivate;
	this->i.set_size=listbox_set_size;
	this->i.add_checkboxes=listbox_add_checkboxes;
	
	this->columnwidths=malloc(sizeof(unsigned int)*numcolumns);
	this->columns=numcolumns;
	this->onfocuschange=NULL;
	this->onactivate=NULL;
	this->checkboxes=NULL;
	
	this->i._base.width=1+19+1+this->columns*20;
	this->i._base.height=1+25+19*0+2;
	
	this->hwnd=malloc(sizeof(const char*)*numcolumns);
	memcpy(this->hwnd, columns, sizeof(const char*)*numcolumns);
	
	return (struct widget_listbox*)this;
}

static uintptr_t listbox_notify(NMHDR* nmhdr)
{
	struct widget_listbox_win32 * this=(struct widget_listbox_win32*)GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
	if (nmhdr->code==LVN_GETDISPINFO)
	{
		LV_DISPINFO* info=(LV_DISPINFO*)nmhdr;
		
		unsigned int row=info->item.iItem;
		unsigned int column=info->item.iSubItem;
		
		if (info->item.mask & LVIF_TEXT)
		{
			const char * str=this->get_cell((struct widget_listbox*)this, row, column, this->virt_userdata);
			unsigned int len=strlen(str);
			if (len > (unsigned int)info->item.cchTextMax-1) len=info->item.cchTextMax-1;
			memcpy(info->item.pszText, str, len);
			info->item.pszText[len]='\0';
		}
		
		if (info->item.mask & LVIF_IMAGE)
		{
			info->item.iImage=0;
			info->item.mask|=LVIF_STATE;
			info->item.stateMask=LVIS_STATEIMAGEMASK;
			info->item.state=INDEXTOSTATEIMAGEMASK(this->checkboxes ? this->get_cell((struct widget_listbox*)this, row, -1, this->virt_userdata) ? 2 : 1 : 0);
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
			size_t ret=this->search((struct widget_listbox*)this, find->lvfi.psz, find->iStart, false, this->virt_userdata);
			if (ret >= this->rows) ret=this->search((struct widget_listbox*)this, find->lvfi.psz, 0, false, this->virt_userdata);
			if (ret >= this->rows) ret=(size_t)-1;
			return (uintptr_t)ret;
		}
		else
		{
			return _widget_listbox_search((struct widget_listbox*)this, this->rows, this->get_cell,
			                              find->lvfi.psz, find->iStart, false, this->virt_userdata);
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
				if (this->ontoggle) this->ontoggle((struct widget_listbox*)this, row, this->tg_userdata);
				(void)ListView_RedrawItems(this->hwnd, row, row);
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
			if (this->ontoggle) this->ontoggle((struct widget_listbox*)this, row, this->tg_userdata);
			(void)ListView_RedrawItems(this->hwnd, row, row);
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
	if (nmhdr->code==LVN_ITEMCHANGED)
	{
		NMLISTVIEW* change=(NMLISTVIEW*)nmhdr;
		if (!(change->uOldState&LVIS_FOCUSED) && (change->uNewState&LVIS_FOCUSED) && change->iItem>=0)
		{
			if (this->onfocuschange) this->onfocuschange((struct widget_listbox*)this, change->iItem, this->foc_userdata);
		}
	}
	return 0;
}



struct widget_frame_win32 {
	//struct widget_frame i;
	
	//struct window * parent;
	HWND hwnd;
	
	struct widget_base * child;
	
	void (*onclick)(struct widget_frame * frame, void* userdata);
	void* userdata;
};

static unsigned int frame__init(struct widget_base * this_, struct window * parent, uintptr_t parenthandle)
{
	struct widget_frame_win32 * this=(struct widget_frame_win32*)this_;
	//this->parent=parent;//this one can not be resized
	this->hwnd=CreateWindow(WC_BUTTON, (char*)this->hwnd, WS_CHILD|WS_VISIBLE|WS_GROUP|WS_DISABLED|BS_GROUPBOX, 0, 0, 16, 16,
	                        (HWND)parenthandle, (HMENU)CTID_NONINTERACTIVE, GetModuleHandle(NULL), NULL);
	//SetWindowLongPtr(this->hwnd, GWLP_USERDATA, (LONG_PTR)this);//this one sends no notifications
	SendMessage(this->hwnd, WM_SETFONT, (WPARAM)dlgfont, FALSE);
	return 1+this->child->init(this->child, parent, parenthandle);
}

static void frame__measure(struct widget_base * this_)
{
	struct widget_frame_win32 * this=(struct widget_frame_win32*)this_;
	this->child->measure(this->child);
	this->i._base.width=this->child->width + frame_left+frame_right + g_padding*2;
	this->i._base.height=this->child->height + frame_top+frame_bottom + g_padding*2;
	this->i._base.widthprio=this->child->widthprio;
	this->i._base.heightprio=this->child->heightprio;
}

static void frame__place(struct widget_base * this_, void* resizeinf,
                         unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	x+=g_padding; y+=g_padding; width-=g_padding*2; height-=g_padding*2;
	struct widget_frame_win32 * this=(struct widget_frame_win32*)this_;
	this->child->place(this->child, resizeinf, x+frame_left, y+frame_top,
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
	this->i._base.init=frame__init;
	this->i._base.measure=frame__measure;
	this->i._base.place=frame__place;
	this->i._base.free=frame__free;
	
	this->i.set_text=frame_set_text;
	
	this->child=(struct widget_base*)contents;
	
	this->hwnd=(HWND)text;
	
	return (struct widget_frame*)this;
}
#endif



uintptr_t _window_notify_inner(void* notification)
{
	NMHDR* nmhdr=(NMHDR*)notification;
	switch (nmhdr->idFrom)
	{
		case CTID_NONINTERACTIVE: break;
		case CTID_BUTTON:
		case CTID_DEFBUTTON:
		{
			if (nmhdr->code==BN_CLICKED)
			{
				widget_button* obj=(widget_button*)GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
				if (obj->m->onclick)
				{
					obj->m->onclick(obj, obj->m->userdata);
				}
			}
			break;
		}
		case CTID_CHECK:
		{
			if (nmhdr->code==BN_CLICKED)
			{
				widget_checkbox * obj=(widget_checkbox*)GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
				bool state=obj->get_state();
				state=!state;
				obj->set_state(state);
				if (obj->m->onclick)
				{
					obj->m->onclick(obj, state, obj->m->userdata);
				}
			}
			break;
		}
		case CTID_RADIO:
		{
			if (nmhdr->code==BN_CLICKED)
			{
				widget_radio* obj=(widget_radio*)GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
				widget_radio* leader=obj->m->leader;
				leader->set_state(obj->m->id);
				if (leader->m->onclick)
				{
					leader->m->onclick(leader, obj->m->id, leader->m->userdata);
				}
			}
			break;
		}
		case CTID_TEXTBOX:
		{
			if (nmhdr->code==EN_CHANGE)
			{
				widget_textbox* obj=(widget_textbox*)GetWindowLongPtr(nmhdr->hwndFrom, GWLP_USERDATA);
				if (obj->m->invalid)
				{
					obj->m->invalid=false;
					InvalidateRect(obj->m->hwnd, NULL, FALSE);
				}
				if (obj->m->onchange)
				{
					obj->m->onchange(obj, obj->get_text(), obj->m->ch_userdata);
				}
			}
			break;
		}
/*
		case CTID_LISTVIEW:
		{
			return listbox_notify(nmhdr);
			break;
		}
*/
	}
	return 0;
}

uintptr_t _window_get_widget_color(unsigned int type, void* handle, void* draw, void* parent)
{
	switch (GetDlgCtrlID((HWND)handle))
	{
		case CTID_TEXTBOX:
		{
			widget_textbox* obj=(widget_textbox*)GetWindowLongPtr((HWND)handle, GWLP_USERDATA);
			if (obj->m->invalid)
			{
				SetBkMode((HDC)draw, TRANSPARENT);
				return (LRESULT)bg_invalid;
			}
			break;
		}
	}
	return DefWindowProcA((HWND)parent, (UINT)type, (WPARAM)draw, (LPARAM)handle);
}
#endif
