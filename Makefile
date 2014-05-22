CC = gcc
CFLAGS = -g -Wall -Werror
LD = gcc
LFLAGS =

ifeq ($(platform),)
  uname := $(shell uname -a)
  ifeq ($(uname),)
    platform := win
    delete = del $(subst /,\,$1)
    EXESUFFIX = .exe
  else ifneq ($(findstring Darwin,$(uname)),)
    platform := osx
    delete = rm -f $1
    EXESUFFIX =
  else
    platform := x
    delete = rm -f $1
    EXESUFFIX =
  endif
endif

TESTSRC = memory.c minircheats-model.c
TESTSEPSRC = test-*.c

OBJS = $(patsubst %.c,obj/%.o,$(wildcard *.c))
TESTOBJS = $(patsubst %.c,obj/%.o,$(wildcard $(TESTSRC))) $(patsubst %.c,obj/%-test.o,$(wildcard $(TESTSEPSRC)))

all: minir

#Rules for which dependencies are allowed:
#If there is no reasonable way to continue the program without this library, then make use of it.
#If a library matches the above rule, no effort needs to be spent on getting rid of it from other locations.
#If the library is only a link-time dependency and not needed at runtime, then it's fine.
#Otherwise, effort shall be spent on getting rid of this dependency. It's fine temporarily, but don't leave it there forever.
ifeq ($(platform),x)
  TRUE_CFLAGS = $(CFLAGS) $(shell pkg-config --cflags gtk+-3.0 libpulse) -pthread
  TRUE_LFLAGS = $(LFLAGS) $(shell pkg-config --libs   gtk+-3.0 libpulse) -pthread -ldl -lX11 -lGL -lXi -lXext -lpulse
  #pthread - core (thread), links in threadsafe malloc
  #gtk+-3.0 - core (window)
  #libpulse - audio.pulseaudio - link only
  #dl - core (dylib)
  #X11 - core (libretro/hardware mapping), video.opengl, input.x11-xinput2? - FIXME
  #GL - video.opengl - FIXME
  #Xi - input.x11-xinput2 - FIXME
  #Xext - video.xshm - FIXME
  #pulse - audio.pulseaudio - FIXME
else ifeq ($(platform),osx)
  #can't do this
else ifeq ($(platform),win)
  TRUE_CFLAGS = $(CFLAGS) -Wno-format
  TRUE_LFLAGS = $(LFLAGS) -lgdi32 -lcomctl32 -lcomdlg32 -ldinput8 -ldxguid -lopengl32 -ldsound
  #gdi32 - core (window - fonts), video.gdi, video.opengl
  #comctl32 - core (window - inner)
  #comdlg32 - core (window - open file dialog)
  #dinput8 - input.directinput - link only
  #dxguid - audio.directsound, input.directinput - link only
  #opengl32 - video.opengl - FIXME
  #dsound - audio.directsound - FIXME
  RESOBJ = obj/resource.o
  OBJS += $(RESOBJ)
  TESTOBJS += $(RESOBJ)
obj/resource.o: ico/*
	windres ico/minir.rc obj/resource.o
else
  $(error Unknown platform.)
endif

TRUE_CFLAGS += -std=gnu99
TRUE_LFLAGS +=


#$(CC) $(TRUE_CFLAGS) -DTEST -DNO_ICON test*.c window*.c $(TRUE_LFLAGS) -otest $(RESOBJ)
test: $(TESTOBJS)
	$(LD) $+ $(TRUE_LFLAGS) -o $@

clean:
	rm obj/* || true

obj:
	mkdir obj

obj/config.o: config.c obj/config.c | obj
obj/main.o: main.c obj/config.c minir.h | obj
obj/%.o: %.c | obj obj/config.c
	$(CC) $(TRUE_CFLAGS) -c $< -o $@

obj/%-test.o: %.c | obj obj/config.c
	$(CC) $(TRUE_CFLAGS) -c $< -o $@ -DTEST -DNO_ICON

obj/config.c: obj/configgen$(EXESUFFIX) minir.cfg.tmpl
	obj/configgen$(EXESUFFIX)
obj/configgen$(EXESUFFIX): configgen.c
	$(CC) $(TRUE_CFLAGS) $(TRUE_LFLAGS) -DCONFIGGEN configgen.c -o obj/configgen$(EXESUFFIX)

minir: $(OBJS)
	$(LD) $+ $(TRUE_LFLAGS) -o $@ -lm
