##
## Copyright (c) 2017 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     test.sh
##
## Abstract:
##
##     This script tests the lzma encoder and decoder.
##
## Author:
##
##     Evan Green 29-Apr-2017
##
## Environment:
##
##     Test
##

set -e
set -x

files="gcc-6.3.0.tar binutils-2.27.tar"
for f in $files; do
    if ! [ -f $f ]; then
        gzip -d < $SRCROOT/third-party/src/${f}.gz > $f
    fi
done

for f in $files; do
    for l in 4 9; do

        # Compress using the given level.
        lzma -clv -$l -i $f -o $f.lz$l 2>$f.lz$l.txt

        # Compress in memory test mode using the same level.
        lzma -clv -$l --memory-test=1 -i $f -o $f.lzm$l 2>$f.lzm$l.txt

        # Decompress the just-compressed binary.
        lzma -dlv -i $f.lz$l -o $f.out$l 2>$f.$l.txt

        # Compare the decompressed binary to the original.
        cmp $f.out$l $f

        # Compare the memory-test compression to the stream-based compression.
        cmp $f.lz$l $f.lzm$l
        cmp $f.lzm$l.txt $f.lz$l.txt

        # Clean up.
        rm $f.lz$l $f.lzm$l $f.out$l $f.lzm$l.txt
        rm $f.lz$l.txt $f.$l.txt
    done
done

