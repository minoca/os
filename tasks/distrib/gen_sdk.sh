##
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     gen_sdk.sh
##
## Abstract:
##
##     This script generates the distributed image archive containing the
##     Compilers, header, makefiles, and samples in the Minoca OS SDK.
##
## Author:
##
##     Evan Green 18-Sep-2014
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

BINROOT="$SRCROOT/${ARCH}chk/bin"
if ! test -d $BINROOT; then
    echo "Error: BINROOT '$BINROOT' does not exist."
    exit 1
fi

if ! test -f "$BINROOT/build-revision"; then
    echo "Error: '$BINROOT/build-revision' is missing."
    exit 1
fi

REVISION=`cat "$BINROOT/build-revision"`
if test -z "$REVISION"; then
    echo "Error: $BINROOT/build-revision is empty."
    exit 1
fi

cd "$BINROOT"
ARCHIVE_DIRECTORY="MinocaSDK-$REVISION"
WORKING="$BINROOT/$ARCHIVE_DIRECTORY"
if test -d "$WORKING"; then
    echo "Error: $WORKING already exists. Clean it up first."
    exit 1
fi

ARCHIVE="MinocaSDK-$REVISION.zip"
if test -f "$ARCHIVE"; then
    echo "Error: '$ARCHIVE' already exists. Delete it first."
    exit 1
fi

mkdir -p "$WORKING"

##
## Copy the win32 tools.
##

mkdir -p "$WORKING/tools/win32"
cp -Rv "$SRCROOT/tools/win32/bin" "$WORKING/tools/win32"
cp -Rv "$SRCROOT/tools/win32/scripts" "$WORKING/tools/win32"
cp -Rv "$SRCROOT/tools/win32/swiss" "$WORKING/tools/win32"
cp -Rv "$SRCROOT/tools/win32/MinGW" "$WORKING/tools/win32"

##
## Copy the headers.
##

mkdir -p "$WORKING/os"
cp -Rpv "$SRCROOT/os/include" "$WORKING/os"
mkdir -p "$WORKING/os/apps"

##
## Copy the samples.
##

mkdir -pv "$WORKING/os/drivers/usb/"
cp -Rpv "$SRCROOT/os/drivers/null/" "$WORKING/os/drivers/"
cp -Rpv "$SRCROOT/os/drivers/ramdisk/" "$WORKING/os/drivers/"
cp -Rpv "$SRCROOT/os/drivers/usb/onering/" "$WORKING/os/drivers/usb/"
mkdir -pv "$WORKING/os/drivers/net/ethernet"
cp -Rpv "$SRCROOT/os/drivers/net/ethernet/e100/" \
    "$WORKING/os/drivers/net/ethernet"

cp -Rpv "$SRCROOT/os/apps/mount/" "$WORKING/os/apps/"

##
## Copy the prebuilt binaries that the samples link against. Also copy the
## compilers and other tools.
##

for iarch in x86 armv7 armv6; do
    WORKING_BINROOT="$WORKING/${iarch}$DEBUG/bin"
    IBINROOT="$SRCROOT/${iarch}$DEBUG/bin"
    mkdir -p "$WORKING_BINROOT"
    if test -r "$IBINROOT/kernel"; then
        cp -pv "$IBINROOT/kernel" "$WORKING_BINROOT"
        cp -pv "$IBINROOT/libc.so.1" "$WORKING_BINROOT"
        cp -pv "$IBINROOT/libminocaos.so.1" "$WORKING_BINROOT"
        cp -pv "$IBINROOT/netcore.drv" "$WORKING_BINROOT"
        cp -pv "$IBINROOT/usbcore.drv" "$WORKING_BINROOT"
    else
        echo "Warning: SDK binaries not found for $iarch"
    fi

    if test -d "$IBINROOT\tools"; then
        cp -Rpv "$IBINROOT\tools" "$WORKING_BINROOT"
    fi
done

##
## Copy or create the makefiles.
##

TODAY=`date "+%d-%b-%Y"`
cp -pv "$SRCROOT/os/minoca.mk" "$WORKING/os"
cat > "$WORKING/os/Makefile" <<_EOS
################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Minoca OS SDK
#
#   Abstract:
#
#       This file builds the various samples available in the Minoca OS SDK.
#       Feel free to add your own directories here.
#
#   Author:
#
#       Automatically Generated $TODAY
#
#   Environment:
#
#       Build
#
################################################################################

##
## Check for the necessary environment variables.
##

ifndef SRCROOT
\$(error Error: SRCROOT not set: Run setenv.cmd to set up the environment.)
endif

ifndef ARCH
\$(error Error: ARCH not set: Run setenv.cmd to set up the environment.)
endif

ifndef DEBUG
\$(error Error: DEBUG not set: Run setenv.cmd to set up the environment.)
endif

DIRS = apps        \
       drivers     \

include \$(SRCROOT)/os/minoca.mk

_EOS

cat > "$WORKING/os/drivers/Makefile" <<_EOS
################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Minoca OS SDK Drivers
#
#   Abstract:
#
#       This directory builds the sample drivers contained in the SDK.
#
#   Author:
#
#       Automatically Generated $TODAY
#
#   Environment:
#
#       Kernel
#
################################################################################

DIRS = null      \
       ramdisk   \
       usb       \
       net       \

include \$(SRCROOT)/os/minoca.mk

_EOS

cat > "$WORKING/os/drivers/usb/Makefile" <<_EOS
################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Minoca OS SDK USB Drivers
#
#   Abstract:
#
#       This directory builds the sample USB drivers contained in the SDK.
#
#   Author:
#
#       Automatically Generated $TODAY
#
#   Environment:
#
#       Kernel
#
################################################################################

