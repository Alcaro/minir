#include "minir.h"

char * video::shader::translate(lang_t from, lang_t to, const char * text)
{
	if (from==to) return strdup(text);
	return NULL;
}
