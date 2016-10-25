/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    GenFV

Abstract:

    This module builds the GenFV build utility, which can create a FFS2
    Firmware Volume out of one or more FFS files.

Author:

    Evan Green 7-Mar-2014

Environment:

    Build

--*/

function build() {
    sources = [
        "genfv.c",
    ];

    includes = [
        "$//uefi/include"
    ];

    app = {
        "label": "genfv",
        "inputs": sources,
        "includes": includes,
        "build": TRUE
    };

    entries = application(app);
    tool = {
        "type": "tool",
        "name": "genfv",
        "command": "$^//uefi/tools/genfv/genfv $GENFV_FLAGS -o $OUT $IN",
        "description": "GenFV - $OUT"
    };

    return entries + [tool];
}

return build();
