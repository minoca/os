/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    banner

Abstract:

    This executable can be used to toggle the banner thread on and off.

Author:

    Evan Green 31-Jul-2017

Environment:

    User

--*/

from menv import application;

function build() {
    var app;
    var dynlibs;
    var entries;
    var includes;
    var sources;

    sources = [
        "banner.c"
    ];

    dynlibs = [
        "apps/osbase:libminocaos"
    ];

    includes = [
        "$S/apps/libc/include"
    ];

    app = {
        "label": "banner",
        "inputs": sources + dynlibs,
        "includes": includes,
    };

    entries = application(app);
    return entries;
}

