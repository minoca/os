#!/bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     package_distrib.sh
##
## Abstract:
##
##     This script creates the final distributable archives based on the latest
##     build.
##
## Author:
##
##     Evan Green 10-Nov-2014
##
## Environment:
##
##     Minoca (Windows) Build
##

set -e

SAVE_IFS="$IFS"
IFS='
'

export SRCROOT=`echo $SRCROOT | sed 's_\\\\_/_g'`
IFS="$SAVE_IFS"
unset SAVE_IFS

if test -z $SRCROOT; then
    echo "SRCROOT must be set."
    exit 1
fi

if test -z $ARCH; then
    echo "ARCH must be set."
    exit 1
fi

if test -z $DEBUG; then
    echo "DEBUG must be set."
    exit 1
fi

export TMPDIR=$PWD
export TEMP=$TMPDIR
OUTROOT="$SRCROOT/$ARCH$VARIANT$DEBUG"
export PATH="$SRCROOT/tools/win32/mingw/bin;$OUTROOT/tools/bin;$OUTROOT/testbin;$SRCROOT/tools/win32/scripts;$SRCROOT/tools/win32/swiss;$SRCROOT/tools/win32/bin;$SRCROOT/tools/win32/ppython/app;$SRCROOT/tools/win32/ppython/App/Scripts;$PATH"

##
## Download the latest build.
##

if ! test -d ./bin; then
    barch=$ARCH$VARIANT
    last_native_build=`python ../../client.py --query "Native Pilot $barch"`
    if test -z $last_native_build; then
      echo "Error: Failed to get last Native Pilot $barch build."
      exit 1
    fi

    file=minoca-bin-$ARCH$VARIANT$DEBUG.tar.gz
    echo "Downloading $file from schedule instance ID $last_native_build"
    python ../../client.py --pull $file $file $last_native_build
    echo "Extracting $file"
    tar -xzf $file || echo "Ignoring failures from tar."
fi

if ! test -d ./bin; then
    ls -la
    pwd
    echo "Error: missing bin directory."
    exit 1
fi

##
## Copy the original bin directory.
##

if ! test -d "$OUTROOT"; then
    mkdir -p "$OUTROOT"
fi

BINROOT="$OUTROOT/bin"
SAVED_BINROOT="$OUTROOT/bin.orig"
if test -d "$BINROOT"; then
    if test -d "$SAVED_BINROOT"; then
       echo "Removing previous $SAVED_BINROOT."
       rm -rf "$SAVED_BINROOT"
    fi

    echo "Moving original binroot to $SAVED_BINROOT"
    mv -f "$BINROOT" "$SAVED_BINROOT"
fi

##
## Move the extracted binroot into place.
##

mv ./bin "$BINROOT"

##
## Copy the debugger files from x86.
##

DEBUGROOT="$SRCROOT/x86$DEBUG/tools/bin"
for file in debugui.exe debug.exe kexts.dll dbgext.a msetup.exe; do
    cp -v $DEBUGROOT/$file  "$BINROOT/"
done

##
## Build the distribution files.
##

ORIGINAL_DIRECTORY=`pwd`
cd "$BINROOT"
if test "$ARCH$VARIANT" = "x86"; then
    echo "Running gen_bin.sh"
    sh "$SRCROOT/os/tasks/distrib/gen_bin.sh"
    echo "Running gen_sdk.sh"
    sh "$SRCROOT/os/tasks/distrib/gen_sdk.sh"
    echo "Running gen_tp.sh"
    sh "$SRCROOT/os/tasks/distrib/gen_tp.sh"
fi

echo "Running gen_plats.sh"
sh "$SRCROOT/os/tasks/distrib/gen_plats.sh"
echo "Running gen_syms.sh"
sh "$SRCROOT/os/tasks/distrib/gen_syms.sh"
echo "Running gen_inst.sh"
sh "$SRCROOT/os/tasks/distrib/gen_inst.sh"

##
## Upload the files.
##

if test "$ARCH$VARIANT" = "x86"; then
    REVISION=`cat $BINROOT/build-revision`
    file=MinocaOS-Starter-$REVISION.zip
    file_size=`ls -l $file | \
        sed -n 's/[^ ]* *[^ ]* *[^ ]* *[^ ]* *\([0123456789]*\).*/\1/p'`

    if test -n "$MBUILD_STEP_ID"; then
        python $SRCROOT/client.py --result "MinocaOS Size" integer "$file_size"
        python $SRCROOT/client.py --upload schedule $file $file
        echo Uploaded file $file, size $file_size
    fi

    ##
    ## The SDK includes all architectures.
    ##

    file=MinocaSDK-$REVISION.zip
    file_size=`ls -l $file | \
        sed -n 's/[^ ]* *[^ ]* *[^ ]* *[^ ]* *\([0123456789]*\).*/\1/p'`

    if test -n "$MBUILD_STEP_ID"; then
        python $SRCROOT/client.py --result "MinocaSDK Size" integer "$file_size"
        python $SRCROOT/client.py --upload schedule $file $file
        echo Uploaded file $file, size $file_size
    fi

    ##
    ## Third-party is just source, so the architecture is irrelevant and
    ## therefore unspecified.
    ##

    file=MinocaTP-$REVISION.zip
    file_size=`ls -l $file | \
        sed -n 's/[^ ]* *[^ ]* *[^ ]* *[^ ]* *\([0123456789]*\).*/\1/p'`

    if test -n "$MBUILD_STEP_ID"; then
        python $SRCROOT/client.py --result "MinocaTP Size" integer "$file_size"
        python $SRCROOT/client.py --upload schedule $file $file
        echo Uploaded file $file, size $file_size
    fi
fi

##
## Remove the original binroot.
##

cd "$ORIGINAL_DIRECTORY"
if test -d "$SAVED_BINROOT"; then
    echo "Removing original BINROOT."
    rm -rf "$SAVED_BINROOT"
fi

echo "Completed generating distributables."
exit 0

