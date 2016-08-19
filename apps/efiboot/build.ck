/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

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
