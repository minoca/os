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
##     upload.sh
##
## Abstract:
##
##     This script contains helper functions for uploading nightly builds to
##     the production site.
##
## Author:
##
##     Evan Green 19-Jan-2017
##
## Environment:
##
##     Windows Build
##

##
## This code assumes the proper public key is already loaded in the ssh-agent.
##

UPUSER=upload
UPDEST=www.minocacorp.com
UPPORT=2222

# Define the number of builds to keep, plus one.
KEEPCOUNT=16

SSH="$SRCROOT/git/usr/bin/ssh.exe"
SCP="$SRCROOT/git/usr/bin/scp.exe"
UPLOAD_DATE=
NIGHTLIES=nightlies

SSH_CMD="$SSH -p $UPPORT $UPUSER@$UPDEST -- "

run_ssh_cmd() {
    echo "Running: $@" >&2
    $SSH_CMD $@
}

create_todays_directory() {
    [ -z "$UPLOAD_DATE" ] && UPLOAD_DATE=`date +%Y-%m-%d`
    $SSH_CMD "mkdir -p $NIGHTLIES/$UPLOAD_DATE/"
}

get_latests() {
    latests=
    for arch in x86 x86q armv7 armv6; do
        latests="$latests `run_ssh_cmd readlink $NIGHTLIES/latest-$arch || true`"
    done

    echo "$latests"
}

prune_stale_builds() {
    # The + number at the end is one greater than the number of builds to keep.
    stale=`run_ssh_cmd "ls $NIGHTLIES | grep 201 | sort -r | tail --lines=+$KEEPCOUNT"`

    # Don't blow away the latest build from any particular architecture. ARMv6
    # for instance might be way behind.
    latests=`get_latests`

    # Combine the two lists, sort them, and then ask uniq to print everything
    # that's not repeated.
    stale=`echo $stale $latests $latests | sed 's/ /\n/g' | sort | uniq -u`
    for d in $stale; do
        echo "Deleting build $d"
        run_ssh_cmd rm -rf $NIGHTLIES/$d/
    done
}

prepare_for_upload () {
    prune_stale_builds
    create_todays_directory
}

mkdir_on_production() {
    for d in $@; do
        $SSH_CMD mkdir -p "$NIGHTLIES/$UPLOAD_DATE/$d"
    done
}

upload_to_production() {
    for f in $@; do
        echo "Uploading $f to production in $UPLOAD_DATE"
        $SCP -P $UPPORT $f $UPUSER@$UPDEST:$NIGHTLIES/$UPLOAD_DATE/$f
    done
}

update_production_latest() {
    arch="$1"
    [ -z "$arch" ] && echo "Error: architecture must be specified" && exit 1
    $SSH_CMD "ln -sf $UPLOAD_DATE $NIGHTLIES/latest-$arch"
}

