/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

from menv import staticApplication, flattenedBinary;

function build() {
    var entries;
    var flattened;
    var image;
    var linkConfig;
    var sources;

    sources = [
        "mbr.S"
    ];

    linkConfig = {
        "LDFLAGS": ["-Wl,-zmax-page-size=1"]
    };

    image = {
        "label": "mbr.elf",
        "inputs": sources,
        "config": linkConfig,
        "text_address": "0x600",
    };

    entries = staticApplication(image);

    //
    // Flatten the binary so it can be written directly to disk and loaded by
    // the BIOS.
    //

    flattened = {
        "label": "mbr.bin",
        "inputs": [":mbr.elf"],
        "binplace": "bin",
        "nostrip": true
    };

    flattened = flattenedBinary(flattened);
    entries += flattened;
    return entries;
}

