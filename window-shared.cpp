#include "minir.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#undef this

#ifdef NEED_MANUAL_LAYOUT
//This is in practice only used on Windows, but it's theoretically usable on other operating systems too. Maybe I'll need it on OSX.
widget_padding::widget_padding(bool vertical)
{
	this->width=0;
	this->height=0;
	this->widthprio=(vertical ? 0 : 2);
	this->heightprio=(vertical ? 2 : 0);
}

unsigned int widget_padding::init(struct window * parent, uintptr_t parenthandle) { return 0; }
void widget_padding::measure() {}
void widget_padding::place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height) {}

widget_padding::~widget_padding() {}



struct widget_layout::impl {
	unsigned int numchildren;
	bool uniform[2];
	//char padding[2];
	
	unsigned int totsize[2];
	
	widget_base* * children;
	unsigned int * startpos[2];
	unsigned int * extent[2];
};

void widget_layout::construct(unsigned int numchildren, widget_base* * children,
                              unsigned int totwidth,  unsigned int * widths,  bool uniformwidths,
                              unsigned int totheight, unsigned int * heights, bool uniformheights)
{
	m=new impl;
	
	m->numchildren=numchildren;
	m->uniform[0]=uniformwidths;
	m->uniform[1]=uniformheights;
	m->totsize[0]=totwidth;
	m->totsize[1]=totheight;
	
	m->children=malloc(sizeof(struct widget_base*)*numchildren);
	memcpy(m->children, children, sizeof(struct widget_base*)*numchildren);
	
	for (unsigned int dir=0;dir<2;dir++)
	{
		m->extent[dir]=malloc(sizeof(unsigned int)*numchildren);
		unsigned int * sizes=(dir==0 ? widths : heights);
		if (sizes)
		{
			memcpy(m->extent[dir], sizes, sizeof(unsigned int)*numchildren);
		}
		else
		{
			for (unsigned int i=0;i<numchildren;i++) m->extent[dir][i]=1;
		}
	}
	
	m->startpos[0]=malloc(sizeof(unsigned int)*numchildren);
	m->startpos[1]=malloc(sizeof(unsigned int)*numchildren);
	bool posused[totheight*totwidth];
	memset(posused, 0, sizeof(posused));
	unsigned int firstempty=0;
	for (unsigned int i=0;i<numchildren;i++)
	{
		while (posused[firstempty]) firstempty++;
		m->startpos[0][i]=firstempty%m->totsize[0];
		m->startpos[1][i]=firstempty/m->totsize[0];
		for (unsigned int x=0;x<m->extent[0][i];x++)
		for (unsigned int y=0;y<m->extent[1][i];y++)
		{
			posused[firstempty + y*m->totsize[0] + x]=true;
		}
	}
}

widget_layout::~widget_layout()
{
	free(m->children);
	free(m->extent[0]);
	free(m->extent[1]);
	free(m->startpos[0]);
	free(m->startpos[1]);
	delete m;
}

unsigned int widget_layout::init(struct window * parent, uintptr_t parenthandle)
{
	unsigned int ret=0;
	for (unsigned int i=0;i<m->numchildren;i++)
	{
		ret+=m->children[i]->init(parent, parenthandle);
	}
	return ret;
}

static void widget_layout_calc_size(widget_layout* obj, unsigned int * widths, unsigned int * heights)
{
	for (unsigned int dir=0;dir<2;dir++)
	{
		unsigned int * sizes=(dir==0 ? widths : heights);
		if (obj->m->uniform[dir])
		{
			unsigned int maxsize=0;
			for (unsigned int i=0;i<obj->m->numchildren;i++)
			{
				unsigned int thissizepx=(dir==0 ? obj->m->children[i]->width : obj->m->children[i]->height);
				unsigned int thissize=((thissizepx+obj->m->extent[dir][i]-1)/obj->m->extent[dir][i]);
				if (thissize>maxsize) maxsize=thissize;
			}
			for (unsigned int i=0;i<obj->m->totsize[dir];i++) sizes[i]=maxsize;
		}
		else
		{
			memset(sizes, 0, sizeof(unsigned int)*obj->m->totsize[dir]);
			for (unsigned int i=0;i<obj->m->numchildren;i++)
			{
				unsigned int thissize=(dir==0 ? obj->m->children[i]->width : obj->m->children[i]->height);
				if (obj->m->extent[dir][i]==1 && thissize > sizes[obj->m->startpos[dir][i]]) sizes[obj->m->startpos[dir][i]]=thissize;
			}
			//TODO: This does not grant the desired size to elements covering more than one tile.
			//GTK+ does something highly weird, so it'll need to be tested everywhere anyways. I can do whatever I want.
		}
	}
}

