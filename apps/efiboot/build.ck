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

from menv import application, mconfig;

function build() {
    var app;
    var arch = mconfig.arch;
    var dynlibs;
    var entries;
    var includes;
    var linkConfig;
    var sources;
    var sourcesConfig;

    sources = [
        "efiboot.c"
    ];

    dynlibs = [
        "apps/osbase:libminocaos"
    ];

    includes = [
        "$S/apps/libc/include"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"]
    };

    linkConfig = {};
    if ((arch == "armv6") || (arch == "armv7")) {
        linkConfig["LDFLAGS"] = ["-Wl,--no-wchar-size-warning"];
    }

    app = {
        "label": "efiboot",
        "inputs": sources + dynlibs,
        "sources_config": sourcesConfig,
        "includes": includes,
        "config": linkConfig
    };

    entries = application(app);
    return entries;
}

