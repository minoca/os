/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

    includes = [
        "$//drivers/usb/ehci"
    ];

    lib = {
        "label": "kdusb",
        "inputs": sources,
        "includes": includes
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
