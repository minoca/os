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

function build() {
    sources = [
        "genffs.c"
    ];

    includes = [
        "$//uefi/include"
    ];

    app = {
        "label": "genffs",
        "inputs": sources,
        "includes": includes,
        "build": TRUE
    };

    entries = application(app);
    genffs_command = "$^/uefi/tools/genffs/genffs -s -i $IN " +
                      "-r EFI_SECTION_PE32 -i $IN " +
                      "-r EFI_SECTION_USER_INTERFACE " +
                      "-t EFI_FV_FILETYPE_DRIVER -o $OUT";

    runtime_tool = {
        "type": "tool",
        "name": "genffs_runtime",
        "command": genffs_command,
        "description": "GenFFS - $IN"
    };

    genffs_command = "$^/uefi/tools/genffs/genffs " +
                     "-g 7E374E25-8E01-4FEE-87F2-390C23C606CD " +
                     "-r EFI_SECTION_RAW -t EFI_FV_FILETYPE_FREEFORM " +
                     "-o $OUT $IN";

    acpi_tool = {
        "type": "tool",
        "name": "genffs_acpi",
        "command": genffs_command,
        "description": "GenFFS - $OUT"
    };

    return entries + [runtime_tool, acpi_tool];
}

return build();
