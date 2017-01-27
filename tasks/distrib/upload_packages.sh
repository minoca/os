##
## Copyright (c) 2017 Minoca Corp.
##
##    This file is licensed under the terms of the GNU General Public License
##    version 3. Alternative licensing terms are available. Contact
##    info@minocacorp.com for details. See the LICENSE file at the root of this
##    project for complete licensing information..
##
## Script Name:
##
##     upload_packages.sh
##
## Abstract:
##
##     This script uploads the packages to production.
##
## Author:
##
##     Evan Green 20-Jan-2017
##
## Environment:
##
##     Windows Build
##

if test -z "$SRCROOT"; then
    echo "Error: SRCROOT must be set."
    exit 1
fi

if test -z "$ARCH"; then
    echo "Error: ARCH must be set."
    exit 1
fi

if test -z "$UPLOAD_DATE"; then
    echo "Uploads not prepared."
    exit 1
fi

BINROOT=$SRCROOT/${ARCH}${VARIANT}dbg/bin
VERSION=`cat $BINROOT/kernel-version | \
    sed 's/\([0-9]*\)\.\([0-9]*\)\..*/\1.\2/'`

parch=$ARCH$VARIANT
case "$ARCH$VARIANT" in
  x86) parch=i686 ;;
  x86q) parch=i586 ;;
  *) parch="$ARCH" ;;
esac

##
## Get the last native build instance.
##

barch=$ARCH$VARIANT
last_native_build=`python ../../client.py --query "Native Pilot $barch"`
if test -z $last_native_build; then
  echo "Error: Failed to get last Native Pilot $barch build."
  exit 1
fi

##
## Loop over each repository.
##

for repo in main; do

    ##
    ## Get the index file, which lists the other files.
    ##

    repo_dir="$VERSION/$parch/$repo/"
    index_file="$repo_dir/Packages"
    echo "Downloading $index_file"
    python ../../client.py --pull package $index_file Packages \
        $last_native_build

    packages=`cat Packages | grep Filename: | sed 's/Filename: //'`

    ##
    ## Create the destination directory.
    ##

    mkdir_on_production "$repo_dir"
    mkdir -p "$repo_dir"

    ##
    ## Download each package and upload it to production.
    ##

    for f in $packages Packages Packages.gz; do
        file_path="$repo_dir/$f"
        python ../../client.py --pull package "$repo_dir/$f" "$repo_dir/$f" 0
        upload_to_production "$repo_dir/$f"
    done
done

