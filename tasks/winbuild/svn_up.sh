#!/bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     svn_up.sh
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

export PATH="$PATH;C:/Program Files/SlikSvn/bin;"
cd $SRCROOT/os
pwd
svn up --non-interactive
svn st
cd $SRCROOT/tools
pwd
svn up --non-interactive
svn st
cd $SRCROOT/third-party
pwd
svn up --non-interactive
svn st
echo "Svn update complete"
exit 0