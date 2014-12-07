#include "io.h"

//const struct video::shader::translation list_trans[]={
//	&video_shader_translate_cgc,
//	NULL
//};

char * video::shader::translate(lang_t from, lang_t to, const char * text)
{
	if (from==to) return strdup(text);
	return NULL;
}
