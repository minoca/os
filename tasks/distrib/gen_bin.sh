##
## Copyright (c) 2014 Minoca Corp.
##
##    This file is licensed under the terms of the GNU General Public License
##    version 3. Alternative licensing terms are available. Contact
##    info@minocacorp.com for details. See the LICENSE file at the root of this
##    project for complete licensing information..
##
## Script Name:
##
##     gen_bin.sh
##
## Abstract:
##
##     This script generates the distributed image archive containing Qemu,
##     the image, the debugger, and symbols.
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
ARCHIVE_DIRECTORY="MinocaOS-Starter-$REVISION"
WORKING="$BINROOT/$ARCHIVE_DIRECTORY"
if test -d "$WORKING"; then
    echo "Error: $WORKING already exists. Clean it up first."
    exit 1
fi

ARCHIVE="MinocaOS-Starter-$REVISION.zip"
if test -f "$ARCHIVE"; then
    echo "Error: '$ARCHIVE' already exists. Delete it first."
    exit 1
fi

FILES=`echo *`
mkdir -p $WORKING

##
## Copy Qemu.
##

if test "x$ARCH$VARIANT" = "xx86"; then
    cp -Rv $SRCROOT/tools/win32/qemu-0.13.0-windows $WORKING
    mv $WORKING/qemu-0.13.0-windows $WORKING/Qemu
    rm -rf "$WORKING/Qemu/bin"
    rm -f "$WORKING/Qemu/stderr.txt" "$WORKING/Qemu/stdout.txt"

elif test "x$ARCH" = "xarmv7"; then
    cp -Rv "$SRCROOT/tools/win32/qemu-2.0.0" "$WORKING/Qemu"
    rm -f "$WORKING/Qemu/stderr.txt" "$WORKING/Qemu/stdout.txt"
fi

##
## Copy the debugger (always from x86).
##

mkdir -p $WORKING/Minoca/Debug
OLDPWD="$PWD"
cd "$SRCROOT/x86$DEBUG/bin"
for file in debugui.exe debug.exe kexts.dll dbgext.a; do
    cp -pv ./$file "$WORKING/Minoca/Debug/"
done

cp -pv "$SRCROOT/os/include/minoca/debug/dbgext.h" "$WORKING/Minoca/Debug"
cd "$OLDPWD"

##
## Copy the images.
##

mkdir -p $WORKING/Minoca/Image
if test "x$ARCH" = "xx86"; then
    cp -pv ./distribute/pc.img "$WORKING/Minoca/Image"

elif test "x$ARCH" = "xarmv7"; then
    cp -pv ./distribute/integ.img "$WORKING/Minoca/Image"

elif test "x$ARCH" = "xarmv6"; then
    cp -pv ./distribute/rpi.img "$WORKING/Minoca/Image"
fi

##
## Copy the symbols.
##

REMOVE='apps*
dep
skel
Python*
*.img
*.vmdk
pagefile.sys
*.zip
tools
distribute
packages'

for file in $REMOVE; do
    FILES=`echo $FILES | sed s/$file//`
done

mkdir -p $WORKING/Minoca/Symbols
cp -pv -- $FILES $WORKING/Minoca/Symbols

##
## Copy win32 disk imager.
##

cp -Rv $SRCROOT/tools/win32/Win32DiskImager $WORKING

##
## Add the readme file.
##

DATE=`date "+%B %d, %Y"`
echo "Minoca OS Revision $REVISION.
Created: $DATE
Architecture: $ARCH
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

Installation
============
This archive requires no installation. Extract the contents of the downloaded
zip archive to the directory of your choice.


Running Minoca OS under Qemu
============================
On x86 or ARMv7 builds, double click run.bat to start up Qemu with MinocaOS.
A kernel debugger will also start and connect to the Qemu instance via a named
pipe. By default, the emulator starts with 256MB of RAM and networking enabled.
Feel free to edit run.bat to change these parameters. Try breaking into the
debugger by hitting Control+B. Type "k" to see the call stack where you broke
in, and "g" to let the system go again. The "help" command provides an overview
of debugger commands, or see the started page at:
http://www.minocacorp.com/documentation/getting-started

For ARMv6 builds, the provided rpi.img must be built and run on a Raspberry Pi.


