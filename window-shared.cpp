#include "minir.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

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
	struct widget_layout_impl * this=(struct widget_layout_impl*)this_;
	unsigned int ret=0;
	for (unsigned int i=0;i<this->numchildren;i++)
	{
		ret+=this->children[i]->init(this->children[i], parent, parenthandle);
	}
	return ret;
}

static void grid_calc_size(struct widget_layout_impl * this, unsigned int * widths, unsigned int * heights)
{
	for (unsigned int dir=0;dir<2;dir++)
	{
		unsigned int * sizes=(dir==0 ? widths : heights);
		if (this->uniform[dir])
		{
			unsigned int maxsize=0;
			for (unsigned int i=0;i<this->numchildren;i++)
			{
				unsigned int thissizepx=(dir==0 ? this->children[i]->width : this->children[i]->height);
				unsigned int thissize=((thissizepx+this->extent[dir][i]-1)/this->extent[dir][i]);
				if (thissize>maxsize) maxsize=thissize;
			}
			for (unsigned int i=0;i<this->totsize[dir];i++) sizes[i]=maxsize;
		}
		else
		{
			memset(sizes, 0, sizeof(unsigned int)*this->totsize[dir]);
			for (unsigned int i=0;i<this->numchildren;i++)
			{
				unsigned int thissize=(dir==0 ? this->children[i]->width : this->children[i]->height);
				if (this->extent[dir][i]==1 && thissize > sizes[this->startpos[dir][i]]) sizes[this->startpos[dir][i]]=thissize;
			}
			//TODO: This does not grant the desired size to elements covering more than one tile.
			//GTK+ does something highly weird, so it'll need to be tested everywhere anyways. I can do whatever I want.
		}
	}
}

static void grid__measure(struct widget_base * this_)
{
	struct widget_layout_impl * this=(struct widget_layout_impl*)this_;
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
	struct widget_layout_impl * this=(struct widget_layout_impl*)this_;
	
	unsigned int cellwidths[this->totsize[0]];
	unsigned int cellheights[this->totsize[1]];
	grid_calc_size(this, cellwidths, cellheights);
	
	for (int dir=0;dir<2;dir++)
	{
		uint32_t expand[this->totsize[dir]];
		memset(expand, 0, sizeof(uint32_t)*this->totsize[dir]);
		unsigned int extrasize_pix;
		if (dir==0) extrasize_pix = width  - this->i._base.width;
		else        extrasize_pix = height - this->i._base.height;
		unsigned int extrasize_frac=0;
		unsigned int extrasize_split=0;
		unsigned int extrasize_max=0;
		for (unsigned int i=0;i<this->numchildren;i++)
		{
			if ((dir==0 && this->children[i]->widthprio ==this->i._base.widthprio) ||
			    (dir==1 && this->children[i]->heightprio==this->i._base.heightprio))
			{
				for (unsigned int j=0;j<this->extent[dir][i];j++)
				{
					expand[this->startpos[dir][i]+j]++;
					if (expand[this->startpos[dir][i]+j] == extrasize_max) extrasize_split++;
					if (expand[this->startpos[dir][i]+j] >  extrasize_max)
					{
						extrasize_split=1;
						extrasize_max=expand[this->startpos[dir][i]+j];
					}
				}
			}
		}
		for (unsigned int i=0;i<this->totsize[dir];i++)
		{
			if (expand[i]==extrasize_max)
			{
				extrasize_frac+=extrasize_pix;
				if (dir==0) cellwidths[i]+=extrasize_frac/extrasize_split;
				else       cellheights[i]+=extrasize_frac/extrasize_split;
				extrasize_frac%=extrasize_split;
			}
		}
	}
	
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
	struct widget_layout_impl * this=(struct widget_layout_impl*)this_;
	free(this->children);
	free(this->extent[0]);
	free(this->extent[1]);
	free(this->startpos[0]);
	free(this->startpos[1]);
	free(this);
}

