#! /bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     build_install.sh
##
## Abstract:
##
##     This script creates the install.img image.
##
## Author:
##
##     Evan Green 9-Sep-2014
##
## Environment:
##
##     Build
##

set -e

OUTPUT=install.img

if test -z "$TASKDIR"; then
    TASKDIR="$SRCROOT/os/tasks"
fi

. $TASKDIR/images/add_common.sh

##
## Add the architecture specific ARM drivers.
##

ARM_IMAGE=no
case "$ARCH" in
x86)
    ;;

armv7)
    ARM_IMAGE=yes
    DRIVERS="$DRIVERS
sdomap4.drv
am3eth.drv"

    BOOT_DRIVERS="$BOOT_DRIVERS
ehci.drv
sdomap4.drv"

    ;;

armv6)
    ARM_IMAGE=yes
    ;;

*)
    echo "$0: Error: Unknown architecture"
    exit 1
esac

##
## Add the remaining ARM drivers if this is ARM.
##

if test $ARM_IMAGE = yes; then
    DRIVERS="$DRIVERS
dwhci.drv
pl050.drv
ramdisk.drv
smsc91c1.drv
smsc95xx.drv
sdbm2709.drv
rk32gpio.drv
rk32spi.drv
sdrk32xx.drv
am3i2c.drv
tps65217.drv
am3soc.drv"

    BOOT_DRIVERS="$BOOT_DRIVERS
dwhci.drv
ramdisk.drv
usbcomp.drv
usbhub.drv
usbmass.drv
sd.drv
sdbm2709.drv
sdrk32xx.drv
sdomap4.drv
am3soc.drv"

fi

. $TASKDIR/images/assemble_common.sh

##
## Add the ARM firmware binaries if needed.
##

if test "x$ARCH" = "xarmv7"; then
    PANDA_DIRECTORY="$SYSTEM_DIRECTORY/panda"
    mkdir -p "$PANDA_DIRECTORY"
    cp -p pandafw "$PANDA_DIRECTORY/pandafw"
    cp -p omap4mlo "$PANDA_DIRECTORY/omap4mlo"

    BEAGLE_BONE_DIRECTORY="$SYSTEM_DIRECTORY/bbone"
    mkdir -p "$BEAGLE_BONE_DIRECTORY"
    cp -p bbonefw "$BEAGLE_BONE_DIRECTORY/bbonefw"
    cp -p bbonemlo "$BEAGLE_BONE_DIRECTORY/bbonemlo"

    RPI_DIRECTORY="$SYSTEM_DIRECTORY/rpi2"
    RPI_CONFIG_DIRECTORY="$SRCROOT/os/kernel/config/rpi"
    mkdir -p "$RPI_DIRECTORY"
    cp -p rpifw "$RPI_DIRECTORY/rpifw"
    cp -p "$RPI_CONFIG_DIRECTORY/config.txt" "$RPI_DIRECTORY/config.txt"
    cp -p "$RPI_CONFIG_DIRECTORY/start.elf" "$RPI_DIRECTORY/start.elf"
    cp -p "$RPI_CONFIG_DIRECTORY/fixup.dat" "$RPI_DIRECTORY/fixup.dat"
    cp -p "$RPI_CONFIG_DIRECTORY/bootcode.bin" "$RPI_DIRECTORY/bootcode.bin"

elif test "x$ARCH" = "xarmv6"; then
    RPI_DIRECTORY="$SYSTEM_DIRECTORY/rpi"
    RPI_CONFIG_DIRECTORY="$SRCROOT/os/kernel/config/rpi"
    mkdir -p "$RPI_DIRECTORY"
    cp -p rpifw "$RPI_DIRECTORY/rpifw"
    cp -p "$RPI_CONFIG_DIRECTORY/config.txt" "$RPI_DIRECTORY/config.txt"
    cp -p "$RPI_CONFIG_DIRECTORY/start.elf" "$RPI_DIRECTORY/start.elf"
    cp -p "$RPI_CONFIG_DIRECTORY/fixup.dat" "$RPI_DIRECTORY/fixup.dat"
    cp -p "$RPI_CONFIG_DIRECTORY/bootcode.bin" "$RPI_DIRECTORY/bootcode.bin"
fi

createimage -aci $CREATEIMAGE_DEBUG -f flat -o "$OUTPUT" $IMAGE_FILES
echo "Created $OUTPUT"
rm -rf $ROOT_DIRECTORY EFI

