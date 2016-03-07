#!/bin/sh
## Copyright (c) 2016 Minoca Corp. All Rights Reserved.
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

ROOT_FILES=".gitattributes
.gitconfig"

for file in $ROOT_FILES; do
    cp -pv "$SRCROOT/os/$file" "$DEST/$file"
done

TERMLIB="$DEST/termlib"
mkdir -p "$TERMLIB"
TERMLIB_FILES="term.c"
for file in $TERMLIB_FILES; do
    cp -pv "$LIB/termlib/$file" "$TERMLIB/$file"
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
    cp -pv "$LIB/rtl/base/$file" "$RTL_BASE/$file"
done

cp -pv "$SRCROOT/os/lib/rtl/rtlp.h" "$DEST/rtl/rtlp.h"

RTLC="$DEST/rtl/rtlc"
mkdir -p "$RTLC/"
RTLC_FILES="stubs.c"
for file in $RTLC_FILES; do
    cp -pv "$LIB/rtl/rtlc/$file" "$RTLC/$file"
done

WINCSUP="$DEST/libc/wincsup"
mkdir -p "$WINCSUP/include"
WINCSUP_FILES="../regexcmp.c
../regexexe.c
../regexp.h
strftime.c
include/regex.h"
for file in $WINCSUP_FILES; do
    cp -pv "$APPS/libc/dynamic/wincsup/$file" "$WINCSUP/$file"
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
    cp -pv "$APPS/swiss/$file" "$SWISS/$file"
done

SWISS_EDITED_FILES="win32/w32cmds.c
cmds.c
uos/uoscmds.c
win32/w32cmds.c"

for file in $SWISS_EDITED_FILES; do
    echo \'$APPS/swiss/$file\' -\> \'$SWISS/$file\'
    sed '/.*DwMain.*/d' "$APPS/swiss/$file" > "$SWISS/$file"
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
    cp -pv "$SRCROOT/os/include/$file" "$INCLUDE/$file"
done

echo "Done copying swiss files."

