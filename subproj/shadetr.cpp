#if 0
<<COMMENT1
//shadetr - a shader translator
//usage: shadetr source dest
//example: ./shadetr mcgreen.cg mcgreen.glsl
//[TODO] 'source' and 'dest' can be presets; if one is, the other must be too
COMMENT1

cd subproj || true
cd ..
g++ -std=c++11 -Wno-unused-result subproj/shadetr.cpp \
	-DDYLIB_POSIX dylib.cpp -ldl \
	-DWINDOW_MINIMAL window-none.cpp \
	-DFILEPATH_POSIX window-native.cpp \
	-DHAVE_CG_SHADERS video-shader-translate*.cpp \
	memory.cpp video.cpp -g -o shadetr ||
	exit 1
mv shadetr ~/bin

<<COMMENT2
#endif

#include "../io.h"
#include "../string.h"
#include "../file.h"
#include "../window.h"
#include <stdio.h>

const char * shaderpath;

char * readfile(const char * filename)
{
	char * path=window_get_absolute_path(shaderpath, filename, false);
	if (!path) return NULL;
	char* ret;
	file_read(path, (void**)&ret, NULL);
	free(path);
	return ret;
}

char * convert_one(const char * filename, video::shader::lang_t target)
{
	shaderpath=filename;
	
	char* text;
	if (!file_read(filename, (void**)&text, NULL))
	{
		puts("Couldn't read file");
		exit(1);
	}
	
	const char * srcext=strrchr(filename, '.');
	if (!srcext)
	{
		puts("Couldn't determine source file type");
		exit(1);
	}
	char* new_text=video::shader::translate(video::shader::str_to_lang(srcext+1),
	                                        target,
	                                        text, bind(readfile));
	free(text);
	
	if (!new_text)
	{
		puts("Couldn't translate");
		exit(1);
	}
	return new_text;
}

int main(int argc, char * argv[])
{
	window_init(&argc, &argv);
	const char * dstext=strrchr(argv[2], '.');
	if (!dstext)
	{
		puts("Couldn't determine destination file type");
		exit(1);
	}
//preset format: shaders=1\nshader0=%s
char* new_text=convert_one(window_get_absolute_path(window_get_cwd(), argv[1], true), video::shader::str_to_lang(dstext+1));
puts(new_text);
	//if (!file_write(argv[2], 
}
#if 0
COMMENT2
#endif
