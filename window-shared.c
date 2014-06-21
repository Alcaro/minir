#include "minir.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef NEED_MANUAL_LAYOUT
//This is in practice only used on Windows, but it's theoretically usable on other operating systems too.
struct widget_padding_impl {
	struct widget_padding i;
};

static unsigned int padding__init(struct widget_base * this_, struct window * parent, uintptr_t parenthandle) { return 0; }
static void padding__measure(struct widget_base * this_) {}
static void padding__place(struct widget_base * this_, void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height) {}
static void padding__free(struct widget_base * this_) { free(this_); }

struct widget_padding * widget_create_padding_horz()
{
	struct widget_padding_impl * this=malloc(sizeof(struct widget_padding_impl));
	this->i.base._init=padding__init;
	this->i.base._measure=padding__measure;
	this->i.base._place=padding__place;
	this->i.base._free=padding__free;
	
	this->i.base._width=0;
	this->i.base._height=0;
	this->i.base._widthprio=2;
	this->i.base._heightprio=0;
	
	return (struct widget_padding*)this;
}

struct widget_padding * widget_create_padding_vert()
{
	struct widget_padding_impl * this=malloc(sizeof(struct widget_padding_impl));
	this->i.base._init=padding__init;
	this->i.base._measure=padding__measure;
	this->i.base._place=padding__place;
	this->i.base._free=padding__free;
	
	this->i.base._width=0;
	this->i.base._height=0;
	this->i.base._widthprio=0;
	this->i.base._heightprio=2;
	
	return (struct widget_padding*)this;
}



struct widget_layout_impl {
	struct widget_layout i;
	
	struct widget_base * * items;
	unsigned int numitems;
	unsigned char nummaxprio;
	bool vertical;
	bool uniform;
};

static unsigned int layout__init(struct widget_base * this_, struct window * parent, uintptr_t parenthandle)
{
	struct widget_layout_impl * this=(struct widget_layout_impl*)this_;
	unsigned int ret=0;
	for (unsigned int i=0;i<this->numitems;i++)
	{
		ret+=this->items[i]->_init(this->items[i], parent, parenthandle);
	}
	return ret;
}

static void layout__measure(struct widget_base * this_)
{
	struct widget_layout_impl * this=(struct widget_layout_impl*)this_;
	if (!this->uniform)
	{
		if (this->vertical)
		{
			unsigned int width=0;
			unsigned int height=0;
			unsigned char maxwidthprio=0;
			unsigned char maxheightprio=0;
			unsigned char nummaxheight=0;
			for (unsigned int i=0;i<this->numitems;i++)
			{
				this->items[i]->_measure(this->items[i]);
				if (this->items[i]->_width > width) width=this->items[i]->_width;
				height+=this->items[i]->_height;
				if (this->items[i]->_widthprio > maxwidthprio) maxwidthprio = this->items[i]->_widthprio;
				if (this->items[i]->_heightprio > maxheightprio) { maxheightprio = this->items[i]->_heightprio; nummaxheight=0; }
				if (this->items[i]->_heightprio == maxheightprio) nummaxheight++;
			}
			this->i.base._width=width;
			this->i.base._height=height;
			this->i.base._widthprio=maxwidthprio;
			this->i.base._heightprio=maxheightprio;
			this->nummaxprio=nummaxheight;
		}
		else
		{
			unsigned int width=0;
			unsigned int height=0;
			unsigned char maxwidthprio=0;
			unsigned char maxheightprio=0;
			unsigned char nummaxwidth=0;
			for (unsigned int i=0;i<this->numitems;i++)
			{
				this->items[i]->_measure(this->items[i]);
				width+=this->items[i]->_width;
				if (this->items[i]->_height > height) height=this->items[i]->_height;
				if (this->items[i]->_widthprio > maxwidthprio) { maxwidthprio = this->items[i]->_widthprio; nummaxwidth=0; }
				if (this->items[i]->_heightprio > maxheightprio) maxheightprio = this->items[i]->_heightprio;
				if (this->items[i]->_widthprio == maxwidthprio) nummaxwidth++;
			}
			this->i.base._width=width;
			this->i.base._height=height;
			this->i.base._widthprio=maxwidthprio;
			this->i.base._heightprio=maxheightprio;
			this->nummaxprio=nummaxwidth;
		}
	}
	else
	{
		unsigned int maxwidth=0;
		unsigned int maxheight=0;
		for (unsigned int i=0;i<this->numitems;i++)
		{
			this->items[i]->_measure(this->items[i]);
			if (this->items[i]->_width > maxwidth) maxwidth=this->items[i]->_width;
			if (this->items[i]->_height > maxheight) maxheight=this->items[i]->_height;
		}
		this->i.base._width=maxwidth * (this->vertical ? 1 : this->numitems);
		this->i.base._height=maxheight * (this->vertical ? this->numitems : 1);
		this->i.base._widthprio=0;
		this->i.base._heightprio=0;
	}
}

