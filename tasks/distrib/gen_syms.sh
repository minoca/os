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
##     gen_syms.sh
##
## Abstract:
##
##     This script generates the symbol files.
##
## Author:
##
##     Evan Green 29-Mar-2016
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

REVISION="0"
WORKING="$SRCROOT/symswork"
if [ -d "$WORKING" ]; then
    echo "Removing old $WORKING"
    rm -rf "$WORKING"
fi

mkdir "$WORKING"
OLDPWD="$PWD"
for arch in x86 x86q armv7; do
    BINROOT=$SRCROOT/${arch}dbg/bin
    if ! [ -d $BINROOT ] ; then
        continue
    fi

    if [ -r $BINROOT/build-revision ] ; then
        rev=`cat $BINROOT/build-revision`
        if expr "$rev" \> "$REVISION" ; then
            REVISION="$rev"
        fi
    fi

    cd "$BINROOT"
    FILES=`echo *`

    REMOVE='*.img
pagefile.sys
*.zip'

    for file in $REMOVE; do
        if [ -r $file ]; then
            FILES=`echo $FILES | sed s/$file//`
        fi
    done

    mkdir -p "$WORKING/$arch/"
    cp -pv -- $FILES "$WORKING/$arch/"
done

##
## Copy the debugger in as well.
##

cd "$SRCROOT/x86$DEBUG/tools/bin"
for file in debugui.exe debug.exe kexts.dll ; do
    cp -pv ./$file "$WORKING/"
done

cp -pv "$SRCROOT/os/include/minoca/debug/dbgext.h" "$WORKING/"

cd "$OLDPWD"
ARCHIVE="Minoca-Symbols-$REVISION.zip"
7za a -tzip -mx9 -mmt -mtc "$ARCHIVE" $WORKING/*
FILE_SIZE=`ls -l $ARCHIVE | \
    sed -n 's/[^ ]* *[^ ]* *[^ ]* *[^ ]* *\([0123456789]*\).*/\1/p'`

if test -n "$MBUILD_STEP_ID"; then
    python $SRCROOT/client.py --result "$ARCHIVE Size" integer "$FILE_SIZE"
    python $SRCROOT/client.py --upload schedule $ARCHIVE $ARCHIVE
    echo Uploaded file $ARCHIVE, size $FILE_SIZE
fi

echo "Removing $WORKING"
rm -rf "$WORKING"
echo "Done creating symbols."

