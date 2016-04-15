/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    KD USB

Abstract:

    This library contains the USB kernel debugger support library.

Author:

    Evan Green 18-Apr-2014

Environment:

    Kernel, Boot

--*/

function build() {
    sources = [
        "ftdi.c",
        "hub.c",
        "kdehci.c",
        "kdusb.c"
    ];

    stub_sources = [
        "kdnousb/stubs.c"
    ];

    sources_config = {
        "CPPFLAGS": ["$CPPFLAGS", "-I$///drivers/usb/ehci"]
    };

    lib = {
        "label": "kdusb",
        "inputs": sources,
        "sources_config": sources_config
    };

    stub_lib = {
        "label": "kdnousb",
        "inputs": stub_sources
    };

    entries = static_library(lib);
    entries += static_library(stub_lib);
    return entries;
}

return build();
