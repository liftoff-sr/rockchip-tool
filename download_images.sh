
# A bash script to program a Nagrace NTV6 box with simple Linux, no Android
# Copyright (C) 2016 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>


AFPTOOL="build-rockchip-tools/afptool"

UBOOT="/svn/u-boot-rockchip/RK3288UbootLoader_V2.19.01.bin"

IMG_BASE="HPH-RK3288_20140924/rockdev"

BOOT_IMG="$IMG_BASE/Image/boot-linux.img"

RFS_IMG="$IMG_BASE/Image/rootfs-linux.img"

RESOURCE_IMG="$IMG_BASE/Image/resource.img"

PARAMETER=parameter.hph



package_file_tmp="\
# NAME          Relative path
#============   ==================
boot            $BOOT_IMG
resource        $RESOURCE_IMG
swap            RESERVED
linuxroot       $RFS_IMG
"

ROOT_PARTITION=4    # which /dev/mmcblk0 partition no. is "linuxroot" file system


parameter_tmp="\
FIRMWARE_VER:1.0.0
MACHINE_MODEL:NTV6
MACHINE_ID:007
MANUFACTURER:Nagrace
MAGIC: 0x5041524B
ATAG: 0x60000800
MACHINE: 3288
CHECK_MASK: 0x80
PWR_HLD: 0,0,A,0,1
#KERNEL_IMG: 0x62008000
#FDT_NAME: rk-kernel.dtb
#RECOVER_KEY: 1,1,0,20,0
CMDLINE:console=ttyFIQ0,115200 root=/dev/mmcblk0p${ROOT_PARTITION} rw rootfstype=ext4 init=/sbin/init initrd=0x62000000,0x00800000 "


FLASHER=Linux_Upgrade_Tool_v1.21/upgrade_tool


echo -n "$package_file_tmp" > package-file

echo -n "$parameter_tmp" > $PARAMETER

$AFPTOOL -CMDLINE . >> $PARAMETER

$FLASHER UL $UBOOT

echo

$FLASHER DI -p $PARAMETER

echo

$FLASHER DI resource $RESOURCE_IMG

echo

$FLASHER DI -b $BOOT_IMG

echo

$FLASHER DI linuxroot $RFS_IMG

exit
