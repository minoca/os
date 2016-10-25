/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    FATBoot

Abstract:

    This module implements the boot code between the MBR and the loader on
    BIOS systems. It knows just enough to load the Boot Manager. It runs
    in a very gray environment, is undebuggable, and should be considered
    sacred code.

Author:

    Evan Green 14-Oct-2013

Environment:

    Boot

--*/

function build() {
    sources = [
        "vbr.S",
        "fatboot.c",
        "prochw.c",
        "//boot/lib:x86/archsup.o",
        "//boot/lib:pcat/realmode.o",
        "//boot/lib:pcat/realmexe.o"
    ];

    includes = [
        "$//boot/lib/include",
        "$//boot/lib/pcat"
    ];

    link_ldflags = [
        "-nostdlib",
        "-Wl,-zmax-page-size=1",
        "-static"
    ];

    link_config = {
        "LDFLAGS": link_ldflags
    };

    image = {
        "label": "fatboot.elf",
        "inputs": sources,
        "includes": includes,
        "config": link_config,
        "text_address": "0x7C00",
    };

    entries = executable(image);

    //
    // Flatten the binary so it can be written directly to disk and loaded by
    // the MBR.
    //

    flattened = {
        "label": "fatboot.bin",
        "inputs": [":fatboot.elf"],
        "binplace": TRUE,
        "nostrip": TRUE
    };

    flattened = flattened_binary(flattened);
    entries += flattened;
    return entries;
}

return build();
