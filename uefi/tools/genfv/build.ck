/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

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
