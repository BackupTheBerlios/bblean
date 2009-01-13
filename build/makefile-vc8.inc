# --------------------------------------------------------------------
# for MS VC8 compiler

PROGRAMFILES ?= $(ProgramFiles)
VC8_DIR := $(PROGRAMFILES)\Microsoft Visual Studio 8\VC
PSDK    := $(PROGRAMFILES)\Microsoft Platform SDK for Windows Server 2003 R2
PATH    := $(VC8_DIR)\bin;$(VC8_DIR)\..\Common7\IDE;$(PATH)
LIB     := $(VC8_DIR)\lib;$(PSDK)\lib
INCLUDE := $(VC8_DIR)\include;$(PSDK)\include

export LIB
export INCLUDE
export PATH

DEFINES += -DVC8
_CFLAGS = /Zi /MD /wd4996
_CPPFLAGS = /EHsc

ifndef NO_MT
MANIFEST_CMD.dll = if exist $@.manifest mt /nologo /manifest $@.manifest /outputresource:$@;2
MANIFEST_CMD.exe = if exist $@.manifest mt /nologo /manifest $@.manifest /outputresource:$@;1
endif

include $(TOP)/build/makefile-msc.inc

# --------------------------------------------------------------------
