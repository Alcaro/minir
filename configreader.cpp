#include "configreader.h"
#include <ctype.h>

bool configreader::parse(char * data)
{
	this->items.reset();
	hashmap<string,sub_inner>* group=&this->items.get("").items;
	while (data)
	{
		char * nextline=strchr(data, '\n');
		if (nextline) { *nextline='\0'; nextline++; }
		
		while (isspace(*data)) data++;
		if (*data=='#' || *data==';' || *data=='\0') goto next;
		
		char * lineend;
		lineend=strchr(data, '\0');
		while (isspace(lineend[-1])) lineend--;
		*lineend='\0';
		
		if (*data=='[')
		{
			if (lineend[-1]==']' && (isalpha(data[1]) || data[1]=='_'))
			{
				lineend[-1]='\0';
				group=&this->items.get(data+1).items;
				puts(data+1);
			}
			else goto fail;
			goto next;
		}
		
		if (isalpha(*data) || *data=='_')
		{
			char * keyend=data;
			while (isalnum(*keyend) || *keyend=='_') keyend++;
			char * valstart=keyend;
			while (isspace(*valstart)) valstart++;
			if (*valstart!='=') goto fail;
			valstart++;
			while (isspace(*valstart)) valstart++;
			*keyend='\0';
			if (group) group->set(data, valstart);
			else goto fail;
			goto next;
		}
		
		goto fail;
		
	next:
		data=nextline;
	}
	
	set_group(NULL);
	return true;
	
fail:
	this->items.reset();
	this->group=NULL;
	return false;
}
