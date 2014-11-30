#include "minir.h"
#include <ctype.h>
#include <stdio.h>

#if 0
const char * const * video::shader::var::out_get_names(unsigned int * count);
const change_t * video::shader::var::out_get_changed(unsigned int * count);
const change_t * video::shader::var::out_get_all(unsigned int * count);
void video::shader::var::auto_set_wram(uint8_t* data, size_t size);
void video::shader::var::auto_reset();

void video::shader::var::auto_frame()
{
	for (unsigned int i=0;i<this->count;i++)
	{
		//the compiler will flatten all these references
		float& out=this->values[i];
		
		au_item* item=&this->au_items[i];
		uint16_t val;
		if (item->src==so_input) val=input[item->index];
		else val=wram[item->index];
		
		val&=item->mask;
		if (item->equal && val!=item->equal) val=0;
		
		uint32_t& last=item->last;
		uint32_t& prev=item->prev;
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
				out=val;
				break;
			}
		case se_capture_previous:
			{
				if (val!=last)
				{
					prev=last;
					last=val;
				}
				out=prev;
				break;
			}
		case se_transition:
			{
				if (val!=last)
				{
					last=val;
					transition=this->au_framecount;
				}
				out=transition;
				break;
			}
		case se_transition_count:
			{
				if (val!=last)
				{
					last=val;
					transition++;
				}
				out=transition;
				break;
			}
		case se_transition_previous:
			{
				if (val!=last)
				{
					last=val;
					prev=transition;
					transition=this->au_framecount;
				}
				out=prev;
				break;
			}
		case se_python: break;//TODO
		}
	}
	this->framecount++;
}

struct param_t video::shader::var::param_get(unsigned int * count);
void video::shader::var::param_set(unsigned int id, double val);

#if 0
video::shader* video::shader_parse(const char * filename)
{
	const char * ext=strrchr(filename, '.');
	if (!ext || strchr(ext, '/')) return NULL;
	ext++;
	shader* ret=malloc(sizeof(struct shader));
	ret->pass=NULL; ret->n_pass=0;
	ret->param=NULL; ret->n_param=0;
	
	unsigned int extlen=strlen(ext);
	bool multipass=(tolower(ext[extlen-1])=='p');
	if (multipass) extlen--;
	char * passdata=NULL;
	
	if(0);
	else if (extlen==4 && !strncasecmp(ext, "glsl", extlen)) ret->type=shader::ty_glsl;
	else if (extlen==2 && !strncasecmp(ext, "cg", extlen)) ret->type=shader::ty_cg;
	else if (extlen==4 && !strncasecmp(ext, "hlsl", extlen)) ret->type=shader::ty_hlsl;
	else goto error;
	
	if (multipass)
	{
		if (!file_read(filename, (void**)&passdata, NULL)) goto error;
	}
	else
	{
		asprintf(&passdata,
			"shaders=1\n"
			"shader0=%s\n"
			//"scale_type0=source\n"
			//"scale0=1.0\n"
			//"filter_linear0=false"
			, filename);
	}
	
	char* name;
	name=passdata;
	
	unsigned int n_pass; n_pass=0;
	shader::pass_t* pass; pass=NULL;
	
	while (true)
	{
		char* lineend=strchr(name, '\n');
	{
		if (lineend) *lineend='\0';
		while (isspace(*name)) name++;
		if (*name=='\0') goto nextline;
		if (*name=='#') goto nextline;
		if (!isalpha(*name)) goto error;
		char* val=strchr(name, '=');
		if (!val) goto error;
		char* nameend=val;
		while (isspace(nameend[-1])) nameend--;
		*nameend='\0';
		val++;
		while (isspace(*val)) val++;
		char* trimwhite=lineend;
		while (isspace(trimwhite[-1])) trimwhite--;
		*trimwhite='\0';
		
		if(0);
		
		else if (!strcmp(name, "shaders"))
		{
			if (ret->pass) goto error;//this can only exist once
			char* end;
			n_pass=strtoul(val, &end, 10);
			if (n_pass<=0 || end==val || *end) goto error;
			pass=malloc(sizeof(shader::pass_t)*ret->n_pass);
			ret->n_pass=n_pass;
			ret->pass=pass;
			for (unsigned int i=0;i<n_pass;i++)
			{
				static const shader::pass_t def_pass={
					/*source*/      NULL,
					/*interpolate*/ shader::in_nearest,
					/*wrap*/        shader::wr_border,
				};
				pass[i]=def_pass;
			}
			goto nextline;
		}
		
		else if (isdigit(nameend[-1]))
		{
			if (!pass) goto error;//shaders= must come before these
			char* pass_id_str=nameend;
			while (isdigit(pass_id_str[-1])) pass_id_str--; // this is known to terminate - there's an isalpha() check earlier, and nothing is both letter and number
			char* end;
			unsigned int pass_id=strtoul(pass_id_str, &end, 10);
			if (end==pass_id_str || *end) goto error;
			*pass_id_str='\0';
			
			//bool - {0, 1, true, false}
			if(0);
			else if (!strcmp(name, "shader"))
			{
				if (pass[pass_id].source) goto error;
				if (!file_read_rel(filename, false, val, (void**)&pass[pass_id].source, NULL)) goto error;
			}
			else if (!strcmp(name, "filter_linear"))
			{
				//bool
			}
			else if (!strcmp(name, "wrap_mode"))
			{
				//{clamp_to_border, clamp_to_edge, repeat, mirrored_repeat}
			}
			else if (!strcmp(name, "frame_count_mod"))
			{
				//int
			}
			if (!strcmp(name, "srgb_framebuffer"))
			{
				//bool
			}
			if (!strcmp(name, "float_framebuffer"))
			{
				//bool
			}
			if (!strcmp(name, "mipmap_input"))
			{
				//bool
			}
			if (!strcmp(name, "alias"))
			{
				//string?
			}
			if (!strncmp(name, "scale_type", strlen("scale_type")))
			{
				//scale_type, scale_type_x, scale_type_y
				//{source, viewport, absolute}
			}
			if (!strncmp(name, "scale", strlen("scale")))
			{
				//scale, scale_x, scale_y
				//float, except if scalemode==absolute, in which case they're int
			}
			else goto error;
		}
		
		else if (!strcmp(name, "textures"))
		{
			//semicolon-separated list of strings
			
		}
		//for each texture:
		//%_linear - bool
		//%_wrap_mode - 
		//%_mipmap - 
		
		else if (!strcmp(name, "imports"))
		{
			//semicolon-separated list of strings
			//for game-aware shaders?
			//Python stuff seems painful.
		}
		//for each import:
		//%_semantic - {capture, transition, transition_count, capture_previous, transition_previous, python}
		//%_input_slot - {1, 2}
		//%_wram - hex
		//%_mask - hex
		//%_equal - hex
		//additionally:
		//import_script - string
		//import_script_class - string
		//likely used only for semantic=python
		//
		// def fetch:
		//  u16 ret;
		//  if exist %_input_slot: ret=game::input[slot] as uint16;
		//  else: ret=game::wram[%_wram] as uint8;
		//  ret &= %_mask
		//  if exist %_equal: ret=(ret==equal);
		//  return ret;
		// 
		// def delay(new):
		//  static u32 prev;
		//  static u32 val;
		//  if (val!=new):
		//   prev=val;
		//   val=new;
		//  return prev;
		//
		//if semantic is:
		// capture:
		//  return fetch()
		// transition:
		//  static u16 prev;
		//  static u32 prev_framecount;
		//  if (fetch() != prev):
		//   prev=fetch();
		//   prev_framecount=game::framecount;
		//  return prev_framecount;
		// transition_count:
		//  static u16 prev;
		//  static u32 prev_count;
		//  if (fetch() != prev):
		//   prev=fetch();
		//   prev_count++;
		//  return prev_count;
		// capture_previous:
		//  return delay(capture());
		// transition_previous:
		//  return delay(transition());
		// python: ???
		
		else goto error;
	}
	nextline:
		if (!lineend) break;
		name=lineend+1;
	}
	
	return ret;
	
error:
	shader_delete(ret);
	free(passdata);
	return NULL;
}

