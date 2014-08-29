echo WIN_LIB = -lgdi32  > Makefile.custom
echo WIN_LIB += -lcomctl32 >> Makefile.custom
echo WIN_LIB += -lcomdlg32 >> Makefile.custom
echo WIN_LIB += -ldinput8  >> Makefile.custom
echo WIN_LIB += -ldxguid   >> Makefile.custom
echo WIN_LIB += -lopengl32 >> Makefile.custom
echo WIN_LIB += -lopengl32 >> Makefile.custom
echo TRUE_CFLAGS = $(CFLAGS)               >> Makefile.custom
echo TRUE_CXXFLAGS = $(CXXFLAGS)           >> Makefile.custom
echo TRUE_LFLAGS = $(LFLAGS) $(WIN_LIB)    >> Makefile.custom
echo EXESUFFIX = .exe                      >> Makefile.custom
echo EXTRAOBJ = obj/resource$(OBJSUFFIX).o >> Makefile.custom
echo RC = windres                          >> Makefile.custom
echo RCFLAGS =                             >> Makefile.custom
echo obj/resource$(OBJSUFFIX).o: ico/*     >> Makefile.custom
echo 	$(RC) $(RCFLAGS) ico/minir.rc obj/resource$(OBJSUFFIX).o >> Makefile.custom