void widget_layout::measure()
{
	for (unsigned int i=0;i<m->numchildren;i++)
	{
		m->children[i]->measure();
	}
	unsigned int cellwidths[m->totsize[0]];
	unsigned int cellheights[m->totsize[1]];
	widget_layout_calc_size(this, cellwidths, cellheights);
	unsigned int width=0;
	for (unsigned int i=0;i<m->totsize[0];i++) width+=cellwidths[i];
	this->width=width;
	unsigned int height=0;
	for (unsigned int i=0;i<m->totsize[1];i++) height+=cellheights[i];
	this->height=height;
	
	widthprio=0;
	heightprio=0;
	for (unsigned int i=0;i<m->numchildren;i++)
	{
		if (m->children[i]->widthprio  > this->widthprio)  this->widthprio =m->children[i]->widthprio;
		if (m->children[i]->heightprio > this->heightprio) this->heightprio=m->children[i]->heightprio;
	}
}

void widget_layout::place(void* resizeinf, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	unsigned int cellwidths[m->totsize[0]];
	unsigned int cellheights[m->totsize[1]];
	widget_layout_calc_size(this, cellwidths, cellheights);
	
	for (int dir=0;dir<2;dir++)
	{
		uint32_t expand[m->totsize[dir]];
		memset(expand, 0, sizeof(uint32_t)*m->totsize[dir]);
		unsigned int extrasize_pix;
		if (dir==0) extrasize_pix = width  - this->width;
		else        extrasize_pix = height - this->height;
		unsigned int extrasize_frac=0;
		unsigned int extrasize_split=0;
		unsigned int extrasize_max=0;
		for (unsigned int i=0;i<m->numchildren;i++)
		{
			if ((dir==0 && m->children[i]->widthprio ==this->widthprio) ||
			    (dir==1 && m->children[i]->heightprio==this->heightprio))
			{
				for (unsigned int j=0;j<m->extent[dir][i];j++)
				{
					expand[m->startpos[dir][i]+j]++;
					if (expand[m->startpos[dir][i]+j] == extrasize_max) extrasize_split++;
					if (expand[m->startpos[dir][i]+j] >  extrasize_max)
					{
						extrasize_split=1;
						extrasize_max=expand[m->startpos[dir][i]+j];
					}
				}
			}
		}
		for (unsigned int i=0;i<m->totsize[dir];i++)
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
	
	unsigned int cellstartx[m->totsize[0]+1];
	cellstartx[0]=0;
	for (unsigned int i=0;i<m->totsize[0];i++) cellstartx[i+1]=cellstartx[i]+cellwidths[i];
	unsigned int cellstarty[m->totsize[1]+1];
	cellstarty[0]=0;
	for (unsigned int i=0;i<m->totsize[1];i++) cellstarty[i+1]=cellstarty[i]+cellheights[i];
	
	for (unsigned int i=0;i<m->numchildren;i++)
	{
//printf("pl %u at %u,%u %u,%u\n",i,
//x+cellstartx[m->startpos[0][i]], y+cellstarty[m->startpos[1][i]],
//cellstartx[m->startpos[0][i]+m->extent[0][i]]-cellstartx[m->startpos[0][i]],
//cellstarty[m->startpos[1][i]+m->extent[1][i]]-cellstarty[m->startpos[1][i]]);
		m->children[i]->place(resizeinf,
		                         x+cellstartx[m->startpos[0][i]], y+cellstarty[m->startpos[1][i]],
		                         cellstartx[m->startpos[0][i]+m->extent[0][i]]-cellstartx[m->startpos[0][i]],
		                         cellstarty[m->startpos[1][i]+m->extent[1][i]]-cellstarty[m->startpos[1][i]]);
	}
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

/*
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

widget_radio_group::widget_radio_group(bool vertical, widget_radio** leader, const char * firsttext, ...)
{
	unsigned int numitems=1;
	
	va_list args;
	va_start(args, firsttext);
	while (va_arg(args, const char*)) numitems++;
	va_end(args);
	
	widget_radio* items[numitems];
	items[0]=widget_create_radio(firsttext);
	va_start(args, firsttext);
	for (unsigned int i=1;i<numitems;i++)
	{
		items[i]=widget_create_radio(va_arg(args, const char*));
	}
	va_end(args);
	
	items[0]->group(numitems, items);
	if (leader) *leader=items[0];
	
	construct(numitems, (widget_base**)items, vertical?1:numitems, NULL, false, vertical?numitems:1, NULL, false);
}

widget_listbox::widget_listbox(const char * firstcol, ...)
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
	
	construct(numcols, columns);
}
*/

widget_layout::widget_layout(bool vertical, bool uniform, widget_base* firstchild, ...)
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
                             unsigned int firstwidth, unsigned int firstheight, widget_base* firstchild, ...)
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

widget_layout_grid::widget_layout_grid(unsigned int width, unsigned int height, bool uniformsizes, widget_base* firstchild, ...)
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
