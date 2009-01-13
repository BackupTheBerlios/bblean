# --------------------------------------------------------------------
# common makefile.inc for bbLean core, plugins and tools

# choose compiler (one of gcc, msc, vc8, msc64)
# See also makefile-???.inc

COMPILER ?= gcc

# --------------------------------------------------------------------

# --------------------------------------------------------------------

BS      = "$(subst /,\,$1)"
BSNQ    = $(subst /,\,$1)
MAKE    := @$(call BS,$(MAKE))
COPY    = copy $(call BS,$1) $(call BS,$2)
MKDIR   = mkdir $(call BS,$@)
RM      = for %%f in ($(foreach F,$1,$(call BSNQ,$F))) do $(call DEL,%%f)

FUZZY   = $(call BS,$(TOP)/build/fuzzydoc/fuzzydoc.exe)
INST    = $(call BSNQ,$(TOP)/build/inst.exe)

ifeq "$(OS)" "Windows_NT"
 DIR     = @dir $(call BS,$@) | find $(call BS,$@)
 SHELL   = cmd
 DEL     = @if exist $1 (echo -- removing $1 && del $1)
else
 DIR     = @dir $(call BSNQ,$@)
 SHELL   = command.com
 DEL     = if exist $1 del $1
endif

# --------------------------------------------------------------------

# --------------------------------------------------------------------

all : all.progs

clean : clean.progs clean.stuff

install : install.progs

all.progs clean.progs install.progs :

clean.stuff :
	$(call RM,$(CLEAN_FILES))

.PHONY : all clean install

.SUFFIXES : .exe .dll .progs

%.obj: %.c
	$(COMPILE.c)

%.obj: %.cpp
	$(COMPILE.cpp)

%.res: %.rc
	$(COMPILE.rc)

# --------------------------------------------------------------------

# --------------------------------------------------------------------

include $(TOP)/build/makefile-$(COMPILER).inc

# --------------------------------------------------------------------

# --------------------------------------------------------------------
ifneq "$(BIN)" ""

all : $(BIN)

BBAPI = $(TOP)/blackbox
DEFINES += -I$(BBAPI)
TYPE = $(subst .,,$(suffix $(BIN)))

ifeq "$(SUBSYSTEM)" "CONSOLE"
NO_BBLIB ?= 1
endif

ifeq "$(TYPE)" "lib"
NO_BBLIB ?= 1
endif

ifneq "$(NO_BBLIB)" "1"
  DEFINES += -I$(TOP)/lib
  LIBS += $(TOP)/lib/$(call LIBNAME,bblib)
endif

ifeq "$(TYPE)" "dll"
  VPATH += $(TOP)/plugins/bbPlugin
  DEFINES += -I$(TOP)/plugins/bbPlugin
ifneq "$(NO_BBLIB)" "1"
  LIBS += $(BBAPI)/$(call LIBNAME,blackbox)
endif
endif

$(BIN): $(OBJ) $(LIBS)
	$(LINK.$(TYPE))
	@echo ---------------------------------------------------------------
	$(DIR)
	@echo ---------------------------------------------------------------

CLEAN_FILES += $(BIN) $(BIN).manifest
CLEAN_FILES += *.obj *.res *.exp *.lib *.a *.pdb *.tds lmakestate

endif
# --------------------------------------------------------------------

# --------------------------------------------------------------------
ifneq "$(INSTDIR)" ""

all :
	@$(INST) "$(INSTDIR)" $(INSTALL_FILES) -- -keep $(INSTALL_IF_NEW)

endif
# --------------------------------------------------------------------
