/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Input

Abstract:

    This directory contains user input related drivers.

Author:

    Evan Green 28-Apr-2017

Environment:

    Kernel

--*/

from menv import group;

function build() {
    var entries;
    var inputDrivers;

    inputDrivers = [
        "drivers/input/elani2c:elani2c",
        "drivers/input/i8042:i8042"
    ];

    entries = group("input_drivers", inputDrivers);
    return entries;
}

