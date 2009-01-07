# --------------------------------------------------------------------
# for MS IA64 compiler from PSDK

PROGRAMFILES ?= $(ProgramFiles)
PSDK    := $(PROGRAMFILES)\Microsoft Platform SDK for Windows Server 2003 R2
PATH    := $(PSDK)\bin\win64;$(PSDK)\bin;$(PATH)
INCLUDE := $(PSDK)\Include;$(PSDK)\Include\crt
LIB     := $(PSDK)\Lib\IA64
BB64    = 1

export LIB
export INCLUDE
export PATH

_CFLAGS = /Zi /MD /GS- /wd4244 /wd4267 -D_WIN64 -DWIN64
_CPPFLAGS = /EHsc

include $(TOP)/build/makefile-msc.inc

# --------------------------------------------------------------------
