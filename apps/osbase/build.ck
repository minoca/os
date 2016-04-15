/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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

function build() {
    sources = [
        "env.c",
        "heap.c",
        "osimag.c",
        "osbase.c",
        "socket.c",
        "spinlock.c",
        "time.c",
        "tls.c"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        text_base = "0x10000000";
        arch_sources = [
            "armv7/features.c",
            "armv7/osbasea.S",
            "armv7/syscall.c"
        ];

    } else if (arch == "x86") {
        text_base = "0x200000";
        arch_sources = [
            "x86/features.c",
            "x86/osbasea.S",
            "x86/syscall.c"
        ];

    } else if (arch == "x64") {
        text_base = "0x200000";
        arch_sources = [
            "x64/osbasea.S",
            "x64/syscall.c"
        ];
    }

    sources_config = {
        "CPPFLAGS": ["$CPPFLAGS", "-I$///apps/include"],
    };

    link_ldflags = [
        "$LDFLAGS",
        "-Wl,-Bsymbolic",
        "-nostdlib",
        "-Wl,--whole-archive",
        "-Wl,-Ttext-segment=" + text_base,
    ];

    link_config = {
        "LDFLAGS": link_ldflags
    };

    libs = [
        "//lib/rtl/base:basertl",
        "//lib/rtl/base:basertlw",
        "//lib/im:im",
        "//apps/osbase/urtl:urtl",
        "//lib/crypto:crypto"
    ];

    so = {
        "label": "libminocaos",
        "inputs": sources + arch_sources + libs,
        "sources_config": sources_config,
        "entry": "OsDynamicLoaderMain",
        "config": link_config,
        "major_version": "1"
    };

    entries = shared_library(so);
    return entries;
}

return build();

