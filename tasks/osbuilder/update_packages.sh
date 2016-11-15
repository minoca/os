#!/bin/sh
## Copyright (c) 2015 Minoca Corp.
##
##    This file is licensed under the terms of the GNU General Public License
##    version 3. Alternative licensing terms are available. Contact
##    info@minocacorp.com for details. See the LICENSE file at the root of this
##    project for complete licensing information..
##
## Script Name:
##
##     update_packages.sh
##
## Abstract:
##
##     This script downloads the files needed to run OS builder tasks.
##
## Author:
##
##     Evan Green 27-Feb-2015
##
## Environment:
##
##     Minoca Build
##

set -xe

export TMPDIR=$PWD
export TEMP=$TMPDIR

if test -z "$DEBUG"; then
    echo "DEBUG must be set."
    exit 1
fi

##
## Update the files for each architecture.
## TODO: Add armv6 once an armv6 build is created.
##

for arch in x86 x86q armv7; do
    ARCH=$arch
    case $arch in
        x86) package_arch=minoca-i686 ;;
        x86q)
            package_arch=minoca-i586
            ARCH=x86
            VARIANT=q
            ;;

        armv6) package_arch=minoca-armv6 ;;
        armv7) package_arch=minoca-armv7 ;;
    esac

    parch=${package_arch#minoca-}

    ##
    ## Download the latest build. This will create a "bin" directory with the
    ## build binaries.
    ##

    OLDPWD="$PWD"
    rm -rf "../$arch"
    mkdir "../$arch"
    cd "../$arch"
    sh ../../tasks/build/download_build.sh
    VERSION=`cat ./bin/kernel-version | sed 's/\([0-9]*\)\.\([0-9]*\)\..*/\1.\2/'`

    ##
    ## Assuming this is an x86 machine, copy the x86 createimage into the path.
    ##

    if test "x$arch" = "xx86"; then
        cp -pv ./bin/createimage /usr/bin/
    fi

    ##
    ## Get the packages and packages.gz file.
    ##

    url="http://${MBUILD_SERVER_ADDRESS}:${MBUILD_SERVER_PORT}"
    url="$url/packages/$VERSION/$parch/main"
    rm -rf "./packages"
    mkdir "./packages"
    cd "./packages"
    wget "$url/Packages"
    wget "$url/Packages.gz"

    ##
    ## Go get all the packages buster.
    ##

    package_list=`cat ./Packages | sed -n 's/Filename: *\([^ \t]*\)/\1/p'`
    for package in $package_list; do
        wget "$url/$package"
    done

    ##
    ## Create a working opkg.conf file for builds.
    ##

    cd ..
    cat >./opkg.conf <<_EOS
arch $package_arch 100
src/gz main file:///$PWD/packages
dest root /
lists_dir ext $PWD/opkg-lists
_EOS

done

echo Completed Updating packages.
exit

