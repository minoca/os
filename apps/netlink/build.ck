/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    Minoca NetLink Library

Abstract:

    This module contains the netlink sockets library, which it built on top
    of the C library socket's interface and offers a set of APIs to make
    use of netlink sockets easier.

Author:

    Chris Stevens 24-Mar-2016

Environment:

    User

--*/

function build() {
    sources = [
        "generic.c",
        "netlink.c",
    ];

    sources_cppflags = [
        "$CPPFLAGS",
        "-I$///apps/include",
        "-I$///apps/include/libc"
    ];

    sources_config = {
        "CPPFLAGS": sources_cppflags
    };

    lib_config = {
        "LDFLAGS": ["$LDFLAGS", "-nostdlib"]
    };

    lib = {
        "label": "libnetlink",
        "inputs": sources,
        "entry": "NlInitialize",
        "sources_config": sources_config,
        "config": lib_config,
        "major_version": "1",
    };

    entries = shared_library(lib);
    return entries;
}

return build();

