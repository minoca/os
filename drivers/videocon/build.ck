/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    Video Console

Abstract:

    This module implements a basic console on a framebuffer.

Author:

    Evan Green 15-Feb-2013

Environment:

    Kernel

--*/

function build() {
    name = "videocon";
    sources = [
        "videocon.c"
    ];

    libs = [
        "//lib/basevid:basevid",
        "//lib/termlib:termlib"
    ];

    drv = {
        "label": name,
        "inputs": sources + libs,
    };

    entries = driver(drv);
    return entries;
}

return build();
