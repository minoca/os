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
        "-I$///apps/include",
        "-I$///apps/include/libc"
    ];

    sources_config = {
        "CPPFLAGS": ["$CPPFLAGS"] + includes,
    };

    app = {
        "label": "netcon",
        "inputs": sources + dynlibs,
        "sources_config": sources_config
    };

    entries = application(app);
    return entries;
}

return build();
