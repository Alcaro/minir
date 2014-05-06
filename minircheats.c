#include "minir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

//window titles:
//Cheat Search
//Cheat Entry and Editor
//Cheat Details

enum cheat_compto { cht_prev, cht_cur, cht_curptr };
enum cheat_dattype { cht_unsign, cht_sign, cht_hex };
struct minircheats_impl {
	struct minircheats i;
	
	//struct libretro * core;
	struct window * parent;
	
	struct minircheats_model * model;
	
	struct window * wndsrch;
	struct widget_button * wndsrch_dosearch;
	struct widget_listbox * wndsrch_listbox;
	struct widget_radio * wndsrch_comptype;
	struct widget_radio * wndsrch_compto_select;
	struct widget_textbox * wndsrch_compto_entry;
	struct widget_label * wndsrch_compto_label;
	struct widget_radio * wndsrch_dattype;
	struct widget_radio * wndsrch_datendian;
	struct widget_radio * wndsrch_datsize;
	
	struct window * wndlist;
	
	struct window * wndwatch;
	
	struct window * wnddetail;
	
	bool enabled :1;
	
	unsigned int datsize :3;
	enum cheat_dattype dattype :2;
};

static void search_update(struct minircheats_impl * this);

static void set_core(struct minircheats * this_, struct libretro * core)
{
	struct minircheats_impl * this=(struct minircheats_impl*)this_;
	this->model->set_core(this->model, core);
	search_update(this);
}

static void set_parent(struct minircheats * this_, struct window * parent)
{
	struct minircheats_impl * this=(struct minircheats_impl*)this_;
	this->parent=parent;
}





static const char * search_get_cell(struct widget_listbox * subject, unsigned int row, unsigned int column, void * userdata);







static void search_update(struct minircheats_impl * this)
{
	if (this->wndsrch_listbox)
	{
		this->wndsrch_listbox->set_contents_virtual(this->wndsrch_listbox,
		                                            this->model->search_get_num_rows(this->model),
		                                            search_get_cell, NULL, this);
	}
}


//test


static void search_set_datsize(struct widget_radio * subject, unsigned int state, void* userdata)
{
	struct minircheats_impl * this=(struct minircheats_impl*)userdata;
	this->datsize=state+1;
	this->model->search_set_datsize(this->model, state+1);
	search_update(this);
}

static void search_set_dattype(struct widget_radio * subject, unsigned int state, void* userdata)
{
	struct minircheats_impl * this=(struct minircheats_impl*)userdata;
	this->dattype=state;
	this->model->search_set_signed(this->model, (state==cht_sign));
	search_update(this);
}

static void search_set_compto_select(struct widget_radio * subject, unsigned int state, void* userdata)
{
	struct minircheats_impl * this=(struct minircheats_impl*)userdata;
	this->wndsrch_compto_entry->set_enabled(this->wndsrch_compto_entry, (state!=cht_prev));
	this->wndsrch_compto_label->set_enabled(this->wndsrch_compto_label, (state!=cht_prev));
}

static void search_dosearch(struct widget_button * subject, void* userdata)
{
	struct minircheats_impl * this=(struct minircheats_impl*)userdata;
	
	uint32_t compto_val;
	
	enum cheat_compto comptowhat=(this->wndsrch_compto_select->get_state(this->wndsrch_compto_select));
	bool comptoprev=(comptowhat==cht_prev);
	if (!comptoprev)
	{
		const char * compto_str=(this->wndsrch_compto_entry->get_text(this->wndsrch_compto_entry));
		if (this->dattype==cht_hex) compto_val=strtoul(compto_str, NULL, 16);
		if (this->dattype==cht_sign) compto_val=strtol(compto_str, NULL, 10);
		if (this->dattype==cht_unsign) compto_val=strtoul(compto_str, NULL, 10);
	}
	this->model->search_do_search(this->model, this->wndsrch_comptype->get_state(this->wndsrch_comptype), comptoprev, compto_val);
	search_update(this);
}

static void search_reset(struct widget_button * subject, void* userdata)
{
	struct minircheats_impl * this=(struct minircheats_impl*)userdata;
	this->model->search_reset(this->model);
	search_update(this);
}

