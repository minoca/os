#!/bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     git_up.sh
##
## Abstract:
##
##     This script updates the source repositories for a Windows build. SRCROOT
##     must be set.
##
## Author:
##
##     Evan Green 13-May-2014
##
## Environment:
##
##     Minoca (Windows) Build
##

set -e
if ! test -d $SRCROOT; then
    echo "Error: SRCROOT ($SRCROOT) not set or invalid."
    exit 1
fi

cd $SRCROOT/os
pwd
git pull
git status
cd $SRCROOT/tools
pwd
git pull
git status
cd $SRCROOT/third-party
pwd
git pull
git status
echo "Git update complete"
exit 0
