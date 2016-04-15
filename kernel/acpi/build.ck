/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    ACPI

Abstract:

    This library contains minimal support for ACPI in the kernel. It mostly
    marshals firmware tables from the boot environment and makes them
    available in the kernel environment.

Author:

    Evan Green 4-Aug-2012

Environment:

    Kernel

--*/

function build() {
    sources = [
        "tables.c"
    ];

    lib = {
        "label": "acpi",
        "inputs": sources,
    };

    entries = static_library(lib);
    return entries;
}

return build();
