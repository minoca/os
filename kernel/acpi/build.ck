/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

from menv import kernelLibrary;

function build() {
    var entries;
    var lib;
    var sources;

    sources = [
        "tables.c"
    ];

    lib = {
        "label": "acpi",
        "inputs": sources,
    };

    entries = kernelLibrary(lib);
    return entries;
}

