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

from menv import mconfig, kernelLibrary;

function build() {
    var arch = mconfig.arch;
    var entries;
    var includes;
    var lib;
    var sources;
    var stubLib;
    var stubLib32;
    var stubSources;

    sources = [
        "ftdi.c",
        "hub.c",
        "kdehci.c",
        "kdusb.c"
    ];

    stubSources = [
        "kdnousb/stubs.c"
    ];

    includes = [
        "$S/drivers/usb/ehci"
    ];

    lib = {
        "label": "kdusb",
        "inputs": sources,
        "includes": includes
    };

    stubLib = {
        "label": "kdnousb",
        "inputs": stubSources
    };

    entries = kernelLibrary(lib);
    entries += kernelLibrary(stubLib);
    if (arch == "x64") {
        stubLib32 = {
            "label": "kdnousb32",
            "inputs": sources,
            "includes": includes,
            "prefix": "x6432",
            "sources_config": {"CPPFLAGS": ["-m32"]}
        };

        entries += kernelLibrary(stubLib32);
    }

    return entries;
}

