#include "io.h"

char * video::shader::translate(lang_t from, lang_t to, const char * text)
{
	if (from==to) return strdup(text);
	
	char * (*translators[])(lang_t from, lang_t to, const char * text) = {
		translate_cgc,
		NULL
	};
	for (unsigned int i=0;translators[i];i++)
	{
		char * ret=translators[i](from, to, text);
		if (ret) return ret;
	}
	
	return NULL;
}
