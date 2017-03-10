##
## This script compiles z-lib.
##

OBJS="adler32.c \
compress.c \
crc32.c \
deflate.c \
gzclose.c \
gzlib.c \
gzread.c \
gzwrite.c \
infback.c \
inffast.c \
inflate.c \
inftrees.c \
trees.c \
uncompr.c \
zutil.c"

cd ./zlib-1.2.11
rm -f *.o libz.a
ofiles=
a=0
for cfile in $OBJS; do
    ofile=`echo $cfile | sed 's/\.c/.o/'`
    ofiles="$ofiles $ofile"
    a=$((a+1))
    ../usbrelay $a
    echo gcc -c -o $ofile $cfile
    gcc -c -o $ofile $cfile
done

echo ar rcs libz.a $ofiles
ar rcs libz.a $ofiles
rm -f *.o libz.a
echo Donezo
