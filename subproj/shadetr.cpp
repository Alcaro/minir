#if 0
<<COMMENT1
//shadetr - a shader translator
//usage: shadetr source dest
//example: ./shadetr mcgreen.cg mcgreen.glsl
//[TODO] 'source' and 'dest' can be presets; if one is, the other must be too
COMMENT1

cd subproj || true
cd ..
g++ -Wno-unused-result subproj/shadetr.cpp \
	-DDYLIB_POSIX dylib.cpp -ldl -DWINDOW_MINIMAL -DWINDOW_MINIMAL_IMUTEX_DUMMY window-none.cpp \
	-DHAVE_CG_SHADERS video-shader-translate*.cpp \
	memory.cpp video.cpp -g -o shadetr
mv shadetr ~/bin

<<COMMENT2
#endif

#include "../io.h"
#include "../string.h"
#include "../file.h"
#include <stdio.h>

char * readfile(const char * filename)
{
	//return strdup("#error Q");
	return NULL;
}

int main(int argc, char * argv[])
{
	char* text;
	if (!file_read(argv[1], (void**)&text, NULL))
	{
		puts("Couldn't read file");
		return 1;
	}
	const char * srcext=strrchr(argv[1], '.');
	const char * dstext=strrchr(argv[2], '.');
	if (!srcext || !dstext)
	{
		puts("Couldn't determine file type");
		return 1;
	}
	//preset format: shaders=1\nshader0=%s
	char* new_text=video::shader::translate(video::shader::str_to_lang(srcext+1),
	                                        video::shader::str_to_lang(dstext+1),
	                                        text, bind(readfile));
	free(text);
	
	if (!new_text)
	{
		puts("Couldn't translate");
		return 1;
	}
	
puts(new_text);
	//if (!file_write(argv[2], 
}
#if 0
COMMENT2
#endif
