/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    SSL Library

Abstract:

    This module contains a limited SSL support library.

Author:

    Evan Green 22-Jul-2015

Environment:

    Any

--*/

function build() {
    sources = [
        "asn1.c",
        "base64.c",
        "bigint.c",
        "loader.c",
        "rsa.c"
    ];

    lib = {
        "label": "ssl",
        "inputs": sources,
    };

    build_lib = {
        "label": "build_ssl",
        "output": "ssl",
        "inputs": sources,
        "build": TRUE,
        "prefix": "build"
    };

    entries = static_library(lib);
    entries += static_library(build_lib);
    return entries;
}

return build();
