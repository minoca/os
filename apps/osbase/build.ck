/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Minoca OS Library

Abstract:

    This module implements the native system call interface between user
    mode applications and the kernel. It runs in user mode, and is utilized
    by the C library (or by applications directly) as an interface to the
    kernel. Applications are permitted to link against this library and
    call functions exported by it, but are not allowed to make system calls
    themselves (most of this library is simply a function call veneer over
    the system calls anyway). Applications utilizing this native library
    can get added functionality or performance, but at the cost of
    portability.

Author:

    Evan Green 25-Feb-2013

Environment:

    User

--*/

from menv import sharedLibrary, mconfig;

function build() {
    var arch = mconfig.arch;
    var archSources;
    var entries;
    var ldflags;
    var libs;
    var linkConfig;
    var so;
    var sources;
    var textAddress;

    sources = [
        "env.c",
        "heap.c",
        "osimag.c",
        "osbase.c",
        "rwlock.c",
        "socket.c",
        "spinlock.c",
        "time.c",
        "tls.c"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        textAddress = "0x10000000";
        archSources = [
            "armv7/features.c",
            "armv7/osbasea.S",
            "armv7/syscall.c"
        ];

    } else if (arch == "x86") {
        textAddress = "0x200000";
        archSources = [
            "x86/features.c",
            "x86/osbasea.S",
            "x86/syscall.c"
        ];

    } else if (arch == "x64") {
        textAddress = "0x200000";
        archSources = [
            "x64/osbasea.S",
            "x64/syscall.c"
        ];
    }

    ldflags = [
        "-Wl,-Bsymbolic",
        "-nostdlib",
        "-Wl,--whole-archive",
        "-Wl,-Ttext-segment=" + textAddress,
    ];

    linkConfig = {
        "LDFLAGS": ldflags
    };

    libs = [
        "lib/rtl/base:basertl",
        "lib/rtl/base:basertlw",
        "lib/rtl/urtl:urtl",
        "lib/im:imn",
        "lib/crypto:crypto"
    ];

    so = {
        "label": "libminocaos",
        "inputs": sources + archSources + libs,
        "entry": "OsDynamicLoaderMain",
        "config": linkConfig,
        "major_version": "1"
    };

    entries = sharedLibrary(so);
    return entries;
}

