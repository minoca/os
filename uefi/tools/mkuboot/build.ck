/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mkuboot

Abstract:

    This module builds the mkuboot build utility, which can create a U-Boot
    firmware images.

Author:

    Chris Stevens 2-Jul-2015

Environment:

    Build

--*/

from menv import application;

function build() {
    var app;
    var entries;
    var includes;
    var mkubootCommand;
    var sources;
    var tool;

    sources = [
        "mkuboot.c",
        "../../core/crc32.c"
    ];

    includes = [
        "$S/uefi/include"
    ];

    app = {
        "label": "mkuboot",
        "inputs": sources,
        "includes": includes,
        "build": true
    };

    entries = application(app);

    //
    // Adjust crc32.o so it doesn't collide with the native version.
    //

    for (entry in entries) {
        if ((entry.get("output")) && (entry.output.endsWith("crc32.o"))) {
            entry.output = "crc32.o";
            break;
        }
    }

    mkubootCommand = "$O/uefi/tools/mkuboot/mkuboot $MKUBOOT_FLAGS " +
                     "-l $text_address -e $text_address -o $OUT $IN";

    tool = {
        "type": "tool",
        "name": "mkuboot",
        "command": mkubootCommand,
        "description": "Creating U-Boot Image - $OUT"
    };

    return entries + [tool];
}

