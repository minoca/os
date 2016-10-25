/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Object Manager

Abstract:

    This library contains the Object Manager. It maintains a global
    file-system like hierarchy of objects used by many other kernel
    components including Mm, Io, Ke and Ps.

Author:

    Evan Green 4-Sep-2012

Environment:

    Kernel

--*/

function build() {
    sources = [
        "handles.c",
        "obapi.c"
    ];

    lib = {
        "label": "ob",
        "inputs": sources,
    };

    entries = static_library(lib);
    return entries;
}

return build();
