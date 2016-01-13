##
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     assemble_common.sh
##
## Abstract:
##
##     This script performs common assembly steps for most build images.
##
## Author:
##
##     Evan Green 9-Sep-2014
##
## Environment:
##
##     Build.
##

set -e

if test "x$DRIVERS" = "x"; then
    echo "Error: DRIVERS not set."
    exit 1
fi

if test "x$CONFIG_FILES" = "x"; then
    echo "Error: CONFIG_FILES not set."
    exit 1
fi

if test "x$SYSTEM_FILES" = "x"; then
    echo "Error: SYSTEM_FILES not set."
    exit 1
fi

if test "x$IMAGE_FILES" = "x"; then
    echo "Error: IMAGE_FILES not set."
    exit 1
fi

if test "x$LIB_FILES" = "x"; then
    echo "Error: LIB_FILES not set."
    exit 1
fi

ROOT_DIRECTORY="minoca"
DRIVERS_DIRECTORY="$ROOT_DIRECTORY/drivers"
CONFIG_DIRECTORY="$ROOT_DIRECTORY/config"
SYSTEM_DIRECTORY="$ROOT_DIRECTORY/system"
SYSTEM_PCAT_DIRECTORY="$SYSTEM_DIRECTORY/pcat"
LIB_DIRECTORY="apps/lib"
BIN_DIRECTORY="apps/bin"

if test "x$STRIP" = "x"; then
    echo "Error: STRIP must be set."
    exit 1
fi

STRIP="$STRIP -gp --strip-unneeded"

mkdir -p $DRIVERS_DIRECTORY
for file in $DRIVERS; do
    $STRIP -o $DRIVERS_DIRECTORY/$file $file
done

mkdir -p $CONFIG_DIRECTORY
for file in $CONFIG_FILES; do
    cp -p $file $CONFIG_DIRECTORY/$file
done

echo "$BOOT_DRIVERS" > $CONFIG_DIRECTORY/$BOOT_DRIVERS_FILE

mkdir -p $SYSTEM_DIRECTORY
for file in $SYSTEM_FILES; do

    ##
    ## Avoid trying to strip EFI executables.
    ##

    if test "x${file%%.efi}" != "x$file"; then
        cp -p $file $SYSTEM_DIRECTORY/$file

    else
        $STRIP -o $SYSTEM_DIRECTORY/$file $file
    fi
done

if test -n "$PCAT_FILES"; then
    mkdir -p $SYSTEM_PCAT_DIRECTORY
    for file in $PCAT_FILES; do
        cp -p $file $SYSTEM_PCAT_DIRECTORY/$file
    done
fi

if test "x$ARCH" = "xx86"; then
    mkdir -p $SYSTEM_PCAT_DIRECTORY
    for file in $PCAT_FILES; do
        if test "x$file" = "xloader"; then
            $STRIP -o $SYSTEM_PCAT_DIRECTORY/$file $file
        else
            cp -p $file $SYSTEM_PCAT_DIRECTORY/$file
        fi
    done
fi

skel_dirs=`cd ./skel && find -type d`
for dir in $skel_dirs; do
    mkdir -p "./apps/$dir"
done

skel_files=`cd ./skel && find -type f`
for file in $skel_files; do
    if test "./skel/$file" -nt "./apps/$file"; then
        cp -p "./skel/$file" "./apps/$file"
    fi
done

if ! test -d "$LIB_DIRECTORY"; then
    IMAGE_FILES="$IMAGE_FILES $LIB_FILES"

else
    for file in $LIB_FILES; do
        cp -p $file $LIB_DIRECTORY/$file
    done
fi

if ! test -d "$BIN_DIRECTORY"; then
    IMAGE_FILES="$IMAGE_FILES $BIN_FILES"

else
    for file in $BIN_FILES; do
        cp -p $file $BIN_DIRECTORY/$file
    done
fi

CREATEIMAGE_DEBUG=
if test "x$DEBUG" = "xchk"; then
    CREATEIMAGE_DEBUG="-D0"
fi

