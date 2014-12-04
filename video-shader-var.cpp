#include "io.h"
#include <ctype.h>
#include <stdio.h>

void video::shader::var::auto_set_wram(const uint8_t * data, size_t size)
{
	for (unsigned int i=0;i<this->au_count;i++)
	{
		if (this->au_items[i].source==so_wram && this->au_items[i].index>=size) this->au_items[i].sem=se_constant;
		else this->au_items[i].sem=this->au_items[i].sem_org;
	}
	this->au_wram=data;
	auto_reset();
}

void video::shader::var::auto_reset()
{
	this->au_framecount=0;
	for (unsigned int i=0;i<this->au_count;i++)
	{
		this->au_items[i].last=au_fetch(&this->au_items[i]);
		this->au_items[i].transition=0;
	}
}

inline uint16_t video::shader::var::au_fetch(au_item* item)
{
	uint16_t val;
	if (item->source==so_input) val=this->au_input[item->index];
	else val=this->au_wram[item->index];
	
	val&=item->mask;
	if (item->equal && val!=item->equal) val=0;
	return val;
}

void video::shader::var::auto_frame()
{
	for (unsigned int i=0;i<this->au_count;i++)
	{
		//the compiler will flatten all these references
		au_item* item=&this->au_items[i];
		uint16_t val=au_fetch(item);
		
		uint32_t& last=item->last;
		uint32_t& transition=item->transition;
		
		switch (item->sem)
		{
		case se_constant:
			{
				//used if it refers to out-of-bounds WRAM, and other invalid cases
				break;
			}
		case se_capture:
			{
				out_append(i, val);
				break;
			}
		case se_capture_previous:
			{
				if (val!=last)
				{
					out_append(i, last);
					last=val;
				}
				break;
			}
		case se_transition:
			{
				if (val!=last)
				{
					last=val;
					out_append(i, this->au_framecount);
				}
				break;
			}
		case se_transition_count:
			{
				if (val!=last)
				{
					last=val;
					transition++;
					out_append(i, transition);
				}
				break;
			}
		case se_transition_previous:
			{
				if (val!=last)
				{
					last=val;
					out_append(i, transition);
					transition=this->au_framecount;
				}
				break;
			}
		case se_python: break;//TODO
		}
	}
	this->au_framecount++;
}

void video::shader::var::setup(const struct auto_t * auto_items, unsigned int auto_count, const struct param_t * params, unsigned int param_count)
{
	this->out_names=malloc(sizeof(char*)*(auto_count+param_count));
	this->out_all=malloc(sizeof(struct change_t)*(auto_count+param_count));
	this->out_changes=malloc(sizeof(struct change_t)*1);
	this->out_numchanges=0;
	this->out_changes_buflen=1;
	
	//this->au_wram=NULL;
	//this->au_wramsize=0;
	//this->au_input[0]=0;
	//this->au_input[1]=0;
	this->au_count=auto_count;
	this->au_items=malloc(sizeof(struct au_item)*auto_count);
	this->pa_count=param_count;
	this->pa_items=malloc(sizeof(struct param_t)*param_count);
	
	for (unsigned int i=0;i<auto_count;i++)
	{
		this->au_items[i].sem   = auto_items[i].sem;
		this->au_items[i].index = auto_items[i].source;
		this->au_items[i].mask  = auto_items[i].mask;
		this->au_items[i].equal = auto_items[i].equal;
		this->out_names[i]=strdup(auto_items[i].name);
	}
	for (unsigned int i=0;i<param_count;i++)
	{
		this->pa_items[i]=params[i];
		this->pa_items[i].pub_name=strdup(this->pa_items[i].pub_name);
		this->pa_items[i].int_name=strdup(this->pa_items[i].int_name);
		this->out_names[auto_count+i]=this->pa_items[i].int_name;
	}
	auto_set_wram(NULL, 0);
}

void video::shader::var::reset()
{
	for (unsigned int i=0;i<this->au_count;i++)//the params' names are pointed to from pa_items->int_name
	{
		free((char*)this->out_names[i]);
	}
	free(this->out_names);
	free(this->out_all);
	free(this->out_changes);
	for (unsigned int i=0;i<this->pa_count;i++)
	{
		free((char*)this->pa_items[i].int_name);
		free((char*)this->pa_items[i].pub_name);
	}
	free(pa_items);
}

void video::shader::var::out_append(unsigned int index, float value)
{
	if (this->out_all[index].value==value) return;
	this->out_all[index].value=value;
	
	if (this->out_numchanges == this->out_changes_buflen)
	{
		this->out_changes_buflen*=2;
		this->out_changes=realloc(this->out_changes, sizeof(struct change_t)*this->out_changes_buflen);
	}
	this->out_changes[this->out_numchanges].index=index;
	this->out_changes[this->out_numchanges].value=value;
	this->out_numchanges++;
}
