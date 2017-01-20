##
## Copyright (c) 2016 Minoca Corp.
##
##    This file is licensed under the terms of the GNU General Public License
##    version 3. Alternative licensing terms are available. Contact
##    info@minocacorp.com for details. See the LICENSE file at the root of this
##    project for complete licensing information..
##
## Script Name:
##
##     gen_plats.sh
##
## Abstract:
##
##     This script generates the platform zip files.
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

if test -z "$SRCROOT"; then
    echo "Error: SRCROOT must be set."
    exit 1
fi

if test -z "$ARCH"; then
    echo "Error: ARCH must be set."
    exit 1
fi

BINROOT=$SRCROOT/${ARCH}${VARIANT}dbg/bin
if ! test -d $BINROOT; then
    echo "Error: BINROOT '$BINROOT' does not exist."
    exit 1
fi

if ! test -f $BINROOT/build-revision; then
    echo "Error: '$BINROOT/build-revision' is missing."
    exit 1
fi

REVISION=`cat $BINROOT/build-revision`
if test -z "$REVISION"; then
    echo "Error: $BINROOT/build-revision is empty."
    exit 1
fi

cd $BINROOT
REVISION=`cat $BINROOT/build-revision`
if test -z "$REVISION"; then
    echo "Error: $BINROOT/build-revision is empty."
    exit 1
fi

cd $BINROOT/distribute
if [ "$ARCH" = "x86" ] ; then
    if [ "x$VARIANT" = "xq" ] ; then
        IMAGES="galileo"
    else
        IMAGES="pc pcefi"
    fi

elif [ "$ARCH" = "armv7" ] ; then
    IMAGES="bbone panda rpi2 veyron"

elif [ "$ARCH" = "armv6" ] ; then
    IMAGES="rpi"

else
    echo "Error: Unknown architecture $ARCH."
    exit 1
fi

for image in $IMAGES install; do
    ARCHIVE="Minoca-$image.zip"
    [ "$image" = "install" ] && ARCHIVE="Minoca-$image-$ARCH$VARIANT.zip"
    7za a -tzip -mmt -mx9 -mtc "$ARCHIVE" "${image}.img"
    FILE_SIZE=`ls -l $ARCHIVE | \
        sed -n 's/[^ ]* *[^ ]* *[^ ]* *[^ ]* *\([0123456789]*\).*/\1/p'`

    if test -n "$MBUILD_STEP_ID"; then
        python $SRCROOT/client.py --result "$image Size" integer "$FILE_SIZE"
        python $SRCROOT/client.py --upload schedule $ARCHIVE $ARCHIVE
        echo Uploaded file $ARCHIVE, size $FILE_SIZE
        upload_to_production "$ARCHIVE"
    fi
done

echo "Done generating platform archives."

