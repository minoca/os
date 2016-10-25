/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Raspberry Pi Binary Blobs

Abstract:

    This directory contains the binary blobs needed to boot the Raspberry
    Pi, Raspberry Pi 2, and Raspberry Pi 3 (in 32-bit mode).

Author:

    Chris Stevens 26-Oct-2015

Environment:

    Firmware

--*/

function build() {
    plat = "rpi2";
    blobs = [
        "bootcode.bin",
        "config.txt",
        "fixup.dat",
        "LICENCE.broadcom",
        "start.elf"
    ];

    rpibin = binroot + "/rpi/";
    blob_targets = [];
    entries = [];
    for (blob in blobs) {
        entries += copy(blob,
                        rpibin + blob,
                        blob,
                        null,
                        null);

        blob_targets += [":" + blob];
    }

    entries += group("blobs", blob_targets);
    return entries;
}

return build();