video::shader* video::shader_clone(const shader* other)
{
	shader* ret=malloc(sizeof(struct shader));
	*ret=*other;
	ret->pass=malloc(sizeof(shader::pass_t)*other->n_pass);
	for (unsigned int i=0;i<other->n_pass;i++)
	{
		((shader::pass_t*)ret->pass)[i]=other->pass[i];
		((shader::pass_t*)ret->pass)[i].source=strdup(other->pass[i].source);
	}
	ret->param=malloc(sizeof(shader::param_t)*other->n_param);
	for (unsigned int i=0;i<other->n_param;i++)
	{
		((shader::param_t*)ret->param)[i]=other->param[i];
		((shader::param_t*)ret->param)[i].name=strdup(other->param[i].name);
		((shader::param_t*)ret->param)[i].pub_name=strdup(other->param[i].pub_name);
	}
	return ret;
}

void video::shader_delete(shader* obj)
{
	for (unsigned int i=0;i<obj->n_pass;i++)
	{
		free((char*)obj->pass[i].source);
	}
	for (unsigned int i=0;i<obj->n_param;i++)
	{
		free((char*)obj->param[i].name);
		free((char*)obj->param[i].pub_name);
	}
	free((void*)obj->pass);
	free((void*)obj->param);
	free(obj);
}

char * video::shader_translate(shader::type_t in, shader::type_t out, const char * source)
{
	return NULL;
}

video::shader* video::shader_translate(const shader* in, shader::type_t out)
{
	shader* ret=shader_clone(in);
	for (unsigned int i=0;i<in->n_pass;i++)
	{
		free((char*)ret->pass[i].source);
		((shader::pass_t*)&ret->pass[i])->source=shader_translate(in->type, out, in->pass[i].source);
	}
	return ret;
}
#endif
#endif
