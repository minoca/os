/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    BeagleBone Black ACPI tables

Abstract:

    This module compiles the BeagleBone Black ACPI tables.

Author:

    Evan Green 6-Jan-2015

Environment:

    Firmware

--*/

function build() {
    sources = [
        "am33.asl",
        "dbg2.asl",
        "dsdt.asl",
        "facp.asl",
        "facs.asl"
    ];

    asl = compiled_asl(sources);
    entries = asl[1];
    ffs_sources = asl[0];
    ffs = {
        "label": "acpi.ffs",
        "inputs": ffs_sources,
        "implicit": ["//uefi/tools/genffs:genffs"],
        "tool": "genffs_acpi"
    };

    entries += [ffs];
    return entries;
}

return build();
