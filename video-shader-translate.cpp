#include "io.h"

char * video::shader::translate(lang_t from, lang_t to, const char * text,
                                function<char*(const char * filename)> get_include)
{
	if (from==la_none || to==la_none) return NULL;
	
	static char * (* const translators[])(lang_t from, lang_t to, const char * text,
	                                      function<char*(const char * filename)> get_include) = {
#ifdef HAVE_CG_SHADERS
		translate_cgc,
#endif
		NULL
	};
	for (unsigned int i=0;translators[i];i++)
	{
		char * ret=translators[i](from, to, text, get_include);
		if (ret) return ret;
	}
	
	if (from==to) return strdup(text);
	
	return NULL;
}
