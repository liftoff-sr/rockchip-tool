rockchip-tools
==============

RockChip tools for RK29, RK30, and RK32 boards.

Converted to C++ from C to preserve my sanity.

Many fixes, afptool works better, especially on files it created.
For files it did not create, it is deficient in the area of understanding
UPDATE_PART::padded_size.

So do not expect afptool to be able to unpack update.img files created by other
tools at this time.  However, unless you get an error message, it probably worked.

