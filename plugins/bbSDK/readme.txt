  ----------------------------------------------------------------------

  bbSDK - Example plugin for Blackbox for Windows.
  Copyright © 2004,2009 grischka

  This program is free software, released under the GNU General Public
  License (GPL version 2). See:

  http://www.fsf.org/licenses/gpl.html

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.


  Description:
  ------------
  This is an example Plugin for Blackbox for Windows. It displays
  a little stylized window with an inscription.

  Left mouse:
    - with the control key held down: moves the plugin
    - with alt key held down: resizes the plugin

  Right mouse click:
    - shows the plugin menu with some standard plugin configuration
      options. Also the inscription text can be set.

  bbSDK is compatible with all current bb4win versions:
    - bblean 1.12 or later
    - xoblite bb2 or later
    - bb4win 0.90 or later


  Files:
  ======

  # -- common --
  bbSDK.cpp                 Source code (works as C or C++)
  BBApi.h                   bbLean API header file
  readme.txt                This file
  bbSDK.dll                 The compiled plugin

  # -- MINGW --
  makefile-gcc              makefile for gnu make
  libblackbox.a             blackbox import library for mingw
  libblackbox.def           blackbox import definition file
  bbSDK.dev                 DevCpp project file
  bbSDK.cbp                 CodeBlocks project file

  # -- MSC --
  makefile-msc              makefile for MSC nmake
  blackbox.lib              blackbox import library for MSVC
  bbSDK.vcproj              VC8(Express) project file

  # -- BCC55 --
  makefile-bcc              makefile for borland free compiler
  blackbox_bor.lib          blackbox import library for BCC
  bbSDK.def                 export definition file


  Using the included makefiles:
  =============================

  Make shure the compiler's bin directory is on the PATH, then run make
  in the bbSDK directory:

  with gcc:
    make -f makefile-gcc

  with msc:
    nmake -f makefile-msc

  with bcc:
    make -f makefile-bcc


  Using IDEs:
  ===========
  Project files are included for
  - DevCpp
  - CodeBlocks
  - MS VC8 Express Edition.


  Free compilers:
  ===============
  MinGW:
    http://www.mingw.org/

  MS VC8 Express Edition:
    http://msdn.microsoft.com/vstudio/express/visualc/download/

  Borland free commandline compiler:
    http://www.borland.com/products/downloads/download_cbuilder.html


  History:
  ========

  07 Jan 2009 - bbSDK 0.2
  -----------------------
  - Replaced all "bbSDK" strings by macros such that they can be
    changed more easily.
  - Function 'edit_rc(file)' works also under xoblite
  - Now also works as plain C source

  01 Aug 2004 - bbSDK 0.1
  -----------------------
  - initial release


  Enjoy!
  --- grischka

