#include "minir.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include<stdio.h>

#ifdef NEED_MANUAL_LAYOUT
//This is in practice only used on Windows, but it's theoretically usable on other operating systems too. Maybe I'll need it on OSX.
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
	this->i._base.init=padding__init;
	this->i._base.measure=padding__measure;
	this->i._base.place=padding__place;
	this->i._base.free=padding__free;
	
	this->i._base.width=0;
	this->i._base.height=0;
	this->i._base.widthprio=2;
	this->i._base.heightprio=0;
	
	return (struct widget_padding*)this;
}

struct widget_padding * widget_create_padding_vert()
{
	struct widget_padding_impl * this=malloc(sizeof(struct widget_padding_impl));
	this->i._base.init=padding__init;
	this->i._base.measure=padding__measure;
	this->i._base.place=padding__place;
	this->i._base.free=padding__free;
	
	this->i._base.width=0;
	this->i._base.height=0;
	this->i._base.widthprio=0;
	this->i._base.heightprio=2;
	
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
		ret+=this->items[i]->init(this->items[i], parent, parenthandle);
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
				this->items[i]->measure(this->items[i]);
				if (this->items[i]->width > width) width=this->items[i]->width;
				height+=this->items[i]->height;
				if (this->items[i]->widthprio > maxwidthprio) maxwidthprio = this->items[i]->widthprio;
				if (this->items[i]->heightprio > maxheightprio) { maxheightprio = this->items[i]->heightprio; nummaxheight=0; }
				if (this->items[i]->heightprio == maxheightprio) nummaxheight++;
			}
			this->i._base.width=width;
			this->i._base.height=height;
			this->i._base.widthprio=maxwidthprio;
			this->i._base.heightprio=maxheightprio;
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
				this->items[i]->measure(this->items[i]);
				width+=this->items[i]->width;
				if (this->items[i]->height > height) height=this->items[i]->height;
				if (this->items[i]->widthprio > maxwidthprio) { maxwidthprio = this->items[i]->widthprio; nummaxwidth=0; }
				if (this->items[i]->heightprio > maxheightprio) maxheightprio = this->items[i]->heightprio;
				if (this->items[i]->widthprio == maxwidthprio) nummaxwidth++;
			}
			this->i._base.width=width;
			this->i._base.height=height;
			this->i._base.widthprio=maxwidthprio;
			this->i._base.heightprio=maxheightprio;
			this->nummaxprio=nummaxwidth;
		}
	}
	else
	{
		unsigned int maxwidth=0;
		unsigned int maxheight=0;
		for (unsigned int i=0;i<this->numitems;i++)
		{
			this->items[i]->measure(this->items[i]);
			if (this->items[i]->width > maxwidth) maxwidth=this->items[i]->width;
			if (this->items[i]->height > maxheight) maxheight=this->items[i]->height;
		}
		this->i._base.width=maxwidth * (this->vertical ? 1 : this->numitems);
		this->i._base.height=maxheight * (this->vertical ? this->numitems : 1);
		this->i._base.widthprio=0;
		this->i._base.heightprio=0;
	}
}

