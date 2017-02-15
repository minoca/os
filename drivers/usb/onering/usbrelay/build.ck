/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbrelay

Abstract:

    This executable implements the usbrelay application, which is a
    simple application that connects to and communicates with the USB
    relay board from One Ring Road.

Author:

    Evan Green 26-Jan-2015

Environment:

    Kernel

--*/

from menv import application;

function build() {
    var app;
    var dynlibs;
    var entries;
    var name = "usbrelay";
    var sources;

    sources = [
        "usbrelay.c"
    ];

    dynlibs = [
        "apps/osbase:libminocaos"
    ];

    app = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = application(app);
    return entries;
}

