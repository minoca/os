#! /bin/sh
## Copyright (c) 2015 Minoca Corp.
##
##    This file is licensed under the terms of the GNU General Public License
##    version 3. Alternative licensing terms are available. Contact
##    info@minocacorp.com for details. See the LICENSE file at the root of this
##    project for complete licensing information..
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
python ../../client.py --pull schedule $file $file 0
export TASKDIR="$PWD/../../tasks"
export STRIP=strip
python $TASKDIR/osbuilder/build_image.py

