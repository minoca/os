/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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

function build() {
    name = "fat";
    sources = [
        "fatfs.c",
        "fatio.c"
    ];

    libs = [
        "//lib/fatlib:fat"
    ];

    drv = {
        "label": name,
        "inputs": sources + libs,
    };

    entries = driver(drv);
    return entries;
}

return build();
