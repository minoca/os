/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    Master Boot Record

Abstract:

    This module implements the Master Boot Record that is installed at
    sector 0 of PC/AT disks.

Author:

    Evan Green 4-Feb-2014

Environment:

    Boot

--*/

function build() {
    sources = [
        "mbr.S"
    ];

    link_ldflags = [
        "-nostdlib",
        "-Wl,-zmax-page-size=1",
        "-static"
    ];

    link_config = {
        "LDFLAGS": ["$LDFLAGS"] + link_ldflags
    };

    image = {
        "label": "mbr.elf",
        "inputs": sources,
        "config": link_config,
        "text_address": "0x600",
    };

    entries = executable(image);

    //
    // Flatten the binary so it can be written directly to disk and loaded by
    // the BIOS.
    //

    flattened = {
        "label": "mbr.bin",
        "inputs": [":mbr.elf"]
    };

    flattened = flattened_binary(flattened);
    entries += flattened;
    return entries;
}

return build();
