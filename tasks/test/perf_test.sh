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
##     perf_test.sh
##
## Abstract:
##
##     This script runs the performance benchmark test.
##
## Author:
##
##     Chris Stevens 4-May-2015
##
## Environment:
##
##     Minoca Build
##

set -e

export TMPDIR=$PWD
export TEMP=$TMPDIR
PERF_TEST=../../testbin/perftest
CLIENT=../../client.py
CPU_INFO=../../tasks/test/cpu_info.py
uname -a

##
## Run the performance test with a single process.
##

results_file=perf_results.txt
if test $1; then
    duration="-d $1"
    echo "Running all performance tests with 1 process for $1 second(s)."

else
    duration=""
    echo "Running all performance tests with 1 process for their default durations."
fi

./$PERF_TEST $duration -s -r $results_file
echo "Done running all performance tests with 1 process."

##
## Push the results. Each line was already formatted as a result.
##

SAVED_IFS="$IFS"
IFS=':'
while read line; do
    python $CLIENT --result $line
done < $results_file

IFS="$SAVED_IFS"
rm $results_file

##
## Get the number of cores on the system. Skip the multi-process test if the
## number of cores could not be determined or if there is only 1 core.
##

cpu_count=`python $CPU_INFO --count`
if [ -z $cpu_count ] ||
   [ "x$cpu_count" = "xunknown" ] ||
   [ "x$cpu_count" = "x1" ]; then

    exit 0
fi

##
## Run each performance test in parallel with N processes, where N is the
## number of cores on the system.
##

results_file=perf_multi_results.txt
if test $1; then
    duration="-d $1"
    echo "Running all performance tests with $cpu_count processes for $1 second(s)."

else
    duration=""
    echo "Running all performance tests with $cpu_count processes for their default durations."
fi

./$PERF_TEST $duration -p $cpu_count -s -r $results_file
echo "Done running all performance tests with $cpu_count processes."

##
## Push the results. Each line was already formatted as a result.
##

SAVED_IFS="$IFS"
IFS=':'
while read line; do
    python $CLIENT --result $line
done < $results_file

IFS="$SAVED_IFS"
rm $results_file

