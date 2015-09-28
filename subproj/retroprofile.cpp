#if 0
<<COMMENT1
//retroprofile - a headless libretro frontend, for performance testing and PGO compilation
//usage: retroprofile corepath rompath frames
//example: ./retroprofile roms/gambatte_libretro.so roms/zeldaseasons.gbc 10000
COMMENT1

cd subproj || true
cd ..
true -Os -s
g++ -g subproj/retroprofile.cpp \
	libretro.cpp dylib.cpp memory.cpp video.cpp file-posix.cpp -DFILEPATH_POSIX \
	-ldl -lrt -DDYLIB_POSIX -DWINDOW_MINIMAL -DWINDOW_MINIMAL_NO_THREAD window-none.cpp -o retroprofile
mv retroprofile ~/bin/retroprofiled

<<COMMENT2
#endif

#include "../window.h"
#include "../minir.h"
#include "../file.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char * argv[])
{
	if (!argv[1])
	{
		puts("usage: retroprofile <core> <rom> <frames>");
		return 1;
	}
	//window_init(&argc, &argv);
	unsigned int frames=atoi(argv[3]);
	
	uint64_t t_init=window_get_time();
	struct libretro * core=libretro::create(argv[1], NULL, NULL);
	if (!core)
	{
		perror(argv[1]);
		//puts("Couldn't load core.");
		return 1;
	}
	
	if (!core->load_rom(file::create(argv[2])))
	{
		perror(argv[2]);
		puts("Couldn't load ROM.");
		return 1;
	}
	
	uint64_t t_run=window_get_time();
	
	unsigned int fps=0;
	while (window_get_time() < t_run+1000000 && fps < frames)
	{
		core->run();
		fps++;
	}
	for (unsigned int i=fps;i<frames;i++)
	{
		if (i%fps==0) printf("%u/%u\r", i, frames),fflush(stdout);
		core->run();
	}
	uint64_t t_run_done=window_get_time();
	
	delete core;
	uint64_t t_end=window_get_time();
	
	printf("Time to load ROM: %luus\n", t_run-t_init);
	if (frames)
	{
		printf("Time per frame: %luus / ", (t_run_done-t_run)/frames);
		printf("Frames per second: %f\n", (float)frames*1000000/(t_run_done-t_run));
	}
	printf("Time to exit: %luus\n", t_end-t_run_done);
	printf("Total time: %luus\n", t_end-t_init);
}
#if 0
COMMENT2
#endif
