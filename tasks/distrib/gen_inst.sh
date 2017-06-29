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
##     gen_inst.sh
##
## Abstract:
##
##     This script generates the install archives.
##
## Author:
##
##     Evan Green 30-Mar-2016
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
WORKING="$SRCROOT/instwork"
if [ -d "$WORKING" ]; then
    echo "Removing old $WORKING"
    rm -rf "$WORKING"
fi

mkdir "$WORKING"
for arch in x86 x86q armv7 armv6; do
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

    mkdir -p "$WORKING/$arch"
    if [ -r "$BINROOT/install.img" ]; then
        cp -pv "$BINROOT/install.img" "$WORKING/$arch/"
    fi

    if [ -r "$BINROOT/msetup" ]; then
        cp -pv "$BINROOT/msetup" "$WORKING/$arch/"
    fi

    if [ -r "$SRCROOT/x86dbg/tools/bin/msetup.exe" ]; then
        cp -pv "$SRCROOT/x86dbg/tools/bin/msetup.exe" "$WORKING/msetup.exe"
    fi

done

##
## Add the readme file.
##

DATE=`date "+%B %d, %Y"`
echo "Minoca OS Installer Revision $REVISION.
Created: $DATE
Flavor: $DEBUG" > $WORKING/readme.txt

cat >> $WORKING/readme.txt <<"_EOF"
Website: www.minocacorp.com
Contact Minoca at: info@minocacorp.com

Minoca OS is a leading-edge, highly customizable, general purpose operating
system. It features application level functionality such as virtual memory,
networking, and POSIX compatibility, but at a significantly reduced image and
memory footprint. Unique development, debugging, and real-time profiling tools
make getting to the bottom of issues straightforward and easy. Direct support
from the development team behind Minoca OS simplifies the process of creating
OS images tailored to your application, saving on engineering resources and
development time. Minoca OS is a one-stop shop for systems-level design.

For a more detailed getting started guide, head to
http://www.minocacorp.com/documentation/getting-started

What is this?
=============
This archive contains the generic installation images and setup command line
app for creating custom images of Minoca OS. If you're looking to simply get
a quick bootable image of Minoca OS on a particular platform, this is not the
archive for you. Go download one of our prebuilt images, available for each
supported platform Minoca runs on.

Inside the archive are installation images for each supported processor
architecture. The Minoca native setup program for that architecture is also in
that architecture directory. If you're running setup on Minoca to install
Minoca, use the native setup programs.

Additionally there is a setup command line tool for Windows. Use this if you'd
like to install Minoca onto a removable disk from Windows. It is also possible
to install directly onto your host system. Make sure to create a backup before
trying to create a dual boot scenario. Currently the setup program will
install its own MBR, blowing away the capability to boot back into Windows
without a rescue disk. We recommend installing to removable media (such as a
USB disk or SD card), a blank disk, or a disk file (for use with a VM).


How do I use it?
================
Run msetup with no arguments to get a list of available installation locations,
including enumerated disks and partitions on the local machine. Use
msetup --help to get a list of command line options and ways to specialize your
image. You can install directly to a removable disk using its device ID number
(eg. msetup -vD -d0x10000).

Example
=======
C:\Users\Evan\Downloads\Minoca-Installer-1341>msetup
Unable to detect platform name.
No destination was specified. Please select one from the following list.
Setup found 6 devices:
    DiskId   PartID   DevType   Fmt    Type          Offset Size   Path
    ----------------------------------------------------------------------------
    CB2C1753 00000000 Disk      MBR                  0      931G   Device 0x0
    CB2C1753 00000001 Partition MBR B  NTFS          1.0M   100M   Device 0x1
    CB2C1753 00000002 Partition MBR    NTFS          101M   931G   Device 0x2
    444A2245 00000000 Disk      MBR                  0      29G    Device 0x10000
    444A2245 00000001 Partition MBR B  FAT16         1.0M   10M    Device 0x10001
    444A2245 00000000 Partition MBR    Minoca        11M    501M   Device 0x10002


C:\Users\Evan\Downloads\mbin\Minoca-Installer-1341>msetup -i armv7\install.img -v -l beagleboneblack -D -d0x10000

License
=======
The contents of this archive are licensed under the Creative Commons
Non-Commercial Share-Alike license, which can be found at
http://creativecommons.org/licenses/by-nc-sa/4.0/


Contact
=======
Contact info@minocacorp.com with any questions or concerns.

_EOF

##
## Create the archive.
##

ARCHIVE="Minoca-Installer-$REVISION.zip"
7za a -tzip -mmt -mx9 -mtc "$ARCHIVE" $WORKING/*
FILE_SIZE=`ls -l $ARCHIVE | \
    sed -n 's/[^ ]* *[^ ]* *[^ ]* *[^ ]* *\([0123456789]*\).*/\1/p'`

if test -n "$MBUILD_STEP_ID"; then
    python $SRCROOT/client.py --result "$ARCHIVE Size" integer "$FILE_SIZE"
    python $SRCROOT/client.py --upload schedule $ARCHIVE $ARCHIVE
    echo Uploaded file $ARCHIVE, size $FILE_SIZE
fi

echo "Removing $WORKING"
rm -rf "$WORKING"
echo "Done creating installer archive."