DIRS = onering   \

include \$(SRCROOT)/os/minoca.mk

_EOS

cat > "$WORKING/os/drivers/net/Makefile" <<_EOS
################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Minoca OS SDK Network Drivers
#
#   Abstract:
#
#       This directory builds the sample Network drivers contained in the SDK.
#
#   Author:
#
#       Automatically Generated $TODAY
#
#   Environment:
#
#       Kernel
#
################################################################################

DIRS = ethernet  \

include \$(SRCROOT)/os/minoca.mk

_EOS

cat > "$WORKING/os/drivers/net/ethernet/Makefile" <<_EOS
################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Minoca OS SDK Ethernet Drivers
#
#   Abstract:
#
#       This directory builds the sample Ethernet drivers contained in the SDK.
#
#   Author:
#
#       Automatically Generated $TODAY
#
#   Environment:
#
#       Kernel
#
################################################################################

DIRS = e100      \

include \$(SRCROOT)/os/minoca.mk

_EOS

cat > "$WORKING/os/apps/Makefile" <<_EOS
################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Minoca OS SDK Applications
#
#   Abstract:
#
#       This directory builds the sample applications contained in the SDK.
#
#   Author:
#
#       Automatically Generated $TODAY
#
#   Environment:
#
#       User
#
################################################################################

DIRS = mount      \

include \$(SRCROOT)/os/minoca.mk

_EOS

##
## Add the readme file.
##

DATE=`date "+%B %d, %Y"`
echo "Minoca OS SDK Revision $REVISION.
Created: $DATE" > $WORKING/readme.txt

cat >> $WORKING/readme.txt <<"_EOF"
Website: www.minocacorp.com
Contact Minoca Corp at: info@minocacorp.com

Minoca OS is a leading-edge, highly customizable, general purpose operating
system. It features application level functionality such as virtual memory,
networking, and POSIX compatibility, but at a significantly reduced image and
memory footprint. Unique development, debugging, and real-time profiling tools
make getting to the bottom of issues straightforward and easy. Direct support
from the development team behind Minoca OS simplifies the process of creating
OS images tailored to your application, saving on engineering resources and
development time. Minoca OS is a one-stop shop for systems-level design.

For a more detailed getting started guide, head to
http://www.minocacorp.com/documentation/

This archive contains the Minoca OS SDK for Windows, which contains all the
compilers, build utilities, and headers you need to cross compile applications
and drivers for Minoca OS.


Host System Requirements
========================
Windows XP or later
512MB RAM
2GB Hard Disk space

Target System Requirements
==========================
Flexible. Functions best with at least 5MB RAM and 5MB disk space.

License
=======
The source code presented in this archive is licensed under the Creative
Commons Attribution license, which can be found at
http://creativecommons.org/licenses/by/4.0/


Installation
============
This archive requires no installation. Extract the contents of the downloaded
zip archive to the directory of your choice. Ideally this directory would be
near the root of the drive and contain no spaces in the path.


Environment
===========
The SDK environment builds from the command line. Double click one of the
run_x86.bat, run_armv7.bat, or run_armv6.bat files, which will set a few
environment variables and drop you into a Bourne shell. The environment is based
around three environment variables:
  * SRCROOT -- Defines the root of the source tree. The setenv.cmd script
    determines this based on its own path.
  * ARCH -- Defines the architecture to build for. This is either set to x86,
    armv6 or armv7 depending on .bat file used.
  * DEBUG -- Defines whether to compile with debugging checks ("chk"), or
    without debugging checks ("fre"). This is set to "chk", as it matches with
    the free edition binaries.

The various scripts make certain assumptions about the layout of directories
underneath SRCROOT. They assume that the following directories exist underneath
it:
  * tools -- Contains the compilers, build utilities (such as the shell and
    other core utilities), and other pieces of support infrastructure.
  * os -- Contains the SDK source.
  * third-party (optional) -- Contains the third party source packages not
    authored by Minoca.
  * x86chk, x86qchk, armv7chk, armv6chk -- These are actually
    $ARCH$VARIANT$DEBUG, and contain the build output. Each directory is
    created automatically when a build for its particular architecture is
    initiated. Notable directories inside of these include obj, the directory
    where object files, libraries, and executables are built, and bin, the
    directory where "final product" binaries are placed.

Building
========
When you double click one of the run_<arch>.bat, you're dropped into the source
root (SRCROOT). To build the samples, "cd" into the os directory, and run
"make". The application and driver samples should build. The final output
binaries will be located in "$SRCROOT/$ARCH$VARIANT$DEBUG/bin". There's a handy
alias installed, you can simply type "bin" to change to this directory.

You can build inside a specific directory by "cd"ing into it and running make
there (ie os/drivers/usb/onering). You can also run "make clean" from somewhere
inside os, which will remove the obj files for the directory you're in. The
contents of the "bin" directory are not cleared, though if you were to run a
subsequent "make", they would be updated.

_EOF

##
## Add the one-click scripts.
##

for iarch in x86 armv7 armv6; do
    cat > $WORKING/run_$iarch.bat <<_EOF
@ECHO OFF

set REVISION=$REVISION
cmd.exe /k .\\tools\\win32\\scripts\\setenv.cmd $iarch chk

_EOF
done

##
## Add the files to the archive.
##

echo "Creating archive..."
7za a -tzip -mmt -mx9 -mtc "$ARCHIVE" "$ARCHIVE_DIRECTORY"
echo "Successfully created $ARCHIVE"
rm -rf "$ARCHIVE_DIRECTORY"

