/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ElfConv

Abstract:

    This module builds the ElfConv utility, which converts an ELF image
    into a PE image. UEFI exclusively loads PE images, which is why the
    conversion is necessary. With certain restrictions (such as the lack
    of dynamic libraries) conversion is doable, though not a lot of fun.

Author:

    Evan Green 10-Mar-2014

Environment:

    Build

--*/

from menv import application;

function build() {
    var app;
    var entries;
    var includes;
    var sources;
    var tool;

    sources = [
        "elfc32.c",
        "elfconv.c",
    ];

    includes = [
        "$S/uefi/include"
    ];

    app = {
        "label": "elfconv",
        "inputs": sources,
        "includes": includes,
        "build": true
    };

    entries = application(app);
    tool = {
        "type": "tool",
        "name": "elfconv",
        "command": "$O/uefi/tools/elfconv/elfconv $ELFCONV_FLAGS -o $OUT $IN",
        "description": "Converting to PE - $IN"
    };

    return entries + [tool];
}

