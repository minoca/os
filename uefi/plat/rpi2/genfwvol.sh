##
## Copyright (c) 2015 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     genfwvol.sh
##
## Abstract:
##
##     This script assembles the firmware volume.
##
## Author:
##
##     Chris Stevens 17-Mar-2015
##
## Environment:
##
##     Build with POSIX tools.
##

set -e

OUTPUT=rpi2fwv
WORK_DIR=${OUTPUT}_work

if test -z $OBJCOPY; then
  OBJCOPY="$1"
  if test -z $OBJCOPY; then
    echo "Error: OBJCOPY must be set."
    exit 2
  fi
fi

##
## Create the working directory.
##

cd ${SRCROOT}/${ARCH}${DEBUG}/bin
rm -rf $WORK_DIR
mkdir $WORK_DIR

##
## Generate the FFS files.
##

genffs -s -i rtbase -r EFI_SECTION_PE32 \
    -i rtbase -r EFI_SECTION_USER_INTERFACE -t EFI_FV_FILETYPE_DRIVER \
    -o $WORK_DIR/rtbase.ffs

genffs -s -i rpi2rt -r EFI_SECTION_PE32 \
    -i rpi2rt -r EFI_SECTION_USER_INTERFACE -t EFI_FV_FILETYPE_DRIVER \
    -o $WORK_DIR/rpi2rt.ffs

cp $SRCROOT/$ARCH$DEBUG/obj/os/uefi/plat/rpi2/acpi/acpi.ffs $WORK_DIR

##
## Generate the firmware volume.
##

echo Generating Firmware Volume - ${OUTPUT}
ORIGINAL_DIR=`pwd`
cd ${WORK_DIR}
genfv -o ${OUTPUT} rtbase.ffs rpi2rt.ffs acpi.ffs

##
## Generate the object file from the firmware volume.
##

${OBJCOPY} -B arm -I binary -O elf32-littlearm ${OUTPUT} ../${OUTPUT}.o

cd "$ORIGINAL_DIR"
rm -rf $WORK_DIR

