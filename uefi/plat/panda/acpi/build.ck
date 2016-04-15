/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    PandaBoard ACPI tables

Abstract:

    This module compiles the PandaBoard ACPI tables.

Author:

    Evan Green 26-Mar-2014

Environment:

    Firmware

--*/

function build() {
    sources = [
        "apic.asl",
        "dbg2.asl",
        "dsdt.asl",
        "facp.asl",
        "facs.asl",
        "omp4.asl"
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
