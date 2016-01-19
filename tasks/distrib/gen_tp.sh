##
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     gen_tp.sh
##
## Abstract:
##
##     This script generates the third party build distributable. This is
##     essentially the entire third party directory minus the source.
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

BINROOT=$SRCROOT/${ARCH}chk/bin
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
ARCHIVE_DIRECTORY="MinocaTp-$REVISION"
WORKING="$BINROOT/$ARCHIVE_DIRECTORY"
if test -d "$WORKING"; then
    echo "Error: $WORKING already exists. Clean it up first."
    exit 1
fi

ARCHIVE="MinocaTp-$REVISION.zip"
if test -f "$ARCHIVE"; then
    echo "Error: '$ARCHIVE' already exists. Delete it first."
    exit 1
fi

mkdir -p "$WORKING"

##
## Copy the build directory and root Makefile.
##

cp -Rv "$SRCROOT/third-party/build" "$WORKING"
cp -Rv "$SRCROOT/third-party/Makefile" "$WORKING"
mkdir -p "$WORKING/src"

##
## Add the readme file.
##

DATE=`date "+%B %d, %Y"`
echo "Minoca OS Revision $REVISION.
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


Contents
========
This archive contains diffs and our own build glue to build third party source
packages. The builds run on either Windows or Minoca OS, though certain
packages (like Python) cannot cross compile and therefore will be omitted from
a Windows build.


Requirements & Installation
===========================
You'll need the SDK environment to properly use our Makefiles and build glue.
Extract the contents of this archive into a directory named "third-party" inside
the source root of the SDK. To clarify, your directory tree should look
something like this:

MinocaSDK-NNNN/
     os/
     tools/
     third-party/
          Makefile
          readme.txt (this readme)
          build/
               ...
          src/
               ...

For the sake of download size, the actual clean source packages have been
omitted. To build a specific source package, download the revision specified in
the build directory into the "src" directory. For example, if you were building
tar, you'd have something like this:

MinocaSDK-NNN/
     third-party/
          build/
               tar-1.27.1/
                    build.sh
                    Makefile
                    tar-1.27.1.diff
          src
               tar-1.27.1.tar.gz (downloaded upstream archive here)

Our build glue for a package generally works something like this:
     * Run make in third-party/build/<package>/ (our glue).
     * Our glue extracts the clean source in third-party/src/<package>.tar.gz
       to x86chk/obj/third-party/<package>.src/ (x86chk may be different based
       on architecture and flavor).
     * The patch from third-party/build/<package>/<package>.diff is applied to
       the newly copied source.
     * Make then invokes third-party/build/<package>/build.sh
     * The build.sh script will invoke configure, make, and make install of the
       patched source in x86chk/obj/third-party/<package>/ (it's an out-of-tree
       build).


Rebuilding the toolchain
========================
The third-party build can be used to rebuild the Minoca GCC cross compiler.
You'll need the sources for awk, binutils, GMP, MPFR, MPC, GCC, and ncurses.
After downloading and extracting those packages, simply run "make wintools"
from the base of the third-party directory. Once that builds successfully (and
it may take awhile), cd into "build/gcc-X.X.X/" (replacing X.X.X with the GCC
version found in your directory) and run "make install-wintools".


Other notes
===========
If you are the maintainer of one of the packages here (or would like to act as
a liaison), we'd love to work with you to get to a state where the package
builds without patching. We expect that some of our patches are less than
ideal, and we're more than happy to start a dialog.


License
=======
The diffs provided here are licensed under the same terms as the original
source upon which it was based. Often this is GPL. For any source packages that
do not specify a license, the material presented in this archive is licensed
under the Creative Commons Attribution license, which can be found at
http://creativecommons.org/licenses/by/4.0/


Troubleshooting
===============
Our SDK tools may conflict with an existing installation of MinGW. Make sure
you don't have anything in C:\MinGW. Even if it's not in the path, MinGW has a
way of looking in that directory regardless. If you have this directory and
don't want to remove it, perhaps rename it for the duration of the build.

_EOF

##
## Add the files to the archive.
##

echo "Creating archive..."
7za a -tzip -mmt -mtc "$ARCHIVE" "$ARCHIVE_DIRECTORY"
echo "Successfully created $ARCHIVE"
rm -rf "$ARCHIVE_DIRECTORY"

