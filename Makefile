all: minir_dummy

CC = gcc
CFLAGS = -g
LD = gcc
LFLAGS =

#Stuff needed for Windows goes here; there is no Makefile.custom on Windows.
TRUE_CFLAGS = $(CFLAGS)
TRUE_LFLAGS = $(LFLAGS) -lgdi32 -lcomctl32 -lcomdlg32 -ldinput8 -ldxguid -lopengl32 -ldsound
EXESUFFIX = .exe
EXTRAOBJ = obj/resource.o
obj/resource.o: ico/*
	windres ico/minir.rc obj/resource.o

-include Makefile.custom

TESTSRC = memory.c thread-*.c window-*.c
TESTSEPSRC = test-*.c

OBJS = $(patsubst %.c,obj/%.o,$(wildcard *.c)) $(EXTRAOBJ)
TESTOBJS = $(patsubst %.c,obj/%.o,$(wildcard $(TESTSRC))) $(patsubst %.c,obj/%-test.o,$(wildcard $(TESTSEPSRC))) $(EXTRAOBJ)

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
obj/configgen$(EXESUFFIX): configgen.c miniz.c
	$(CC) $(TRUE_CFLAGS) $(TRUE_LFLAGS) -DCONFIGGEN configgen.c miniz.c -o obj/configgen$(EXESUFFIX)

minir$(EXESUFFIX): $(OBJS)
	$(LD) $+ $(TRUE_LFLAGS) -o $@ -lm

minir_dummy: minir$(EXESUFFIX)
