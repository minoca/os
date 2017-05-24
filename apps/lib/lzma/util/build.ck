/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lzma (utility)

Abstract:

    This executable implements an interface to the LZMA library. It can
    compress and decompress Minoca LZMA images.

Author:

    Evan Green 23-May-2017

Environment:

    User

--*/

from menv import application, mconfig;

function build() {
    var app;
    var buildApp;
    var entries;
    var sources;

    sources = [
        "lzma.c"
    ];

    app = {
        "label": "lzma",
        "inputs": sources + ["apps/lib/lzma:liblzma"],
    };

    buildApp = {
        "label": "build_lzma",
        "output": "lzma",
        "inputs": sources + ["apps/lib/lzma:build_liblzma"],
        "build": true,
        "prefix": "build",
        "binplace": "tools/bin"
    };

    entries = application(app);
    entries += application(buildApp);
    return entries;
}

