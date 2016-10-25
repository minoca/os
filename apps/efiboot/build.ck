/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    efiboot

Abstract:

    This executable implements support for the efiboot program, which is a
    user mode utility that can be used to change EFI boot options.

Author:

    Evan Green 9-Dec-2014

Environment:

    User

--*/

function build() {
    sources = [
        "efiboot.c"
    ];

    dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    includes = [
        "$//apps/libc/include"
    ];

    sources_config = {
        "CFLAGS": ["-fshort-wchar"]
    };

    link_config = {};
    if ((arch == "armv6") || (arch == "armv7")) {
        link_config["LDFLAGS"] = ["-Wl,--no-wchar-size-warning"];
    }

    app = {
        "label": "efiboot",
        "inputs": sources + dynlibs,
        "sources_config": sources_config,
        "includes": includes,
        "config": link_config
    };

    entries = application(app);
    return entries;
}

return build();
