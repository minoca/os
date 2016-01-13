##
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     genfwvol.sh <objcopy>
##
## Abstract:
##
##     This script assembles the firmware volume.
##
## Author:
##
##     Evan Green 10-Mar-2014
##
## Environment:
##
##     Build with POSIX tools.
##

set -e

OUTPUT=biosfwv
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

genffs -s -i biosrt -r EFI_SECTION_PE32 \
    -i biosrt -r EFI_SECTION_USER_INTERFACE -t EFI_FV_FILETYPE_DRIVER \
    -o $WORK_DIR/biosrt.ffs

##
## Generate the firmware volume.
##

echo Generating Firmware Volume - ${OUTPUT}
ORIGINAL_DIR=`pwd`
cd ${WORK_DIR}
genfv -o ${OUTPUT} rtbase.ffs biosrt.ffs

##
## Generate the object file from the firmware volume.
##

${OBJCOPY} -B i386 -I binary -O elf32-i386 ${OUTPUT} ../${OUTPUT}.o

cd "$ORIGINAL_DIR"
rm -rf $WORK_DIR

