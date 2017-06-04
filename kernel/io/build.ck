/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Input/Output

Abstract:

    This library contains the I/O (Input/Output) functionality of the
    kernel. The I/O library manages devices, connects them to drivers, and
    coordinates exposing their functionality to other kernel components and
    user mode.

Author:

    Evan Green 16-Sep-2012

Environment:

    Kernel

--*/

from menv import kernelLibrary, mconfig;

function build() {
    var arch = mconfig.arch;
    var archSources;
    var baseSources;
    var entries;
    var lib;

    baseSources = [
        "arb.c",
        "cachedio.c",
        "cstate.c",
        "device.c",
        "devinfo.c",
        "devrem.c",
        "devres.c",
        "driver.c",
        "fileobj.c",
        "filesys.c",
        "flock.c",
        "info.c",
        "init.c",
        "intrface.c",
        "intrupt.c",
        "iobase.c",
        "iohandle.c",
        "irp.c",
        "mount.c",
        "obfs.c",
        "pagecach.c",
        "path.c",
        "perm.c",
        "pipe.c",
        "pminfo.c",
        "power.c",
        "pstate.c",
        "pty.c",
        "pwropt.c",
        "shmemobj.c",
        "socket.c",
        "stream.c",
        "testhook.c",
        "unsocket.c",
        "userio.c"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        archSources = [
            "armv7/archio.c",
            "armv7/archpm.c"
        ];

    } else if ((arch == "x86") || (arch == "x64")) {
        archSources = [
            "x86/archio.c",
            "x86/archpm.c",
            "x86/intelcst.c"
        ];
    }

    lib = {
        "label": "io",
        "inputs": baseSources + archSources,
    };

    entries = kernelLibrary(lib);
    return entries;
}

