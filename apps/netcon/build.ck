/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    netcon

Abstract:

    This executable implements the network configuration application.

Author:

    Chris Stevens 14-Mar-2016

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
        "netcon.c"
    ];

    dynlibs = [
        "apps/osbase:libminocaos",
        "apps/netlink:libnetlink"
    ];

    includes = [
        "$S/apps/libc/include"
    ];

    app = {
        "label": "netcon",
        "inputs": sources + dynlibs,
        "includes": includes
    };

    entries = application(app);
    return entries;
}

