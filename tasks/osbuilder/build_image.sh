#! /bin/sh
## Copyright (c) 2015 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     build_image.sh
##
## Abstract:
##
##     This script builds a custom Minoca OS image.
##
## Author:
##
##     Evan Green 27-Feb-2015
##
## Environment:
##
##     Build
##

set -e

file=build_request.json
python ../../client.py --pull $file $file 0
export TASKDIR="$PWD/../../tasks"
export STRIP=strip
python $TASKDIR/osbuilder/build_image.py

