##
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     assemble_efi.sh
##
## Abstract:
##
##     This script assembles the EFI directory, which will eventually go on the
##     boot volume.
##
## Author:
##
##     Evan Green 9-Sep-2014
##
## Environment:
##
##     Build
##

mkdir -p EFI/BOOT
mkdir -p EFI/MINOCA

cp -p bootmefi.efi EFI/MINOCA/BOOTMEFI.EFI
if test "x$ARCH" = "xx86"; then
    cp bootmefi.efi EFI/BOOT/BOOTIA32.EFI

elif test "x$ARCH" = "xarmv7"; then
    cp bootmefi.efi EFI/BOOT/BOOTARM.EFI

elif test "x$ARCH" = "xarmv6"; then
    cp bootmefi.efi EFI/BOOT/BOOTARM.EFI

else
    echo "Unknown architecture '$ARCH'."
    exit 1
fi

