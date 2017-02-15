/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    GenFFS

Abstract:

    This module builds the GenFFS build utility, which can create a single
    FFS file.

Author:

    Evan Green 6-Mar-2014

Environment:

    Build

--*/

from menv import application;

function build() {
    var acpiTool;
    var app;
    var entries;
    var genffsCommand;
    var includes;
    var sources;
    var runtimeTool;

    sources = [
        "genffs.c"
    ];

    includes = [
        "$S/uefi/include"
    ];

    app = {
        "label": "genffs",
        "inputs": sources,
        "includes": includes,
        "build": true
    };

    entries = application(app);
    genffsCommand = "$O/uefi/tools/genffs/genffs -s -i $IN " +
                    "-r EFI_SECTION_PE32 -i $IN " +
                    "-r EFI_SECTION_USER_INTERFACE " +
                    "-t EFI_FV_FILETYPE_DRIVER -o $OUT";

    runtimeTool = {
        "type": "tool",
        "name": "genffs_runtime",
        "command": genffsCommand,
        "description": "GenFFS - $IN"
    };

    genffsCommand = "$O/uefi/tools/genffs/genffs " +
                    "-g 7E374E25-8E01-4FEE-87F2-390C23C606CD " +
                    "-r EFI_SECTION_RAW -t EFI_FV_FILETYPE_FREEFORM " +
                    "-o $OUT $IN";

    acpiTool = {
        "type": "tool",
        "name": "genffs_acpi",
        "command": genffsCommand,
        "description": "GenFFS - $OUT"
    };

    return entries + [runtimeTool, acpiTool];
}

