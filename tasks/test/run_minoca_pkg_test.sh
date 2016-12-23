#!/bin/sh
##
## Copyright (c) 2016 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     run_minoca_pkg_test.sh
##
## Abstract:
##
##     This script runs third party packages smoke tests on running Minoca OS.
##     Script takes two steps:
##     1) Installs (or upgrades to the latest version) minoca-pkg-test plugin
##        to perform smoke tests
##     2) Runs smoke tests by running minoca-pkg-test. Remove --nocolor if you
##        want to get color output.
##
## Author:
##
##     Alexey Melezhik 06-Dec-2016
##

set -e
set -x

sparrow index update
sparrow plg install minoca-pkg-test
sparrow plg run minoca-pkg-test --nocolor

