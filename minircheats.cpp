#include "minir.h"
#include "os.h"
#include "window.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include "libretro.h"

#define this This

//http://msdn.microsoft.com/en-us/library/vstudio/tcxf1dw6.aspx says %zX is not supported
//let's define it to whatever they do support.
#ifdef _WIN32
#define z "I"
#else
#define z "z"
#endif

namespace {

enum cheat_compto { cht_prev, cht_cur, cht_curptr };
enum cheat_dattype { cht_unsign, cht_sign, cht_hex };
struct minircheatdetail;
static void details_free(struct minircheatdetail * this);
struct minircheats_impl {
	struct minircheats i;
	
	//struct libretro * core;
	struct window * parent;
	
	struct minircheats_model * model;
	
	struct window * wndsrch;
	struct widget_listbox * wndsrch_listbox;
	struct widget_label * wndsrch_nummatch;
	struct widget_radio * wndsrch_comptype;
	struct widget_radio * wndsrch_compto_select;
	struct widget_radio * wndsrch_compto_select_prev;
	struct widget_textbox * wndsrch_compto_entry;
	struct widget_label * wndsrch_compto_label;
	struct widget_radio * wndsrch_dattype;
	struct widget_radio * wndsrch_datendian;
	struct widget_radio * wndsrch_datsize;
	
	struct window * wndlist;
	struct widget_listbox * wndlist_listbox;
	struct widget_textbox * wndlist_code;
	struct widget_textbox * wndlist_desc;
	struct widget_textbox * wndlist_addr;
	struct widget_textbox * wndlist_val;
	
	struct window * wndwatch;
	
	//This is one random cheat detail window; it's used to destroy them all when changing the core or destroying this object.
	//The rest are available as a linked list. The list is not sorted; the order in which things are
	// destroyed doesn't matter, and that's all it's used for.
	struct minircheatdetail * details;
	
	bool enabled :1;
	bool prev_enabled :1;
	
	unsigned int datsize :3;
	enum cheat_dattype dattype :2;
	
	bool hassearched :1;
	
	char celltmp[32];
};

//out length must be at least 16 bytes; return value is 'out'
static const char * encodeval(enum cheat_dattype dattype, unsigned int datsize, uint32_t val, char * out)
{
	if (dattype==cht_hex)    sprintf(out, "%.*X", datsize*2, val);
	if (dattype==cht_sign)   sprintf(out, "%i", (int32_t)val);
	if (dattype==cht_unsign) sprintf(out, "%u", val);
	return out;
}

static bool decodeval(enum cheat_dattype dattype, const char * str, uint32_t * val)
{
	const char * out;
	if (dattype==cht_hex) *val=strtoul(str, (char**)&out, 16);
	if (dattype==cht_sign) *val=strtol(str, (char**)&out, 10);
	if (dattype==cht_unsign) *val=strtoul(str, (char**)&out, 10);
	return (str!=out && !*out && str[1]!='x' && str[1]!='X');
}

static void search_update(struct minircheats_impl * this);
static void search_update_compto_prev(struct minircheats_impl * this);
static void list_update(struct minircheats_impl * this);

static void set_parent(struct minircheats * this_, struct window * parent)
{
	struct minircheats_impl * this=(struct minircheats_impl*)this_;
	this->parent=parent;
}

static void set_core(struct minircheats * this_, struct libretro * core, size_t prev_limit)
{
	struct minircheats_impl * this=(struct minircheats_impl*)this_;
	while (this->details) details_free(this->details);
	if (core)
	{
		unsigned int nummem;
		const struct retro_memory_descriptor * memory;
		memory=core->get_memory_info(core, &nummem);
		if (memory)
		{
			this->model->set_memory(this->model, memory, nummem);
		}
		else
		{
			void* wram;
			size_t wramlen;
			core->get_memory(core, libretromem_wram, &wramlen, &wram);
			if (wram)
			{
				struct retro_memory_descriptor wramdesc;
				wramdesc.ptr=wram;
				wramdesc.len=wramlen;
				this->model->set_memory(this->model, &wramdesc, 1);
			}
			else
			{
				this->model->set_memory(this->model, NULL, 0);
			}
		}
	}
	else
	{
		this->model->set_memory(this->model, NULL, 0);
	}
	this->prev_enabled=(this->model->prev_get_size(this->model) <= prev_limit);
	this->model->prev_set_enabled(this->model, this->prev_enabled);
	search_update_compto_prev(this);
	search_update(this);
	list_update(this);
}




struct minircheatdetail {
	struct minircheats_impl * parent;
	
