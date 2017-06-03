#!/bin/sh
## Copyright (c) 2016 Minoca Corp.
##
##    This file is licensed under the terms of the GNU General Public License
##    version 3. Alternative licensing terms are available. Contact
##    info@minocacorp.com for details. See the LICENSE file at the root of this
##    project for complete licensing information..
##
## Script Name:
##
##     cposs.sh
##
## Abstract:
##
##     This script copies the swiss source to the open source repository.
##
## Author:
##
##     Evan Green 18-Jan-2016
##
## Environment:
##
##     Minoca Build
##

set -e

if test -z "$SRCROOT"; then
    echo "Error: SRCROOT must be set."
    exit 1
fi

if test -z "$DEST"; then
    DEST="$SRCROOT/swiss"
    if ! [ -d "$DEST" ]; then
        echo "Error: DEST must be set or "$DEST" must exist."
        exit 1
    fi
fi

APPS="$SRCROOT/os/apps"
LIB="$SRCROOT/os/lib"

SED_ARG='/^Copyright/,/^Module Name:/{
    # Print the last line normally.
    /^Module Name:/b
    # Preserve spacing.
    /^$/b

    # Normalize the Minoca copyright. Add the OSS license after the Minoca
    # copyright.
    s/Copyright[^0-9]*\([-, 0-9]*\).*/Copyright (c) \1Minoca Corp./
    t addlicense

    # Preserve other non-Minoca copyrights.
    /^[Cc]opyright.*/ b

    # Delete all other junk.
    d
    :addlicense {
        p # Print the Minoca copyright line.
        i \
\
This project is dual licensed. You are receiving it under the terms of the\
GNU General Public License version 3 (GPLv3). Alternative licensing terms are\
available. Contact info@minocacorp.com for details. See the LICENSE file at the\
root of this project for complete licensing information.
        d
    }
}'

ROOT_FILES=".gitattributes
.gitconfig"

for file in $ROOT_FILES; do
    sed "$SED_ARG" "$SRCROOT/os/$file" > "$DEST/$file"
done

TERMLIB="$DEST/termlib"
mkdir -p "$TERMLIB"
TERMLIB_FILES="term.c"
for file in $TERMLIB_FILES; do
    sed "$SED_ARG" "$LIB/termlib/$file" > "$TERMLIB/$file"
done

RTL_BASE="$DEST/rtl/base"
mkdir -p "$RTL_BASE/armv7" "$RTL_BASE/x86" "$RTL_BASE/x64"
RTL_BASE_FILES="crc32.c
heap.c
math.c
print.c
rbtree.c
scan.c
softfp.c
softfp.h
string.c
time.c
time.h
timezone.c
wchar.c
wprint.c
wscan.c
wstring.c
wtime.c
armv7/intrinsa.S
armv7/intrinsc.c
armv7/rtlarch.S
armv7/rtlmem.S
fp2int.c
x86/intrinsc.c
x86/rtlarch.S
x86/rtlmem.S
x64/rtlarch.S
x64/rtlmem.S"
for file in $RTL_BASE_FILES; do
    sed "$SED_ARG" "$LIB/rtl/base/$file" > "$RTL_BASE/$file"
done

sed "$SED_ARG" "$SRCROOT/os/lib/rtl/rtlp.h" > "$DEST/rtl/rtlp.h"

RTLC="$DEST/rtl/urtl/rtlc"
mkdir -p "$RTLC/"
RTLC_FILES="stubs.c ../pdouble.c"
for file in $RTLC_FILES; do
    sed "$SED_ARG" "$LIB/rtl/urtl/rtlc/$file" > "$RTLC/$file"
done

WINCSUP="$DEST/libc/wincsup"
mkdir -p "$WINCSUP/include"
WINCSUP_FILES="../regexcmp.c
../regexexe.c
../regexp.h
strftime.c
include/regex.h"
for file in $WINCSUP_FILES; do
    sed "$SED_ARG" "$APPS/libc/dynamic/wincsup/$file" > "$WINCSUP/$file"
done

