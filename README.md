rockchip-tools
==============

RockChip tools for RK29, RK30, and RK32 boards.


5-Jan-2016
==========
Fixed all known bugs.  File an issue if you find any.
Added the download_images.sh script.

4-Jan-2016
==========
The afptools -CMDLINE option works better, getting a better understanding of requirements.
Will spend more time on it soon after reading the uboot rockchip help.


2-Jan-2016
==========
Fixes, improved comments, and defensive programming additions.


1-Jan-2016
==========
Converted to C++ from C to preserve my sanity.  I don't understand C.
That is, I don't understand why someone would use it for tools like this.

Many fixes, afptool works better, especially on files it created.  It also has
a new -CMDLINE mode which outputs the CMDLINE fragment for the partition table setup
in the parameter file.

Better error messages.

CMake build system, removed Makefile.

