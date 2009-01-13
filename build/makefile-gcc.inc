# --------------------------------------------------------------------
# for MinGW compiler (tested with gcc-3.4.5)

USING_MINGW = 1

CC      = gcc
CP      = g++
WINDRES = windres.exe
CFLAGS  += -Os -Wall -fno-strict-aliasing
LFLAGS  += -s
SYSLIBS += -lstdc++

ifneq "$(SUBSYSTEM)" "CONSOLE"
LFLAGS += -mwindows
SYSLIBS += -lcomctl32 -lcomdlg32 -ladvapi32 -lversion -luuid -lole32 -lwinmm
endif

ifdef IMPLIB
LFLAGS += -Wl,--out-implib,lib$(basename $(notdir $@)).a
endif

COMPILE.c   = $(CC) $(CFLAGS) -o $@ -c $< $(DEFINES)
COMPILE.cpp = $(CP) $(CFLAGS) $(CPPFLAGS) -o $@ -c $< $(DEFINES)
COMPILE.rc  = $(WINDRES) -O coff -o $@ -i $< $(DEFINES)

LINK.exe    = $(CC) $(LFLAGS) -o $@ $^ $(SYSLIBS)
LINK.dll    = $(CC) $(LFLAGS) -shared -o $@ $^ $(SYSLIBS)

define LINK.a
 $(call DEL,$@)
 ar rcs $@ $^
endef

LIBNAME = lib$1.a

# --------------------------------------------------------------------
