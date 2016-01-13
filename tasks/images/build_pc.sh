#! /bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     build_pc.sh
##
## Abstract:
##
##     This script creates the pc.img image.
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

OUTPUT=pc.img

if test -z "$TASKDIR"; then
    TASKDIR="$SRCROOT/os/tasks"
fi

. $TASKDIR/images/add_common.sh
. $TASKDIR/images/assemble_common.sh

##
## Unless overridden create an image that fits nicely on a 1GB memory stick.
##

if test -z "$CI_MIN_IMAGE_SIZE"; then
    export CI_MIN_IMAGE_SIZE=970
fi

createimage -p p:10M*:e,p: -b1 -n2 -aci $CREATEIMAGE_DEBUG -f vmdk \
    -o "$OUTPUT" -m mbr.bin -x fatboot.bin -y bootman.bin $IMAGE_FILES

echo "Created $OUTPUT"
rm -rf $ROOT_DIRECTORY EFI