static void layout__place(struct widget_base * this_, void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	struct widget_layout_impl * this=(struct widget_layout_impl*)this_;
	if (!this->uniform)
	{
		if (this->vertical)
		{
			unsigned int spareheight=height-this->i._base.height;
			unsigned char spareheight_div=this->nummaxprio;
			if (!spareheight_div) y+=spareheight/2;
			unsigned int spareheight_frac=0;
			for (unsigned int i=0;i<this->numitems;i++)
			{
				if (this->items[i]->heightprio == this->i._base.heightprio)
				{
					spareheight_frac+=spareheight;
					unsigned int yourheight=spareheight_frac/spareheight_div + this->items[i]->height;
					spareheight_frac%=spareheight_div;
					this->items[i]->place(this->items[i], resizeinf, x, y, width, yourheight);
					y+=yourheight;
				}
				else
				{
					this->items[i]->place(this->items[i], resizeinf, x, y, width, this->items[i]->height);
					y+=this->items[i]->height;
				}
			}
		}
		else
		{
			unsigned int sparewidth=width-this->i._base.width;
			unsigned char sparewidth_div=this->nummaxprio;
			if (!sparewidth_div) x+=sparewidth/2;
			unsigned int sparewidth_frac=0;
			for (unsigned int i=0;i<this->numitems;i++)
			{
				if (this->items[i]->widthprio == this->i._base.widthprio)
				{
					sparewidth_frac+=sparewidth;
					unsigned int yourwidth=sparewidth_frac/sparewidth_div + this->items[i]->width;
					sparewidth_frac%=sparewidth_div;
					this->items[i]->place(this->items[i], resizeinf, x, y, yourwidth, height);
					x+=yourwidth;
				}
				else
				{
					this->items[i]->place(this->items[i], resizeinf, x, y, this->items[i]->width, height);
					x+=this->items[i]->width;
				}
			}
		}
	}
	else
	{
		unsigned int itemwidth=this->i._base.width/(this->vertical ? 1 : this->numitems);
		unsigned int itemheight=this->i._base.height/(this->vertical ? this->numitems : 1);
		for (unsigned int i=0;i<this->numitems;i++)
		{
			this->items[i]->place(this->items[i], resizeinf, x, y, itemwidth, itemheight);
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
		this->items[i]->free(this->items[i]);
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
	this->i._base.init=layout__init;
	this->i._base.measure=layout__measure;
	this->i._base.place=layout__place;
	this->i._base.free=layout__free;
	
	this->vertical=vertical;
	this->uniform=uniform;
	
	this->numitems=numchildren;
	this->items=malloc(sizeof(struct widget_base*)*numchildren);
	memcpy(this->items, children, sizeof(struct widget_base*)*numchildren);
	
	return (struct widget_layout*)this;
}



struct widget_grid_impl {
	struct widget_grid i;
	
	unsigned int numchildren;
	bool uniform[2];
	//char padding[2];
	
	unsigned int totsize[2];
	
	struct widget_base * * children;
	unsigned int * startpos[2];
	unsigned int * extent[2];
};

static unsigned int grid__init(struct widget_base * this_, struct window * parent, uintptr_t parenthandle)
{
	struct widget_grid_impl * this=(struct widget_grid_impl*)this_;
	unsigned int ret=0;
	for (unsigned int i=0;i<this->numchildren;i++)
	{
		ret+=this->children[i]->init(this->children[i], parent, parenthandle);
	}
	return ret;
}

static void grid_calc_size(struct widget_grid_impl * this, unsigned int * widths, unsigned int * heights)
{
//TODO: This does not grant the desired size to elements covering more than one tile. Fix it once it's known how GTK+ or Qt operates.
	memset(widths,  0, sizeof(unsigned int)*this->totsize[0]);
	memset(heights, 0, sizeof(unsigned int)*this->totsize[1]);
	for (unsigned int i=0;i<this->numchildren;i++)
	{
		if (this->extent[0][i]==1 && this->children[i]->width  >  widths[this->startpos[0][i]])  widths[this->startpos[0][i]]=this->children[i]->width;
		if (this->extent[1][i]==1 && this->children[i]->height > heights[this->startpos[1][i]]) heights[this->startpos[1][i]]=this->children[i]->height;
	}
//for(int i=0;i<this->totsize[0];i++)widths[i]=70;
//for(int i=0;i<this->totsize[1];i++)heights[i]=10+i*10;
}

static void grid__measure(struct widget_base * this_)
{
	struct widget_grid_impl * this=(struct widget_grid_impl*)this_;
	for (unsigned int i=0;i<this->numchildren;i++)
	{
		this->children[i]->measure(this->children[i]);
	}
	unsigned int cellwidths[this->totsize[0]];
	unsigned int cellheights[this->totsize[1]];
	grid_calc_size(this, cellwidths, cellheights);
	unsigned int width=0;
	for (unsigned int i=0;i<this->totsize[0];i++) width+=cellwidths[i];
	this->i._base.width=width;
	unsigned int height=0;
	for (unsigned int i=0;i<this->totsize[1];i++) height+=cellheights[i];
	this->i._base.height=height;
	
	this->i._base.widthprio=0;
	this->i._base.heightprio=0;
	for (unsigned int i=0;i<this->numchildren;i++)
	{
		if (this->children[i]->widthprio  > this->i._base.widthprio)  this->i._base.widthprio =this->children[i]->widthprio;
		if (this->children[i]->heightprio > this->i._base.heightprio) this->i._base.heightprio=this->children[i]->heightprio;
	}
}

static void grid__place(struct widget_base * this_, void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	struct widget_grid_impl * this=(struct widget_grid_impl*)this_;
	
	unsigned int cellwidths[this->totsize[0]];
	unsigned int cellheights[this->totsize[1]];
	grid_calc_size(this, cellwidths, cellheights);
	
	//TODO: Use extra space given
	//unsigned int extrawidth_pix = width  - this->i._base.width;
	//unsigned int extrawidth_frac=0;
	//unsigned int extrawidth_split=0;
	//for (unsigned int i=0;i<this->totsize[0];i++)
	//{
	//	if (this->children[i]->widthprio==this->i._base.widthprio) extrawidth_split++;
	//}
	//for (unsigned int i=0;i<this->totsize[0];i++)
	//{
	//	if (this->children[i]->widthprio==this->i._base.widthprio) extrawidth_split++;
	//}
	
	unsigned int cellstartx[this->totsize[0]+1];
	cellstartx[0]=0;
	for (unsigned int i=0;i<this->totsize[0];i++) cellstartx[i+1]=cellstartx[i]+cellwidths[i];
	unsigned int cellstarty[this->totsize[1]+1];
	cellstarty[0]=0;
	for (unsigned int i=0;i<this->totsize[1];i++) cellstarty[i+1]=cellstarty[i]+cellheights[i];
	
	for (unsigned int i=0;i<this->numchildren;i++)
	{
//printf("pl %u at %u,%u %u,%u\n",i,
//x+cellstartx[this->startpos[0][i]], y+cellstarty[this->startpos[1][i]],
//cellstartx[this->startpos[0][i]+this->extent[0][i]]-cellstartx[this->startpos[0][i]],
//cellstarty[this->startpos[1][i]+this->extent[1][i]]-cellstarty[this->startpos[1][i]]);
		this->children[i]->place(this->children[i], resizeinf,
		                         x+cellstartx[this->startpos[0][i]], y+cellstarty[this->startpos[1][i]],
		                         cellstartx[this->startpos[0][i]+this->extent[0][i]]-cellstartx[this->startpos[0][i]],
		                         cellstarty[this->startpos[1][i]+this->extent[1][i]]-cellstarty[this->startpos[1][i]]);
	}
}

static void grid__free(struct widget_base * this_)
{
	struct widget_grid_impl * this=(struct widget_grid_impl*)this_;
	free(this->children);
	free(this->extent[0]);
	free(this->extent[1]);
	free(this->startpos[0]);
	free(this->startpos[1]);
	free(this);
}

struct widget_grid * widget_create_grid_l(unsigned int numchildren, void * * children,
                                          unsigned int totwidth,  unsigned int * widths,  bool uniformwidths,
                                          unsigned int totheight, unsigned int * heights, bool uniformheights)
{
	struct widget_grid_impl * this=malloc(sizeof(struct widget_grid_impl));
	this->i._base.init=grid__init;
	this->i._base.measure=grid__measure;
	this->i._base.place=grid__place;
	this->i._base.free=grid__free;
	
	this->numchildren=numchildren;
	this->uniform[0]=uniformwidths;
	this->uniform[1]=uniformheights;
	this->totsize[0]=totwidth;
	this->totsize[1]=totheight;
	
	this->children=malloc(sizeof(struct widget_base*)*numchildren);
	memcpy(this->children, children, sizeof(struct widget_base*)*numchildren);
	
	this->extent[0]=malloc(sizeof(unsigned int)*numchildren);
	if (widths)
	{
		memcpy(this->extent[0], widths, sizeof(unsigned int)*numchildren);
	}
	else
	{
		for (unsigned int i=0;i<numchildren;i++) this->extent[0][i]=1;
	}
	
	this->extent[1]=malloc(sizeof(unsigned int)*numchildren);
	if (heights)
	{
		memcpy(this->extent[1], heights, sizeof(unsigned int)*numchildren);
	}
	else
	{
		for (unsigned int i=0;i<numchildren;i++) this->extent[1][i]=1;
	}
	
	this->startpos[0]=malloc(sizeof(unsigned int)*numchildren);
	this->startpos[1]=malloc(sizeof(unsigned int)*numchildren);
	bool posused[totheight*totwidth];
	memset(posused, 0, sizeof(posused));
	unsigned int firstempty=0;
	for (int i=0;i<numchildren;i++)
	{
		while (posused[firstempty]) firstempty++;
		this->startpos[0][i]=firstempty%this->totsize[0];
		this->startpos[1][i]=firstempty/this->totsize[0];
		for (int x=0;x<this->extent[0][i];x++)
		for (int y=0;y<this->extent[1][i];y++)
		{
			posused[firstempty + y*this->totsize[0] + x]=true;
		}
	}
	
	return (struct widget_grid*)this;
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




//varargs are irritating; no point reimplementing them for all platforms.
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

struct widget_grid * widget_create_grid(unsigned int width, unsigned int height, bool uniformsizes,
                                        void * firstchild, ...)
{
	va_list args;
	void* children[width*height];
	children[0]=firstchild;
	va_start(args, firstchild);
	for (unsigned int i=1;i<width*height;i++)
	{
		children[i]=va_arg(args, void*);
	}
	va_end(args);
	
	return widget_create_grid_l(width*height, children,  width, NULL, uniformsizes,  height, NULL, uniformsizes);
}

struct widget_grid * widget_create_grid_v(unsigned int totwidth,   unsigned int totheight,   bool uniformwidths, bool uniformheights,
                                          unsigned int firstwidth, unsigned int firstheight, void * firstchild, ...)
{
	unsigned int numchildren=1;
	unsigned int boxesleft=totwidth*totheight;
	
	boxesleft-=firstwidth*firstheight;
	
	va_list args;
	va_start(args, firstchild);
	while (boxesleft)
	{
		boxesleft-=va_arg(args, unsigned int)*va_arg(args, unsigned int);
		va_arg(args, void*);
		numchildren++;
	}
	va_end(args);
	
	unsigned int widths[numchildren];
	unsigned int heights[numchildren];
	void* children[numchildren];
	widths[0]=firstwidth;
	heights[0]=firstheight;
	children[0]=firstchild;
	va_start(args, firstchild);
	for (unsigned int i=1;i<numchildren;i++)
	{
		widths[i]=va_arg(args, unsigned int);
		heights[i]=va_arg(args, unsigned int);
		children[i]=va_arg(args, void*);
	}
	va_end(args);
	
	return widget_create_grid_l(numchildren, children,  totwidth, widths, uniformwidths,  totheight, heights, uniformheights);
}
