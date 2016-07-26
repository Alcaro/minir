all: minir_dummy

CC = gcc
CFLAGS =
CXX = g++
CXXFLAGS = $(CFLAGS)
LD = g++
LFLAGS =
OBJSUFFIX =

EXESUFFIX =
EXTRAOBJ =
CONF_CXXFLAGS = $(CONF_CFLAGS)

#Native toolchain, aka not cross compiler. Used because parts of minir are autogenerated, and the generator is also written in C++.
#The generated file doesn't change between platforms.
NATCC = $(CC)
NATCFLAGS = $(CFLAGS)
NATCXX = $(CXX)
NATCXXFLAGS = $(CXXFLAGS)
NATLD = $(LD)
NATLFLAGS = $(LFLAGS)

include config$(PROFILE).mk

OUTNAME = minir$(EXESUFFIX)

OBJS = $(patsubst %.cpp,obj/%$(OBJSUFFIX).o,$(wildcard *.cpp)) $(EXTRAOBJ) obj/miniz$(OBJSUFFIX).o

#-Wno-comment is because GCC hates code such as
#//define foo(a,b,c) \
#//  bar(a) \
#//  bar(b) \
#//  bar(c)
#which is perfectly fine and GCC is just being stupid.
TRUE_CFLAGS = $(CFLAGS) $(CONF_CFLAGS) -fvisibility=hidden -fno-exceptions -std=c99 -Wno-comment
TRUE_CXXFLAGS = $(CXXFLAGS) $(CONF_CXXFLAGS) -fvisibility=hidden -fno-exceptions -fno-rtti -std=c++98 -Wno-comment
TRUE_LFLAGS = $(LFLAGS) -fvisibility=hidden $(CONF_LFLAGS)


#On Windows, cleaning up the object directory is expected to be done with 'del /q obj\*' in a batch script.
clean:
	rm obj/* || true

obj:
	mkdir obj

obj/miniz$(OBJSUFFIX).o: miniz.c | obj
	$(CC) $(TRUE_CFLAGS) -c $< -o $@

obj/config$(OBJSUFFIX).o: config.cpp obj/generated.cpp | obj
obj/main$(OBJSUFFIX).o: main.cpp obj/generated.cpp *.h | obj
obj/%$(OBJSUFFIX).o: %.cpp | obj obj/generated.cpp
	$(CXX) $(TRUE_CXXFLAGS) -c $< -o $@

obj/generated.cpp: obj/rescompile$(EXESUFFIX) minir.cfg.tmpl
	obj/rescompile$(EXESUFFIX)
obj/rescompile$(EXESUFFIX): rescompile.cpp miniz.c | obj
	$(NATCXX) $(NATCXXFLAGS) $(NATLFLAGS) -DRESCOMPILE -std=c++98 rescompile.cpp miniz.c -o obj/rescompile$(EXESUFFIX)

$(OUTNAME): $(OBJS)
	$(LD) $+ $(TRUE_LFLAGS) -o $@ -lm

minir_dummy: $(OUTNAME)
