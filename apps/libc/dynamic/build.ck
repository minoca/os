/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

from menv import mconfig, sharedLibrary, staticLibrary;

function build() {
    var arch = mconfig.arch;
    var arch_sources;
    var buildLib;
    var buildSources;
    var buildSourcesConfig;
    var dynlibs;
    var entries;
    var libs;
    var linkConfig;
    var linkLdflags;
    var mathSources;
    var so;
    var sources;
    var sourcesConfig;
    var sourcesIncludes;
    var wincsup;
    var wincsupIncludes;
    var wincsupSources;

    mathSources = [
        "math/abs.c",
        "math/ceil.c",
        "math/ceilf.c",
        "math/div.c",
        "math/exp.c",
        "math/expf.c",
        "math/exp2.c",
        "math/exp2f.c",
        "math/expm1.c",
        "math/expm1f.c",
        "math/floor.c",
        "math/floorf.c",
        "math/fmod.c",
        "math/fmodf.c",
        "math/hypot.c",
        "math/hypotf.c",
        "math/log.c",
        "math/logf.c",
        "math/log2.c",
        "math/log2f.c",
        "math/log10.c",
        "math/log10f.c",
        "math/lround.c",
        "math/lroundf.c",
        "math/minmax.c",
        "math/minmaxf.c",
        "math/modf.c",
        "math/modff.c",
        "math/pow.c",
        "math/powf.c",
        "math/rint.c",
        "math/rintf.c",
        "math/scalbn.c",
        "math/scalbnf.c",
        "math/sqrt.c",
        "math/sqrtf.c",
        "math/trig.c",
        "math/trigf.c",
        "math/trigarc.c",
        "math/trigarcf.c",
        "math/trighyp.c",
        "math/trighypf.c",
        "math/trunc.c",
        "math/truncf.c",
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
        "err.c",
        "errno.c",
        "exec.c",
        "exit.c",
        "fileio.c",
        "fnmatch.c",
        "gaddrinf.c",
        "getopt.c",
        "glob.c",
        "heap.c",
        "if.c",
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
        "scan.c",
        "scandir.c",
        "sched.c",
        "setjmp.c",
        "shadow.c",
        "signals.c",
        "socket.c",
        "spawn.c",
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
        "ucontext.c",
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

    buildSources = [
        "bsearch.c",
        "getopt.c",
        "qsort.c",
        "regexcmp.c",
        "regexexe.c"
    ];

    wincsupSources = [
        "regexcmp.c",
        "regexexe.c",
        "wincsup/strftime.c"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        arch_sources = [
            "armv7/contexta.S",
            "armv7/contextc.c",
            "armv7/fenva.S",
            "armv7/fenvc.c",
            "armv7/setjmpa.S",
            "armv7/tlsaddr.S"
        ];

    } else if (arch == "x86") {
        arch_sources = [
            "x86/contexta.S",
            "x86/contextc.c",
            "x86/fenv.S",
            "x86/fenvc.c",
            "x86/setjmpa.S",
            "x86/tlsaddr.S"
        ];

    } else if (arch == "x64") {
        arch_sources = [
            "x64/contexta.S",
            "x64/contextc.c",
            "x64/fenv.S",
            "x64/setjmpa.S",
            "x86/fenvc.c",
        ];
    }

    sourcesIncludes = [
        "$S/apps/libc/include"
    ];

    sourcesConfig = {
        "CFLAGS": ["-ftls-model=initial-exec"]
    };

    buildSourcesConfig = {
        "CFLAGS": ["-ftls-model=initial-exec", "-ffreestanding"]
    };

    linkLdflags = [
        "-nostdlib",
        "-Wl,--whole-archive"
    ];

    linkConfig = {
        "LDFLAGS": linkLdflags
    };

    if (arch == "x64") {
        libs = [];

    } else {
        libs = [
            "lib/rtl/base:intrins",
        ];
    }

    dynlibs = [
        "apps/osbase:libminocaos"
    ];

    wincsupIncludes = [
        "$S/apps/libc/dynamic/wincsup/include"
    ];

    so = {
        "label": "libc",
        "inputs": sources + mathSources + arch_sources + libs + dynlibs,
        "sources_config": sourcesConfig,
        "includes": sourcesIncludes,
        "config": linkConfig,
        "major_version": "1"
    };

    buildLib = {
        "label": "build_libc",
        "output": "build_libc",
        "inputs": buildSources + mathSources,
        "sources_config": buildSourcesConfig,
        "includes": sourcesIncludes,
        "build": true,
        "prefix": "build"
    };

    wincsup = {
        "label": "wincsup",
        "inputs": wincsupSources,
        "includes": wincsupIncludes,
        "build": true,
        "prefix": "wincsup"
    };

    entries = sharedLibrary(so);
    entries += staticLibrary(buildLib);
    entries += staticLibrary(wincsup);
    return entries;
}

