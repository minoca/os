/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Video Console

Abstract:

    This module implements a basic console on a framebuffer.

Author:

    Evan Green 15-Feb-2013

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var libs;
    var entries;
    var name = "videocon";
    var sources;

    sources = [
        "videocon.c"
    ];

    libs = [
        "lib/basevid:basevid",
        "lib/termlib:termlib"
    ];

    drv = {
        "label": name,
        "inputs": sources + libs,
    };

    entries = driver(drv);
    return entries;
}

