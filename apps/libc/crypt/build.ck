/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Crypt Library

Abstract:

    This module contains the crypt library, which contains the C library
    functions like crypt, encrypt, fcrypt, and setkey.

Author:

    Evan Green 6-Mar-2015

Environment:

    User

--*/

from menv import sharedLibrary;

function build() {
    var dynlibs;
    var entries;
    var so;
    var sources;

    sources = [
        "crypt.c"
    ];

    dynlibs = [
        "apps/libc/dynamic:libc"
    ];

    so = {
        "label": "libcrypt",
        "inputs": sources + dynlibs,
        "major_version": "1"
    };

    entries = sharedLibrary(so);
    return entries;
}

