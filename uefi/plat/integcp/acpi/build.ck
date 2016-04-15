/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    Integrator/CP ACPI Tables

Abstract:

    This module compiles the Integrator/CP ACPI tables.

Author:

    Evan Green 7-Apr-2014

Environment:

    Firmware

--*/

function build() {
    sources = [
        "dbg2.asl",
        "dsdt.asl",
        "facp.asl",
        "facs.asl",
        "incp.asl"
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
