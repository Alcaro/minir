@echo off

echo Checking whether build environment is sane...

echo WIN_LIB =  -lgdi32     > config.mk
echo WIN_LIB += -lcomctl32 >> config.mk
echo WIN_LIB += -lcomdlg32 >> config.mk
echo WIN_LIB += -ldinput8  >> config.mk
echo WIN_LIB += -ldxguid   >> config.mk
echo WIN_LIB += -lopengl32 >> config.mk
echo CONF_LFLAGS = $(WIN_LIB)              >> config.mk
echo EXESUFFIX = .exe                      >> config.mk
echo EXTRAOBJ = obj/resource$(OBJSUFFIX).o >> config.mk
echo RC = windres                          >> config.mk
echo RCFLAGS =                             >> config.mk
echo obj/resource$(OBJSUFFIX).o: ico/*     >> config.mk
echo 	$(RC) $(RCFLAGS) ico/minir.rc obj/resource$(OBJSUFFIX).o >> config.mk

::some people just refuse to leave their XPs.
ver | find "5.1" > nul && echo CONF_CFLAGS = -D_WIN32_WINNT=0x0501 >> config.mk

ping -n 1 127.0.0.1 > nul
echo ...it's a Windows, so probably not, but configuration is done anyways.