SWISS="$DEST/src"
mkdir -p "$SWISS/ls"
mkdir -p "$SWISS/sed"
mkdir -p "$SWISS/sh"
mkdir -p "$SWISS/swlib"
mkdir -p "$SWISS/login"
mkdir -p "$SWISS/uos"
mkdir -p "$SWISS/win32"

SWISS_FILES="basename.c
cat.c
cecho.c
chmod.c
chroot.c
cmp.c
comm.c
cp.c
cut.c
date.c
dd.c
diff.c
dirname.c
easy.c
echo.c
env.c
expr.c
find.c
grep.c
head.c
id.c
install.c
kill.c
ln.c
ls/compare.c
ls/ls.c
ls/ls.h
mkdir.c
mktemp.c
mv.c
nl.c
nproc.c
od.c
printf.c
ps.c
pwd.c
reboot.c
rm.c
rmdir.c
sed/sed.c
sed/sed.h
sed/sedfunc.c
sed/sedparse.c
sed/sedutil.c
seq.c
sh/alias.c
sh/arith.c
sh/builtin.c
sh/exec.c
sh/expand.c
sh/lex.c
sh/linein.c
sh/parser.c
sh/path.c
sh/sh.c
sh/sh.h
sh/shos.h
sh/shparse.h
sh/signals.c
sh/util.c
sh/var.c
sort.c
split.c
stty.c
sum.c
swiss.c
swiss.h
swisscmd.h
swlib/copy.c
swlib/delete.c
swlib/pattern.c
swlib/pwdcmd.c
swlib/string.c
swlib/userio.c
swlib.h
swlibos.h
tail.c
tee.c
test.c
time.c
touch.c
tr.c
uname.c
uniq.c
wc.c
which.c
xargs.c

chown.c
init.c
login/chpasswd.c
login/getty.c
login/groupadd.c
login/groupdel.c
login/login.c
login/lutil.c
login/lutil.h
login/passwd.c
login/su.c
login/sulogin.c
login/useradd.c
login/userdel.c
login/vlock.c
mkfifo.c
readlink.c
sh/shuos.c
ssdaemon.c
swlib/chownutl.c
swlib/uos.c
telnet.c
telnetd.c

swlib/minocaos.c
swlib/linux.c
win32/swiss.exe.manifest
win32/swiss.rc
sh/shntos.c
swlib/ntos.c"

for file in $SWISS_FILES; do
    sed "$SED_ARG" "$APPS/swiss/$file" > "$SWISS/$file"
done

SWISS_EDITED_FILES="win32/w32cmds.c
cmds.c
uos/uoscmds.c
win32/w32cmds.c"

for file in $SWISS_EDITED_FILES; do
    sed -e '/.*DwMain.*/d' -e '/.*SokoMain.*/d' -e "$SED_ARG" \
        "$APPS/swiss/$file" > "$SWISS/$file"

done

INCLUDE="$DEST/include"
mkdir -p "$INCLUDE/minoca/lib"
mkdir -p "$INCLUDE/minoca/kernel"
INCLUDE_FILES="minoca/lib/types.h
minoca/lib/status.h
minoca/lib/rtl.h
minoca/lib/termlib.h
minoca/lib/tzfmt.h
minoca/kernel/x86.inc
minoca/kernel/arm.inc
minoca/kernel/x64.inc"

for file in $INCLUDE_FILES; do
    sed "$SED_ARG" "$SRCROOT/os/include/$file" > "$INCLUDE/$file"
done

VERSION_H="$SRCROOT/$ARCH$DEBUG/obj/os/apps/swiss/version.h"
if ! [ -r "$VERSION_H" ]; then
    echo "Error: $VERSION_H is missing. You must build swiss first!"
    exit 1
fi

sed -e "s/#define VERSION_LICENSE .*/#define VERSION_LICENSE \"\\\\nThis \
software is licensed under the the terms of the GNU General Public \
License v3.\"/" \
    -e "s/#define VERSION_BUILD_USER .*/#define VERSION_BUILD_USER \"Minoca\"/"\
    -e "s/\\(#define VERSION_BUILD_STRING \"\\)[^-]*-\\(.*\\)/\1\2/" \
    "$VERSION_H" > "$SWISS/version.h"

echo "Done copying swiss files."

