##
## This script tars up a file.
##

./usbrelay 1
echo tar -czvf ./galaxy.tar.gz ./galaxy.tif
tar -czvf ./galaxy.tar.gz ./galaxy.tif
./usbrelay 3
echo tar -czvf ./galaxy.tar.gz.tar.gz ./galaxy.tar.gz ./galaxy.tif
tar -czvf ./galaxy.tar.gz.tar.gz ./galaxy.tar.gz ./galaxy.tif
./usbrelay 7
echo rm -f *.gz
rm -f *.gz
echo Donezo
