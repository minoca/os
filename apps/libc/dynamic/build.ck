/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    C Library

Abstract:

    This module contains the dynamic library portion of the C library,
    which is nearly everything. Implementing the bulk of the functionality
    in a dynamic library reduces wasted space on disk and in memory across
    processes by reducing duplicated code. It also allows the C library to
    be updated without recompiling all applications.

Author:

    Evan Green 4-Mar-2013

Environment:

    User Mode C Library

--*/

function build() {
    math_sources = [
        "math/abs.c",
        "math/ceil.c",
        "math/div.c",
        "math/exp.c",
        "math/floor.c",
        "math/fmod.c",
        "math/hypot.c",
        "math/log.c",
        "math/log2.c",
        "math/log10.c",
        "math/modf.c",
        "math/pow.c",
        "math/scalbn.c",
        "math/sqrt.c",
        "math/trig.c",
        "math/trigarc.c",
        "math/trighyp.c",
        "math/util.c",
    ];

    sources = [
        "assert.c",
        "brk.c",
        "bsearch.c",
        "convert.c",
        "ctype.c",
        "dirio.c",
        "dynlib.c",
        "env.c",
        "errno.c",
        "exec.c",
        "exit.c",
        "fileio.c",
        "fnmatch.c",
        "gaddrinf.c",
        "getopt.c",
        "glob.c",
        "heap.c",
        "inet.c",
        "init.c",
        "kerror.c",
        "langinfo.c",
        "line.c",
        "locale.c",
        "memory.c",
        "netaddr.c",
        "netent.c",
        "passwd.c",
        "path.c",
        "pid.c",
        "pthread/atfork.c",
        "pthread/barrier.c",
        "pthread/cond.c",
        "pthread/key.c",
        "pthread/mutex.c",
        "pthread/once.c",
        "pthread/pthread.c",
        "pthread/rwlock.c",
        "pthread/sema.c",
        "pthread/setids.c",
        "pthread/thrattr.c",
        "pty.c",
        "qsort.c",
        "rand.c",
        "random.c",
        "realpath.c",
        "regexcmp.c",
        "regexexe.c",
        "resolv.c",
        "resource.c",
        "setjmp.c",
        "scan.c",
        "shadow.c",
        "signals.c",
        "socket.c",
        "stat.c",
        "statvfs.c",
        "stream.c",
        "streamex.c",
        "string.c",
        "sysconf.c",
        "syslog.c",
        "system.c",
        "termios.c",
        "time.c",
        "times.c",
        "tmpfile.c",
        "uio.c",
        "uname.c",
        "usershel.c",
        "utmpx.c",
        "wchar.c",
        "wctype.c",
        "wscan.c",
        "wstream.c",
        "wstring.c",
    ];

    build_sources = [
        "bsearch.c",
        "getopt.c",
        "qsort.c",
        "regexcmp.c",
        "regexexe.c"
    ];

    wincsup_sources = [
        "regexcmp.c",
        "regexexe.c",
        "wincsup/strftime.c"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        arch_sources = [
            "armv7/fenva.S",
            "armv7/fenvc.c",
            "armv7/setjmpa.S",
            "armv7/tlsaddr.S"
        ];

    } else if (arch == "x86") {
        arch_sources = [
            "x86/fenv.S",
            "x86/fenvc.c",
            "x86/setjmpa.S",
            "x86/tlsaddr.S"
        ];

    } else if (arch == "x64") {
        arch_sources = [
            "x64/fenv.S",
            "x64/fenvc.c",
            "x64/setjmpa.S",
        ];
    }

    sources_includes = [
        "$//apps/include",
        "$//apps/include/libc"
    ];

    sources_config = {
        "CFLAGS": ["-ftls-model=initial-exec"]
    };

    link_ldflags = [
        "-nostdlib",
        "-Wl,--whole-archive"
    ];

    link_config = {
        "LDFLAGS": link_ldflags
    };

    libs = [
        "//lib/rtl/base:intrins",
    ];

    dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    wincsup_includes = [
        "$//apps/include",
        "$//apps/libc/dynamic/wincsup/include"
    ];

    so = {
        "label": "libc",
        "inputs": sources + math_sources + arch_sources + libs + dynlibs,
        "sources_config": sources_config,
        "includes": sources_includes,
        "entry": "ClInitialize",
        "config": link_config,
        "major_version": "1"
    };

    build_lib = {
        "label": "build_libc",
        "output": "build_libc",
        "inputs": build_sources + math_sources,
        "sources_config": sources_config,
        "includes": sources_includes,
        "build": TRUE,
        "prefix": "build"
    };

    wincsup = {
        "label": "wincsup",
        "inputs": wincsup_sources,
        "includes": wincsup_includes,
        "build": TRUE,
        "prefix": "wincsup"
    };

    entries = shared_library(so);
    entries += static_library(build_lib);
    entries += static_library(wincsup);
    return entries;
}

return build();

