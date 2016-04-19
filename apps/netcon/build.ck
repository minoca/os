/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    netcon

Abstract:

    This executable implements the network configuration application.

Author:

    Chris Stevens 14-Mar-2016

Environment:

    User

--*/

function build() {
    sources = [
        "netcon.c"
    ];

    dynlibs = [
        "//apps/osbase:libminocaos",
        "//apps/netlink:libnetlink"
    ];

    includes = [
        "$//apps/include",
        "$//apps/include/libc"
    ];

    app = {
        "label": "netcon",
        "inputs": sources + dynlibs,
        "includes": includes
    };

    entries = application(app);
    return entries;
}

return build();
