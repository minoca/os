##
## Copyright (c) 2015 Minoca Corp.
##
##    This file is licensed under the terms of the GNU General Public License
##    version 3. Alternative licensing terms are available. Contact
##    info@minocacorp.com for details. See the LICENSE file at the root of this
##    project for complete licensing information..
##
## Script Name:
##
##     cpu_info.py
##
## Abstract:
##
##     This script determine cpu information.
##
## Author:
##
##     Chris Stevens 5-May-2015
##
## Environment:
##
##     Python
##

import argparse
import os

try:
    import multiprocessing

except ImportError:
    pass

def main():
    description = "This script collects cpu information and prints it to " \
                  "standard out."

    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("-c",
                        "--count",
                        help="Get the number of active cores on the system.",
                        action="store_true")

    arguments = parser.parse_args()
    if arguments.count:
        cpu_count = get_cpu_count()
        if cpu_count < 1:
            print("unknown")

        else:
            print(cpu_count)

    return 0

def get_cpu_count():

    ##
    ## Try various methods to get the cpu count. Start with the multiprocessing
    ## library, which should be available on Python versions 2.6 and up.
    ##

    try:
        return multiprocessing.cpu_count()

    except (NameError, NotImplementedError):
        pass

    ##
    ## Try using sysconf, which is available in some C librarys, including
    ## glibc and Minoca's libc.
    ##

    try:
        cpu_count = os.sysconf('SC_NPROCESSORS_ONLN')
        if cpu_count > 0:
            return cpu_count

    except (AttributeError, ValueError):
        pass

    ##
    ## Try Windows environment variables.
    ##

    try:
        cpu_count = int(os.environ['NUMBER_OF_PROCESSORS'])
        if cpu_count > 0:
            return cpu_count

    except (KeyError, ValueError):
        pass

    ##
    ## Try using the sysctl utility for BSD systems.
    ##

    try:
        sysctl = os.popen('sysctl -n hw.ncpu')
        cpu_count = int(sysctl.read())
        if cpu_count > 0:
            return cpu_count

    except (OSError, ValueError):
        pass

    ##
    ## Give up and return -1.
    ##

    return -1

if __name__ == '__main__':
    exit(main())

