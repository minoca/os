/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    FAT

Abstract:

    This module implements the File Allocation Table file system driver.
    It supports FAT12, FAT16, and FAT32 with some long file name support.

Author:

    Evan Green 25-Sep-2012

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var entries;
    var libs;
    var name = "fat";
    var sources;

    sources = [
        "fatfs.c",
        "fatio.c"
    ];

    libs = [
        "lib/fatlib:fat"
    ];

    drv = {
        "label": name,
        "inputs": sources + libs,
    };

    entries = driver(drv);
    return entries;
}

