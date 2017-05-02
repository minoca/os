/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Intel High Definition Audio

Abstract:

    This module implements Intel High Definition Audio support.

Author:

    Chris Stevens 3-Apr-2017

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "intelhda";
    var sources;

    sources = [
        "codec.c",
        "hda.c",
        "hdahw.c",
    ];

    dynlibs = [
        "drivers/sound/core:sound"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

