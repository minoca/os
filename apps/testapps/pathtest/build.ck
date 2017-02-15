/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Path Test

Abstract:

    This executable implements the Path test application.

Author:

    Chris Stevens 17-Sep-2013

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
        "pathtest.c"
    ];

    dynlibs = [
        "apps/osbase:libminocaos"
    ];

    includes = [
        "$S/apps/libc/include"
    ];

    app = {
        "label": "pathtest",
        "inputs": sources + dynlibs,
        "includes": includes
    };

    entries = application(app);
    return entries;
}

