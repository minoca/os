/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    User Input

Abstract:

    This module implements the User Input driver. It does not implement
    support for any specific device, but manages and provides support for
    all user input devices. Drivers that support user input hardware should
    link against this driver and utilize its framework for implementing a
    user input device that the system can interact with in a generic manner.

Author:

    Evan Green 16-Feb-2013

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var entries;
    var libs;
    var name = "usrinput";
    var sources;

    sources = [
        "uskeys.c",
        "usrinput.c"
    ];

    libs = [
        "lib/termlib:termlib"
    ];

    drv = {
        "label": name,
        "inputs": sources + libs,
    };

    entries = driver(drv);
    return entries;
}

