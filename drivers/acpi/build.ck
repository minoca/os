/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

from menv import driver, mconfig;

function build() {
    var arch = mconfig.arch;
    var drv;
    var entries;
    var sources;

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
        "resdesc.c",
        "reset.c"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        sources += [
            "armv7/procarch.c"
        ];

    } else if ((arch == "x86") || (arch == "x64")) {
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