Running on real hardware
========================
To run Minoca OS on a supported board, you'll need to write the MinocaOS
image out to a USB flash drive or SD card. The drive must be at least 1GB in
size.
  * Open up the Win32 Disk Imager tool included in the archive under
    Win32DiskImager\Win32DiskImager.exe
  * In the Disk Imager application, Select the image to write out by clicking
    the folder icon or typing in the path manually. The image will be located
    within the directory you unpacked the archive to. For x86 images, the
    location within the archive is Minoca\Image\pc.img. For the Raspberry Pi,
    it is Minoca\Image\rpi.img.
  * Plug in a USB stick or SD card that's at least 1GB in size and select the
    drive letter for that stick in the Device dropdown of the Disk Imager app.
    Since the image comes with a pre-formatted file system, only 1GB of the USB
    stick will be accessible even if the stick itself is larger than 1GB. This
    is normal.
  * Make ABSOLUTELY SURE you've selected the correct drive, as ALL DATA ON THE
    DRIVE WILL BE LOST.
  * Select "Write" to write the disk image out to the USB drive, and wait for
    it to complete.
  * For USB disks on the Gizmo 2 board, make sure to plug the USB stick into
    one of the black USB ports (not the blue ones). Keyboards must also be
    plugged into the black ports, and MUST BE PLUGGED IN BEHIND A HUB.
    (We haven't built OHCI yet).
  * For the Raspberry Pi, only SD boot is supported.
  * If the image was written successfully, the screen will clear to green, and
    you will be presented with a shell prompt. Good luck.


License
=======
The contents of this archive are licensed under the GNU General Public License
version 3.


Troubleshooting
===============
If you're having trouble getting up and running, definitely check out the
getting started page at http://www.minocacorp.com/documentation/getting-started.
It walks through these steps and more in great detail. If your question is
answered neither here nor at the getting started page, feel free to shoot us an
email at info@minocacorp.com.

Q: When I double click run.bat, I get the message "Windows cannot find
   '.\Qemu\qemu.exe'. Make sure you typed the name correctly, and then try
   again".
A: Make sure the downloaded zip file has been extracted, you cannot just double
   click into the archive and run it from there. Right click the zip file you
   downloaded, and select "Extract All". Then navigate into the directory that
   was created and try double-clicking run.bat again.

Q: MinocaOS used to boot, but now it's breaking into the debugger when I start
   up, and printing menacing messages.
A: It's possible that the file system was left in an inconsistent state after
   being unexpectedly shut down. Before turning off the simulator or system,
   make sure that the first number next to "Cache:" in the top banner is zero
   (this is the "dirty" page count). If you've corrupted your image, you'll
   need to re-download it and restore the original Minoca/Image/pc.img from the
   download.

Q: The Gizmo 2 screen turns green and the OS statistics banner displays, but I
   never get a command prompt.
A: Make sure your USB stick is plugged into one of the black USB ports
   (underneath the ethernet port). The blue USB 3.0 ports are not yet supported
   on Minoca OS.

Q: Why does the debugger complain that my symbol timestamps are mismatched?
A: If you used the built-in zip extraction in Windows 7 and later, then all the
   extracted file timestamps get reset. Don't worry, all the symbols still
   match up and you can debug as expected. If you don't like the prints,
   extract the zip archive with a more sophisticated tool like 7za.

Q: I'm getting a "Fatal System Error" or "Assertion failure". What do I do?
A: Please file a bug with us indicating the message and debugger output. If
   you're able to reproduce this issue consistently, make sure to include those
   steps needed to reproduce the issue.

Q: When I break into the debugger, I get warnings about mismatching timestamps
   that read "Warning: Target timestamp for kernel is Thu Feb 19
   23:38:36 2015 but file '.\Minoca\Symbols/kernel' has timestamp Fri Feb 20
   11:17:46 2015."
A: These warnings are safe to ignore. The default Windows archive extraction
   modifies the timestamps on the unzipped files (i.e. Right-Click -> Extract
   All). If you'd like to remove them, try a different extraction method; 7-zip
   works for us.

_EOF

##
## Add the one-click script.
##

if test "x$ARCH" = "xx86"; then
    QEMU_IMAGE=".\\Minoca\\Image\\pc.img"

elif test "x$ARCH" = "xarmv7"; then
    QEMU_IMAGE=".\\Minoca\\Image\\integ.img"
fi

if test "$QEMU_IMAGE"; then
    cat > $WORKING/run.bat <<_EOF

rem Fire up Qemu, which will hang invisibly waiting for something to connect
rem on its named pipe.

start .\\Qemu\\qemu.exe -L .\\Qemu -m 256 -hda $QEMU_IMAGE -serial pipe:minocapipe -net nic,model=i82559er -net user

rem Fire up the debugger to connect to the named pipe, and we're off to the
rem races. The -s parameter sets the symbol path, -e loads handy debugger
rem extensions (type ! for help), and -k creates a kernel connection to the
rem named pipe. Remember, for the GUI version of the debugger, use Ctrl+B to
rem break in.

.\\Minoca\\Debug\\debugui.exe -s .\\Minoca\\Symbols -e .\\Minoca\\debug\\kexts.dll -k \\\\.\\pipe\\minocapipe

_EOF
fi

##
## Add the files to the archive.
##

echo "Creating archive..."
7za a -tzip -mx9 -mmt -mtc "$ARCHIVE" "$ARCHIVE_DIRECTORY"
echo "Successfully created $ARCHIVE"
rm -rf "$ARCHIVE_DIRECTORY"

