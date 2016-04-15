/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    ACPI

Abstract:

    This module implements the Advanced Configuration and Power Interface
    (ACPI) driver, which uses platform firmware to enumerate devices,
    manage hardware, and implement system power transitions. It is
    supported even on systems that do not provide ACPI-specific hardware
    (embedded controllers).

Author:

    Evan Green 29-Nov-2012

Environment:

    Kernel

--*/

function build() {
    sources = [
        "acpidrv.c",
        "aml.c",
        "amlopcr.c",
        "amlopex.c",
        "amloptab.c",
        "amlos.c",
        "drvsup.c",
        "earlypci.c",
        "fixedreg.c",
        "namespce.c",
        "oprgn.c",
        "oprgnos.c",
        "proc.c",
        "resdesc.c"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        sources += [
            "armv7/procarch.c"
        ];

    } else if (arch == "x86") {
        sources += [
            "x86/procarch.c"
        ];
    }

    drv = {
        "label": "acpi",
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();
