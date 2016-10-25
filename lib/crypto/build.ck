/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Crypto Library

Abstract:

    This library contains the Cryptographic Library functions used
    throughout Minoca OS.

Author:

    Evan Green 13-Jan-2015

Environment:

    Any

--*/

function build() {
    sources = [
        "aes.c",
        "fortuna.c",
        "hmac.c",
        "md5.c",
        "sha1.c",
        "sha256.c",
        "sha512.c"
    ];

    lib = {
        "label": "crypto",
        "inputs": sources,
    };

    build_lib = {
        "label": "build_crypto",
        "output": "crypto",
        "inputs": sources,
        "build": TRUE,
        "prefix": "build"
    };

    entries = static_library(lib);
    entries += static_library(build_lib);
    return entries;
}

return build();