static void layout__place(struct widget_base * this_, void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	struct widget_layout_impl * this=(struct widget_layout_impl*)this_;
	if (!this->uniform)
	{
		if (this->vertical)
		{
			unsigned int spareheight=height-this->i.base._height;
			unsigned char spareheight_div=this->nummaxprio;
			if (!spareheight_div) y+=spareheight/2;
			unsigned int spareheight_frac=0;
			for (unsigned int i=0;i<this->numitems;i++)
			{
				if (this->items[i]->_heightprio == this->i.base._heightprio)
				{
					spareheight_frac+=spareheight;
					unsigned int yourheight=spareheight_frac/spareheight_div + this->items[i]->_height;
					spareheight_frac%=spareheight_div;
					this->items[i]->_place(this->items[i], resizeinf, x, y, width, yourheight);
					y+=yourheight;
				}
				else
				{
					this->items[i]->_place(this->items[i], resizeinf, x, y, width, this->items[i]->_height);
					y+=this->items[i]->_height;
				}
			}
		}
		else
		{
			unsigned int sparewidth=width-this->i.base._width;
			unsigned char sparewidth_div=this->nummaxprio;
			if (!sparewidth_div) x+=sparewidth/2;
			unsigned int sparewidth_frac=0;
			for (unsigned int i=0;i<this->numitems;i++)
			{
				if (this->items[i]->_widthprio == this->i.base._widthprio)
				{
					sparewidth_frac+=sparewidth;
					unsigned int yourwidth=sparewidth_frac/sparewidth_div + this->items[i]->_width;
					sparewidth_frac%=sparewidth_div;
					this->items[i]->_place(this->items[i], resizeinf, x, y, yourwidth, height);
					x+=yourwidth;
				}
				else
				{
					this->items[i]->_place(this->items[i], resizeinf, x, y, this->items[i]->_width, height);
					x+=this->items[i]->_width;
				}
			}
		}
	}
	else
	{
		unsigned int itemwidth=this->i.base._width/(this->vertical ? 1 : this->numitems);
		unsigned int itemheight=this->i.base._height/(this->vertical ? this->numitems : 1);
		for (unsigned int i=0;i<this->numitems;i++)
		{
			this->items[i]->_place(this->items[i], resizeinf, x, y, itemwidth, itemheight);
			if (this->vertical) y+=itemheight;
			else x+=itemwidth;
		}
	}
}

static void layout__free(struct widget_base * this_)
{
	struct widget_layout_impl * this=(struct widget_layout_impl*)this_;
	for (unsigned int i=0;i<this->numitems;i++)
	{
		this->items[i]->_free(this->items[i]);
	}
	free(this_);
}

struct widget_layout * widget_create_layout_l(bool vertical, bool uniform, unsigned int numchildren, void * * children)
{
	if (!numchildren)
	{
		while (children[numchildren]) numchildren++;
	}
	
	struct widget_layout_impl * this=malloc(sizeof(struct widget_layout_impl));
	this->i.base._init=layout__init;
	this->i.base._measure=layout__measure;
	this->i.base._place=layout__place;
	this->i.base._free=layout__free;
	
	this->vertical=vertical;
	this->uniform=uniform;
	
	this->numitems=numchildren;
	this->items=malloc(sizeof(struct widget_base*)*numchildren);
	memcpy(this->items, children, sizeof(struct widget_base*)*numchildren);
	
	return (struct widget_layout*)this;
}
#endif


size_t _widget_listbox_search(struct widget_listbox * subject, size_t rows,
                              const char * (*get_cell)(struct widget_listbox * subject, size_t row, int column, void * userdata),
                              const char * prefix, size_t start, bool up, void * userdata)
{
	size_t len=strlen(prefix);
	if (!up)
	{
		for (size_t i=start;i<rows;i++)
		{
			const char * thisstr=get_cell(subject, i, 0, userdata);
			if (!strncasecmp(thisstr, prefix, len)) return i;
		}
		for (size_t i=0;i<start;i++)
		{
			const char * thisstr=get_cell(subject, i, 0, userdata);
			if (!strncasecmp(thisstr, prefix, len)) return i;
		}
	}
	else
	{
		for (size_t i=start;i>=0;i--)
		{
			const char * thisstr=get_cell(subject, i, 0, userdata);
			if (!strncasecmp(thisstr, prefix, len)) return i;
		}
		for (size_t i=rows-1;i>start;i--)
		{
			const char * thisstr=get_cell(subject, i, 0, userdata);
			if (!strncasecmp(thisstr, prefix, len)) return i;
		}
	}
	return (size_t)-1;
}


