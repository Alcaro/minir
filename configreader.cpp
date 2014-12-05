#include "containers.h"
#include <ctype.h>

void config::parse(char * data)
{
	assocarr<string>* group=&this->items.get("");
	this->group=group;
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
				group=&this->items.get(data+1);
				puts(data+1);
			}
			else group=NULL;//error
			goto next;
		}
		
		if (isalpha(*data) || *data=='_')
		{
			char * keyend=data;
			while (isalnum(*keyend) || *keyend=='_') keyend++;
			char * valstart=keyend;
			while (isspace(*valstart)) valstart++;
			if (*valstart!='=') goto next;//error
			valstart++;
			while (isspace(*valstart)) valstart++;
			*keyend='\0';
			if (group) group->set(data, valstart);
			//else error
			goto next;
		}
		
		goto next;//error
		
	next:
		data=nextline;
	}
	
	set_group(NULL);
}
