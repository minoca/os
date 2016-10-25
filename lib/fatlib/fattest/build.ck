/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    FAT Test

Abstract:

    This program tests the FAT file system library in an application.

Author:

    Evan Green 9-Oct-2012

Environment:

    Test

--*/

function build() {
    sources = [
        "fatdev.c",
        "fattest.c"
    ];

    build_libs = [
        "//lib/fatlib:build_fat",
        "//lib/rtl/rtlc:build_rtlc",
        "//lib/rtl/base:build_basertl"
    ];

    build_app = {
        "label": "build_fattest",
        "output": "fattest",
        "inputs": sources + build_libs,
        "build": TRUE,
        "prefix": "build"
    };

    entries = application(build_app);
    return entries;
}

return build();