//Varargs are irritating; no point reimplementing them for all platforms.
struct windowmenu * windowmenu_create_radio(void (*onactivate)(struct windowmenu * subject, unsigned int state, void* userdata),
                                            void* userdata, const char * firsttext, ...)
{
	unsigned int numitems=1;
	
	va_list args;
	va_start(args, firsttext);
	while (va_arg(args, const char*)) numitems++;
	va_end(args);
	
	const char * items[numitems];
	items[0]=firsttext;
	va_start(args, firsttext);
	for (unsigned int i=1;i<numitems;i++)
	{
		items[i]=va_arg(args, const char*);
	}
	va_end(args);
	
	return windowmenu_create_radio_l(numitems, items, onactivate, userdata);
}

struct windowmenu * windowmenu_create_topmenu(struct windowmenu * firstchild, ...)
{
	unsigned int numitems=1;
	
	va_list args;
	if (firstchild)
	{
		va_start(args, firstchild);
		while (va_arg(args, struct windowmenu*)) numitems++;
		va_end(args);
	}
	else numitems=0;
	
	struct windowmenu * items[numitems];
	items[0]=firstchild;
	va_start(args, firstchild);
	for (unsigned int i=1;i<numitems;i++)
	{
		items[i]=va_arg(args, struct windowmenu*);
	}
	va_end(args);
	
	return windowmenu_create_topmenu_l(numitems, items);
}

struct windowmenu * windowmenu_create_submenu(const char * text, struct windowmenu * firstchild, ...)
{
	unsigned int numitems=1;
	
	va_list args;
	if (firstchild)
	{
		va_start(args, firstchild);
		while (va_arg(args, struct windowmenu*)) numitems++;
		va_end(args);
	}
	else numitems=0;
	
	struct windowmenu * items[numitems];
	items[0]=firstchild;
	va_start(args, firstchild);
	for (unsigned int i=1;i<numitems;i++)
	{
		items[i]=va_arg(args, struct windowmenu*);
	}
	va_end(args);
	
	return windowmenu_create_submenu_l(text, numitems, items);
}

struct widget_layout * widget_create_radio_group(struct widget_radio * * leader, bool vertical, const char * firsttext, ...)
{
	unsigned int numitems=1;
	
	va_list args;
	va_start(args, firsttext);
	while (va_arg(args, const char*)) numitems++;
	va_end(args);
	
	struct widget_radio * items[numitems];
	items[0]=widget_create_radio(firsttext);
	va_start(args, firsttext);
	for (unsigned int i=1;i<numitems;i++)
	{
		items[i]=widget_create_radio(va_arg(args, const char*));
	}
	va_end(args);
	
	items[0]->group(items[0], numitems, items);
	*leader=items[0];
	
	return widget_create_layout_l(vertical, false, numitems, (void**)items);
}

struct widget_listbox * widget_create_listbox(const char * firstcol, ...)
{
	unsigned int numcols=1;
	
	va_list args;
	va_start(args, firstcol);
	while (va_arg(args, const char*)) numcols++;
	va_end(args);
	
	const char * columns[numcols];
	columns[0]=firstcol;
	va_start(args, firstcol);
	for (unsigned int i=1;i<numcols;i++)
	{
		columns[i]=va_arg(args, const char*);
	}
	va_end(args);
	
	return widget_create_listbox_l(numcols, columns);
}

struct widget_layout * widget_create_layout(bool vertical, bool uniform, void * firstchild, ...)
{
	unsigned int numchildren=1;
	
	va_list args;
	va_start(args, firstchild);
	while (va_arg(args, void*)) numchildren++;
	va_end(args);
	
	void* children[numchildren];
	children[0]=firstchild;
	va_start(args, firstchild);
	for (unsigned int i=1;i<numchildren;i++)
	{
		children[i]=va_arg(args, void*);
	}
	va_end(args);
	
	return widget_create_layout_l(vertical, uniform, numchildren, children);
}