static void show_search(struct minircheats * this_)
{
	struct minircheats_impl * this=(struct minircheats_impl*)this_;
	
	if (!this->wndsrch)
	{
		struct widget_button * reset;
		
		this->wndsrch=window_create(
			widget_create_layout_vert(
				widget_create_layout_horz(
					this->wndsrch_listbox=widget_create_listbox("Address", "Curr. Value", "Prev. Value", NULL),
					widget_create_layout_vert(
						this->wndsrch_dosearch=widget_create_button("Search"),
						widget_create_button("Add Cheat..."),
						reset=widget_create_button("Reset"),
						widget_create_padding_vert(),
						widget_create_button("Watch"),
						widget_create_button("Clear Watches"),
						widget_create_button("Load Watches"),
						widget_create_button("Save Watches"),
						NULL),
					NULL),
				widget_create_layout_horz(
						widget_create_frame("Comparison Type",
								widget_create_radio_group(&this->wndsrch_comptype, true,
									"< (Less Than)", "> (Greater Than)",
									"<= (Less Than or Equal To)", ">= (Greater Than or Equal To)",
									"= (Equal To)", "!= (Not Equal To)",
									NULL)
							),
					widget_create_layout_vert(
						widget_create_frame("Compare To",
							widget_create_radio_group(&this->wndsrch_compto_select, true, "Previous Value", "Entered Value", "Entered Address", NULL)
							),
						widget_create_frame("Data Type",
							widget_create_radio_group(&this->wndsrch_dattype, true, "Unsigned (>= 0)", "Signed (+/-)", "Hexadecimal", NULL)
							),
						NULL),
					widget_create_layout_vert(
						widget_create_frame("Data Size",
							widget_create_radio_group(&this->wndsrch_datsize, true, "1 byte", "2 bytes", "3 bytes", "4 bytes", NULL)
							),
						widget_create_padding_vert(),
						NULL),
					NULL),
				widget_create_layout_horz(
					this->wndsrch_compto_label=widget_create_label("Enter a Value: "),
					this->wndsrch_compto_entry=widget_create_textbox(),
					widget_create_button("OK"),
					widget_create_button("Cancel"),
					NULL),
				NULL)
			);
		
		this->wndsrch->set_is_dialog(this->wndsrch);
		this->wndsrch->set_parent(this->wndsrch, this->parent);
		this->wndsrch->set_title(this->wndsrch, "Cheat Search");
		
		//const unsigned int tmp[]={15, 15, 15};
		const unsigned int tmp[]={1,2,2};
		this->wndsrch_listbox->set_size(this->wndsrch_listbox, 16, tmp);
		
		this->wndsrch_dosearch->set_onclick(this->wndsrch_dosearch, search_dosearch, this);
		reset->set_onclick(reset, search_reset, this);
		
		this->wndsrch_compto_select->set_onclick(this->wndsrch_compto_select, search_set_compto_select, this);
		search_set_compto_select(NULL, 0, this);
		
		this->wndsrch_datsize->set_onclick(this->wndsrch_datsize, search_set_datsize, this);
		search_set_datsize(NULL, 0, this);
		
		this->wndsrch_dattype->set_onclick(this->wndsrch_dattype, search_set_dattype, this);
		search_set_dattype(NULL, 0, this);
	}
	
	search_update(this);
	
	this->wndsrch->set_visible(this->wndsrch, true);
	this->wndsrch->focus(this->wndsrch);
}

static const char * search_get_cell(struct widget_listbox * subject, unsigned int row, unsigned int column, void * userdata)
{
	struct minircheats_impl * this=(struct minircheats_impl*)userdata;
	
	static char ret[64];
	if (column==0)
	{
		const char * prefix;
		unsigned int addrlen;
		uint32_t addr;
		this->model->search_get_vis_row(this->model, row, &prefix, &addrlen, &addr, NULL, NULL);
		sprintf(ret, "%s%.*X", prefix, addrlen, addr);
	}
	else
	{
		uint32_t val;
		if (column==1) this->model->search_get_vis_row(this->model, row, NULL, NULL, NULL, &val, NULL);
		if (column==2) this->model->search_get_vis_row(this->model, row, NULL, NULL, NULL, NULL, &val);
		if (this->dattype==cht_hex) sprintf(ret, "%.*X", this->datsize*2, val);
		if (this->dattype==cht_sign) sprintf(ret, "%i", (int32_t)val);
		if (this->dattype==cht_unsign) sprintf(ret, "%u", val);
	}
	return ret;
}





static void show_list(struct minircheats * this_)
{
	//struct minircheats_impl * this=(struct minircheats_impl*)this_;
	
}





static void update(struct minircheats * this_, bool ramwatch);
static void set_enabled(struct minircheats * this_, bool enabled)
{
	struct minircheats_impl * this=(struct minircheats_impl*)this_;
	this->enabled=enabled;
	update(this_, true);
}

static bool get_enabled(struct minircheats * this_)
{
	struct minircheats_impl * this=(struct minircheats_impl*)this_;
	return this->enabled;
}

static void update(struct minircheats * this_, bool ramwatch)
{
	struct minircheats_impl * this=(struct minircheats_impl*)this_;
	//Note that various windows should be updated even if cheats are disabled. If the user wants them gone, he can close them.
	if (ramwatch && this->wndsrch_listbox) this->wndsrch_listbox->refresh(this->wndsrch_listbox);
	if (!this->enabled) return;
	
}

static void free_(struct minircheats * this_)
{
	struct minircheats_impl * this=(struct minircheats_impl*)this_;
	
	if (this->wndsrch) this->wndsrch->free(this->wndsrch);
	if (this->wndlist) this->wndlist->free(this->wndlist);
	if (this->wnddetail) this->wnddetail->free(this->wnddetail);
	
	this->model->free(this->model);
	
	free(this);
}

struct minircheats * minircheats_create()
{
	struct minircheats_impl * this=malloc(sizeof(struct minircheats_impl));
	memset(this, 0, sizeof(struct minircheats_impl));
	this->i.set_core=set_core;
	this->i.set_parent=set_parent;
	this->i.show_search=show_search;
	this->i.show_list=show_list;
	this->i.set_enabled=set_enabled;
	this->i.get_enabled=get_enabled;
	this->i.update=update;
	this->i.free=free_;
	
	this->enabled=true;
	this->model=minircheats_create_model();
	
	return (struct minircheats*)this;
}
