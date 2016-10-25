/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Disassembler Test

Abstract:

    This program is used to test the debugger's disassembler.

Author:

    Evan Green 26-Jul-2012

Environment:

    Test

--*/

function build() {
    sources = [
        "testdisa.c",
        "//apps/debug/client:build/x86dis.o",
        "//apps/debug/client:build/armdis.o",
        "//apps/debug/client:build/disasm.o",
        "//apps/debug/client:build/thmdis.o",
        "//apps/debug/client:build/thm32dis.o",
    ];

    build_libs = [
        "//lib/im:build_im",
        "//lib/rtl/base:build_basertl",
        "//lib/rtl/rtlc:build_rtlc",
    ];

    build_app = {
        "label": "build_testdisa",
        "output": "testdisa",
        "inputs": sources + build_libs,
        "build": TRUE
    };

    entries = application(build_app);
    return entries;
}

return build();

