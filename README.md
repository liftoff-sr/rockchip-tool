rockchip-tools
==============

RockChip tools for RK29, RK30, and RK32 boards.

1-Jan-2016
==========
Converted to C++ from C to preserve my sanity.  I don't understand C.
That is, I don't understand why someone would use it for tools like this.

Many fixes, afptool works better, especially on files it created.  It also has
a new -CMDLINE mode which outputs the CMDLINE fragment for the partition table setup
in the parameter file.

Better error messages.

CMake build system, removed Makefile.