	struct window * wndw;
	struct widget_textbox * addr;
	struct widget_textbox * newval;
	struct widget_textbox * desc;
	struct widget_radio * size;
	struct widget_checkbox * allowinc;
	struct widget_checkbox * allowdec;
	
	enum cheat_dattype dattype :2;
	char orgaddr[32];
	
	//A linked list, so they can be closed.
	struct minircheatdetail * prev;
	struct minircheatdetail * next;
};

void details_ok(struct minircheatdetail * this)
{
	uint32_t val;
	if (!decodeval(this->dattype, this->newval->get_text(), &val))
	{
		this->newval->set_invalid(true);
		return;
	}
	const char * orgaddr=this->orgaddr;
	struct cheat newcheat={};
	newcheat.addr=(char*)this->addr->get_text();
	newcheat.datsize=this->size->get_state()+1;
	newcheat.val=val;
	newcheat.issigned=(this->dattype==cht_sign);
	newcheat.changetype=this->allowinc->get_state() + this->allowdec->get_state()*2;
	newcheat.enabled=true;
	newcheat.desc=this->desc->get_text();
	if (!this->parent->model->cheat_set(this->parent->model, -1, &newcheat))
	{
		this->addr->set_invalid(true);
		return;
	}
	int cheatid_org=this->parent->model->cheat_find_for_addr(this->parent->model, newcheat.datsize, orgaddr);
	int cheatid_new=this->parent->model->cheat_find_for_addr(this->parent->model, newcheat.datsize, this->addr->get_text());
	if (cheatid_new>=0 && cheatid_org!=cheatid_new)
	{
		if (!window_message_box("There is already a cheat set for that address. Do you wish to replace it?", NULL, mb_info, mb_yesno))
		{
			return;
		}
		this->parent->model->cheat_remove(this->parent->model, cheatid_new);
	}
	if (cheatid_org<0) cheatid_org=this->parent->model->cheat_get_count(this->parent->model);
	this->parent->model->cheat_set(this->parent->model, cheatid_org, &newcheat);
	list_update(this->parent);
	details_free(this);
}

void details_cancel(struct minircheatdetail * this)
{
	details_free(this);
}

static bool details_onclose(struct window * subject, void* userdata)
{
	struct minircheatdetail * this=(struct minircheatdetail*)userdata;
	details_free(this);
	return true;
}

static void details_free(struct minircheatdetail * this)
{
	if (this->prev) this->prev->next=this->next;
	else this->parent->details=this->next;
	if (this->next) this->next->prev=this->prev;
	this->wndw->free(this->wndw);
	free(this);
}

static void details_create(struct minircheats_impl * parent, struct window * parentwndw, const struct cheat * thecheat, bool hex)
{
	struct minircheatdetail * this=malloc(sizeof(struct minircheatdetail));
	this->parent=parent;
	this->dattype=hex ? cht_hex : thecheat->issigned ? cht_sign : cht_unsign;
	const char * addr=thecheat->addr;
	if (!addr) addr="";
	strcpy(this->orgaddr, addr);
	
	struct widget_textbox * curval;
	struct widget_button * ok;
	struct widget_button * cancel;
	this->wndw=window_create(
		widget_create_layout_vert(
			widget_create_layout_grid(2, 6, false,
				widget_create_label("Address"), this->addr=widget_create_textbox(),
				widget_create_label("Current Value"), curval=widget_create_textbox(),
				widget_create_label("New Value"), this->newval=widget_create_textbox(),
				//TODO: size and type, change mode, etc
				widget_create_label("Description"), this->desc=widget_create_textbox(),
				widget_create_label("Size"), widget_create_radio_group_horz(&this->size, "1", "2", "3", "4 bytes", NULL),
				widget_create_label("Allow"), widget_create_layout_horz(
					this->allowinc=widget_create_checkbox("increment"),
					this->allowdec=widget_create_checkbox("decrement"),
					NULL)
			),
			widget_create_layout_horz(
				widget_create_padding_horz(),
				ok=widget_create_button("OK"),
				cancel=widget_create_button("Cancel"),
				NULL),
			NULL)
		);
	
	this->wndw->set_is_dialog(this->wndw);
	this->wndw->set_parent(this->wndw, parentwndw);
	this->wndw->set_title(this->wndw, "Cheat Details");
	this->wndw->set_onclose(this->wndw, details_onclose, this);
	
	this->addr->set_text(addr);
	this->addr->set_width(this->parent->model->cheat_get_max_addr_len(this->parent->model));
	this->addr->set_length(31);
	
	if (thecheat->datsize!=0)
	{
		char valstr[16];
		encodeval(this->dattype, thecheat->datsize, thecheat->val, valstr);
		curval->set_text(valstr);
		this->newval->set_text(valstr);//default to keep at current value
		curval->set_enabled(false);
		
		this->size->set_state(thecheat->datsize-1);
	}
	
	this->allowinc->set_state((thecheat->changetype&1));
	this->allowdec->set_state((thecheat->changetype&2));
	
	ok->set_onclick(bind_this(details_ok));
	cancel->set_onclick(bind_this(details_cancel));
	
	if (*addr) this->newval->focus();
	else this->addr->focus();
	
	this->wndw->set_visible(this->wndw, true);
	this->wndw->focus(this->wndw);
	
	this->next=this->parent->details;
	this->prev=NULL;
	if (this->next) this->next->prev=this;
	this->parent->details=this;
}




const char * search_get_cell(struct minircheats_impl * this, int column, size_t row);

static void search_update(struct minircheats_impl * this)
{
	if (this->wndsrch_listbox)
	{
		size_t num_rows=this->model->search_get_num_rows(this->model);
		
		this->wndsrch_listbox->set_num_rows(num_rows);
		
		if (this->hassearched)
		{
			char label[64];
			sprintf(label, "%"z"u match%s", num_rows, num_rows==1 ? "" : "es");
			this->wndsrch_nummatch->set_text(label);
		}
		else this->wndsrch_nummatch->set_text("");
	}
}

void search_set_datsize(struct minircheats_impl * this, unsigned int state)
{
	this->datsize=state+1;
	this->model->search_set_datsize(this->model, state+1);
	search_update(this);
}

void search_set_dattype(struct minircheats_impl * this, unsigned int state)
{
	this->dattype=(enum cheat_dattype)state;
	this->model->search_set_signed(this->model, (state==cht_sign));
	search_update(this);
}

void search_set_compto_select(struct minircheats_impl * this, unsigned int state)
{
	this->wndsrch_compto_entry->set_enabled((state!=cht_prev));
	this->wndsrch_compto_label->set_enabled((state!=cht_prev));
}

static void search_split(unsigned int id, void* userdata)
{
	struct minircheats_impl * this=(struct minircheats_impl*)userdata;
	this->model->thread_do_work(this->model, id);
}

void search_dosearch(struct minircheats_impl * this)
{
	uint32_t compto_val;
	
	enum cheat_compto comptowhat=(enum cheat_compto)(this->wndsrch_compto_select->get_state());
	bool comptoprev=(comptowhat==cht_prev);
	if (!comptoprev)
	{
		const char * compto_str=(this->wndsrch_compto_entry->get_text());
		if (comptowhat==cht_curptr)
		{
			if (!this->model->cheat_read(this->model, compto_str, this->datsize, &compto_val))
			{
				this->wndsrch_compto_entry->set_invalid(true);
				return;
			}
		}
		else
		{
			if (!decodeval(this->dattype, compto_str, &compto_val))
			{
				this->wndsrch_compto_entry->set_invalid(true);
				return;
			}
		}
	}
	
	this->model->search_do_search(this->model, (enum cheat_compfunc)this->wndsrch_comptype->get_state(), comptoprev, compto_val);
	thread_split(this->model->thread_get_count(this->model), search_split, this);
	this->model->thread_finish_work(this->model);
	
	this->hassearched=true;
	search_update(this);
}

void search_reset(struct minircheats_impl * this)
{
	this->model->search_reset(this->model);
	this->hassearched=false;
	search_update(this);
}

static void search_add_cheat(struct minircheats_impl * this, int row)
{
	if (row>=0)
	{
		char addr[32];
		
		struct cheat thecheat={};
		thecheat.addr=addr;
		thecheat.changetype=cht_const;
		thecheat.datsize=this->datsize;
		thecheat.issigned=(this->dattype==cht_sign);
		thecheat.enabled=true;
		this->model->search_get_row(this->model, row, thecheat.addr, &thecheat.val, NULL);
		
		details_create(this, this->wndsrch, &thecheat, (this->dattype==cht_hex));
	}
	else
	{
		struct cheat thecheat={};
		thecheat.changetype=cht_const;
		thecheat.datsize=this->datsize;
		thecheat.issigned=(this->dattype==cht_sign);
		thecheat.enabled=true;
		details_create(this, this->wndsrch, &thecheat, (this->dattype==cht_hex));
	}
}

void search_add_cheat_listbox(struct minircheats_impl * this, size_t row)
{
	search_add_cheat(this, row);
}

void search_add_cheat_button(struct minircheats_impl * this)
{
	search_add_cheat(this, this->wndsrch_listbox->get_active_row());
}

static void search_update_compto_prev(struct minircheats_impl * this)
{
	if (this->wndsrch)
	{
		this->wndsrch_compto_select_prev->set_enabled(this->prev_enabled);
		if (!this->prev_enabled && this->wndsrch_compto_select->get_state()==cht_prev)
		{
			this->wndsrch_compto_select->set_state(cht_cur);
		}
	}
}

void search_ok(struct minircheats_impl * this)
{
	this->wndsrch->set_visible(this->wndsrch, false);
}

static void show_search(struct minircheats * this_)
{
	struct minircheats_impl * this=(struct minircheats_impl*)this_;
	
	if (!this->wndsrch)
	{
		struct widget_button * dosearch;
		struct widget_button * addcheat;
		struct widget_button * reset;
		struct widget_button * ok;
		
		struct widget_radio * compto_select[3];
		
		this->wndsrch=window_create(
			widget_create_layout_vert(
				widget_create_layout_horz(
					this->wndsrch_listbox=widget_create_listbox("Address", "Curr. Value", "Prev. Value", NULL),
					widget_create_layout_vert(
						dosearch=widget_create_button("Search"),
						addcheat=widget_create_button("Add Cheat..."),
						reset=widget_create_button("Reset"),
						this->wndsrch_nummatch=widget_create_label(""),
						widget_create_padding_vert(),
						widget_create_button("Watch")->set_enabled(false),
						widget_create_button("Clear Watches")->set_enabled(false),
						widget_create_button("Load Watches")->set_enabled(false),
						widget_create_button("Save Watches")->set_enabled(false),
						NULL),
					NULL),
				widget_create_layout_horz(
						widget_create_frame("Comparison Type",
								widget_create_radio_group_vert(
									&this->wndsrch_comptype,
									"< (Less Than)",              "> (Greater Than)",
									"<= (Less Than or Equal To)", ">= (Greater Than or Equal To)",
									"= (Equal To)",               "!= (Not Equal To)",
									NULL)
							),
					widget_create_layout_vert(
						widget_create_frame("Compare To",
							//widget_create_radio_group(&this->wndsrch_compto_select, true, "Previous Value", "Entered Value", "Entered Address", NULL)
							widget_create_radio_group_vert(
								compto_select[0]=widget_create_radio("Previous Value"),//this manual layout is because I need to be able to disable #1
								compto_select[1]=widget_create_radio("Entered Value"),
								compto_select[2]=widget_create_radio("Entered Address"),
								NULL)
							),
						widget_create_frame("Data Type",
							widget_create_radio_group_vert(&this->wndsrch_dattype, "Unsigned (>= 0)", "Signed (+/-)", "Hexadecimal", NULL)
							),
						NULL),
					widget_create_layout_vert(
						widget_create_frame("Data Size",
							widget_create_radio_group_vert(&this->wndsrch_datsize, "1 byte", "2 bytes", "3 bytes", "4 bytes", NULL)
							),
						widget_create_padding_vert(),
						NULL),
					NULL),
				widget_create_layout_horz(
					this->wndsrch_compto_label=widget_create_label("Enter a Value: "),
					this->wndsrch_compto_entry=widget_create_textbox(),
					ok=widget_create_button("OK"),
					NULL),
				NULL)
			);
		
		this->wndsrch->set_is_dialog(this->wndsrch);
		this->wndsrch->set_parent(this->wndsrch, this->parent);
		this->wndsrch->set_title(this->wndsrch, "Cheat Search");
		
		this->wndsrch_compto_select=compto_select[0];
		this->wndsrch_compto_select_prev=compto_select[cht_prev];
		search_update_compto_prev(this);
		
		this->wndsrch_listbox->set_contents(bind_this(search_get_cell), NULL);
		const unsigned int tmp[]={15, 15, 15};
		//const unsigned int tmp[]={1,2,2};
		this->wndsrch_listbox->set_size(16, tmp, -1);
		this->wndsrch_listbox->set_onactivate(bind_this(search_add_cheat_listbox));
		
		dosearch->set_onclick(bind_this(search_dosearch));
		reset->set_onclick(bind_this(search_reset));
		ok->set_onclick(bind_this(search_ok));
		
		this->wndsrch_compto_select->set_onclick(bind_this(search_set_compto_select));
		search_set_compto_select(this, 0);
		
		this->wndsrch_datsize->set_onclick(bind_this(search_set_datsize));
		search_set_datsize(this, 0);
		
		this->wndsrch_dattype->set_onclick(bind_this(search_set_dattype));
		search_set_dattype(this, 0);
		
		addcheat->set_onclick(bind_this(search_add_cheat_button));
		
		this->wndsrch_nummatch->set_alignment(0);
		this->wndsrch_nummatch->set_ellipsize(true);
	}
	
	search_update(this);
	
	this->wndsrch->set_visible(this->wndsrch, true);
	this->wndsrch->focus(this->wndsrch);
}

const char * search_get_cell(struct minircheats_impl * this, int column, size_t row)
{
	if (column==0)
	{
		this->model->search_get_row(this->model, row, this->celltmp, NULL, NULL);
	}
	else
	{
		uint32_t val;
		if (column==1) this->model->search_get_row(this->model, row, NULL, &val, NULL);
		if (column==2)
		{
			if (this->prev_enabled) this->model->search_get_row(this->model, row, NULL, NULL, &val);
			else return "---";
		}
		encodeval(this->dattype, this->datsize, val, this->celltmp);
	}
	return this->celltmp;
}





static void list_update(struct minircheats_impl * this)
{
	if (this->wndlist_listbox)
	{
		this->wndlist_listbox->set_num_rows(this->model->cheat_get_count(this->model));
	}
}

const char * list_get_cell(struct minircheats_impl * this, int column, size_t row)
{
	struct cheat thecheat;
	thecheat.addr=this->celltmp;
	this->model->cheat_get(this->model, row, &thecheat);
	const char changechar[]="\0+-=";
	switch (column)
	{
		case 0: break;
		case 1: sprintf(this->celltmp, "%.*X%s%c", thecheat.datsize*2, thecheat.val, thecheat.issigned?"S":"", changechar[thecheat.changetype]); break;
		case 2: return thecheat.desc;
	}
	return this->celltmp;
}

void list_listbox_activate(struct minircheats_impl * this, size_t row)
{
	struct cheat thecheat;
	char addr[32];
	thecheat.addr=addr;
	this->model->cheat_get(this->model, row, &thecheat);
	details_create(this, this->wndlist, &thecheat, false);
}

void list_add_cheat(struct minircheats_impl * this)
{
	struct cheat thecheat={};
	thecheat.addr=NULL;
	thecheat.changetype=cht_const;
	thecheat.datsize=1;
	thecheat.issigned=false;
	thecheat.enabled=true;
	details_create(this, this->wndlist, &thecheat, false);
}

void list_delete_cheat(struct minircheats_impl * this)
{
	size_t pos=this->wndlist_listbox->get_active_row();
	if (pos!=(size_t)-1)
	{
		this->model->cheat_remove(this->model, pos);
		list_update(this);
	}
}

void list_edit_cheat(struct minircheats_impl * this)
{
	size_t pos=this->wndlist_listbox->get_active_row();
	if (pos!=(size_t)-1)
	{
		struct cheat thecheat;
		char addr[32];
		thecheat.addr=addr;
		this->model->cheat_get(this->model, pos, &thecheat);
		details_create(this, this->wndlist, &thecheat, false);
	}
}

void list_clear_cheat(struct minircheats_impl * this)
{
	for (int i=this->model->cheat_get_count(this->model)-1;i>=0;i--) this->model->cheat_remove(this->model, i);
	list_update(this);
}

void list_sort_cheat(struct minircheats_impl * this)
{
	this->model->cheat_sort(this->model);
	list_update(this);
}

static void show_list(struct minircheats * this_)
{
	struct minircheats_impl * this=(struct minircheats_impl*)this_;
	if (!this->wndlist)
	{
		this->wndlist=window_create(
			//widget_create_layout_vert(
				widget_create_layout_horz(
					this->wndlist_listbox=widget_create_listbox("Address", "Value", "Description", NULL),
					widget_create_layout_vert(
						widget_create_button("Add")->set_onclick(bind_this(list_add_cheat)),
						widget_create_button("Delete")->set_onclick(bind_this(list_delete_cheat)),
						widget_create_button("Edit")->set_onclick(bind_this(list_edit_cheat)),
						widget_create_button("Clear")->set_onclick(bind_this(list_clear_cheat)),
						widget_create_button("Sort")->set_onclick(bind_this(list_sort_cheat)),
						widget_create_padding_vert(),
						NULL),
					NULL)//,
					
				//widget_create_layout_grid(2, 3, false,
					//widget_create_label("Cheat Code"),
					//widget_create_textbox(),
					//
					//widget_create_label("Cheat Description"),
					//widget_create_textbox(),
					//
					//widget_create_label("Cheat Address (hex)"),
					//widget_create_layout_horz(
						//widget_create_textbox(),
						//widget_create_label("New Value"),
						//widget_create_textbox(),
						//NULL)
					//),
				//NULL)
			);
		
		this->wndlist->set_is_dialog(this->wndlist);
		this->wndlist->set_parent(this->wndlist, this->parent);
		this->wndlist->set_title(this->wndlist, "Cheat Entry and Editor");
		
		this->wndlist_listbox->set_contents(bind_this(list_get_cell), NULL);
		this->wndlist_listbox->set_onactivate(bind_this(list_listbox_activate));
		//value width is max length of 4294967295 and 0xFFFFFFFF
		//description width is just something random
		const unsigned int tmp[]={this->model->cheat_get_max_addr_len(this->model), 10, 10};
		this->wndlist_listbox->set_size(8, tmp, 2);
	}
	
	this->wndlist->set_visible(this->wndlist, true);
	this->wndlist->focus(this->wndlist);
	
	list_update(this);
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
	if (ramwatch && this->wndsrch_listbox) this->wndsrch_listbox->refresh((size_t)-1);
	if (!this->enabled) return;
	this->model->cheat_apply(this->model);
}

static void free_(struct minircheats * this_)
{
	struct minircheats_impl * this=(struct minircheats_impl*)this_;
	
	while (this->details) details_free(this->details);
	
	if (this->wndsrch) this->wndsrch->free(this->wndsrch);
	if (this->wndlist) this->wndlist->free(this->wndlist);
	
	this->model->free(this->model);
	
	free(this);
}

}

struct minircheats * minircheats_create()
{
	struct minircheats_impl * this=malloc(sizeof(struct minircheats_impl));
	memset(this, 0, sizeof(struct minircheats_impl));
	this->i.set_parent=set_parent;
	this->i.set_core=set_core;
	this->i.show_search=show_search;
	this->i.show_list=show_list;
	this->i.set_enabled=set_enabled;
	this->i.get_enabled=get_enabled;
	this->i.update=update;
	//this->i.get_cheat_count=get_cheat_count;
	//this->i.get_cheat=get_cheat;
	//this->i.set_cheat=set_cheat;
	this->i.free=free_;
	
	this->enabled=true;
	this->model=minircheats_create_model();
	this->model->thread_enable(this->model, thread_ideal_count());
	
	return (struct minircheats*)this;
}