struct widget_layout * widget_create_layout_l(unsigned int numchildren, void * * children,
                                              unsigned int totwidth,  unsigned int * widths,  bool uniformwidths,
                                              unsigned int totheight, unsigned int * heights, bool uniformheights)
{
	struct widget_layout_impl * this=malloc(sizeof(struct widget_layout_impl));
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
	
	for (unsigned int dir=0;dir<2;dir++)
	{
		this->extent[dir]=malloc(sizeof(unsigned int)*numchildren);
		unsigned int * sizes=(dir==0 ? widths : heights);
		if (sizes)
		{
			memcpy(this->extent[dir], sizes, sizeof(unsigned int)*numchildren);
		}
		else
		{
			for (unsigned int i=0;i<numchildren;i++) this->extent[dir][i]=1;
		}
	}
	
	this->startpos[0]=malloc(sizeof(unsigned int)*numchildren);
	this->startpos[1]=malloc(sizeof(unsigned int)*numchildren);
	bool posused[totheight*totwidth];
	memset(posused, 0, sizeof(posused));
	unsigned int firstempty=0;
	for (unsigned int i=0;i<numchildren;i++)
	{
		while (posused[firstempty]) firstempty++;
		this->startpos[0][i]=firstempty%this->totsize[0];
		this->startpos[1][i]=firstempty/this->totsize[0];
		for (unsigned int x=0;x<this->extent[0][i];x++)
		for (unsigned int y=0;y<this->extent[1][i];y++)
		{
			posused[firstempty + y*this->totsize[0] + x]=true;
		}
	}
	
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

widget_radio_group::widget_radio_group(bool vertical, widget_radio* leader, ...)
{
	unsigned int numitems=1;
	
	va_list args;
	va_start(args, leader);
	while (va_arg(args, widget_radio*)) numitems++;
	va_end(args);
	
	widget_radio* items[numitems];
	items[0]=leader;
	va_start(args, leader);
	for (unsigned int i=1;i<numitems;i++)
	{
		items[i]=va_arg(args, widget_radio*);
	}
	va_end(args);
	
	items[0]->group(numitems, items);
	
	construct(numitems, (widget_base**)items, vertical?1:numitems, NULL, false, vertical?numitems:1, NULL, false);
}

/*
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
*/

widget_layout::widget_layout(bool vertical, bool uniform, widget_base * firstchild, ...)
{
	unsigned int numchildren=1;
	
	va_list args;
	va_start(args, firstchild);
	while (va_arg(args, void*)) numchildren++;
	va_end(args);
	
	widget_base* children[numchildren];
	children[0]=firstchild;
	va_start(args, firstchild);
	for (unsigned int i=1;i<numchildren;i++)
	{
		children[i]=va_arg(args, widget_base*);
	}
	va_end(args);
	
	construct(numchildren, (widget_base**)children, vertical?1:numchildren, NULL, uniform, vertical?numchildren:1, NULL, uniform);
}

widget_layout::widget_layout(unsigned int totwidth,   unsigned int totheight,   bool uniformwidths, bool uniformheights,
                             unsigned int firstwidth, unsigned int firstheight, widget_base * firstchild, ...)
{
	unsigned int numchildren=1;
	unsigned int boxesleft=totwidth*totheight;
	
	boxesleft-=firstwidth*firstheight;
	
	va_list args;
	va_start(args, firstchild);
	while (boxesleft)
	{
		boxesleft-=va_arg(args, unsigned int)*va_arg(args, unsigned int);
		va_arg(args, widget_base*);
		numchildren++;
	}
	va_end(args);
	
	unsigned int widths[numchildren];
	unsigned int heights[numchildren];
	widget_base* children[numchildren];
	widths[0]=firstwidth;
	heights[0]=firstheight;
	children[0]=firstchild;
	va_start(args, firstchild);
	for (unsigned int i=1;i<numchildren;i++)
	{
		widths[i]=va_arg(args, unsigned int);
		heights[i]=va_arg(args, unsigned int);
		children[i]=va_arg(args, widget_base*);
	}
	va_end(args);
	
	construct(numchildren, children,  totwidth, widths, uniformwidths,  totheight, heights, uniformheights);
}

widget_layout_grid::widget_layout_grid(unsigned int width, unsigned int height, bool uniformsizes,
                                       widget_base * firstchild, ...)
{
	va_list args;
	widget_base* children[width*height];
	children[0]=firstchild;
	va_start(args, firstchild);
	for (unsigned int i=1;i<width*height;i++)
	{
		children[i]=va_arg(args, widget_base*);
	}
	va_end(args);
	
	construct(width*height, children,  width, NULL, uniformsizes,  height, NULL, uniformsizes);
}
